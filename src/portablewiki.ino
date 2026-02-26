//
//  This sketch uses a LILYGO T-Dongle S3 to serve Kiwix content from an SD Card.
//
//  License:  MIT. See the LICENSE file in the project root for more details.
//
#include <FS.h>
#include <WiFi.h>
#include <SD_MMC.h>
#include <ESPmDNS.h>
#include <sqlite3.h>
#include <FastLED.h>
#include <WebServer.h>

//  Pin assignments
#define LED_DI_PIN     40
#define LED_CI_PIN     39

#define TFT_CS_PIN     4
#define TFT_SDA_PIN    3
#define TFT_SCL_PIN    5
#define TFT_DC_PIN     2
#define TFT_RES_PIN    1
#define TFT_LEDA_PIN   38

#define SD_MMC_D0_PIN  14
#define SD_MMC_D1_PIN  17
#define SD_MMC_D2_PIN  21
#define SD_MMC_D3_PIN  18
#define SD_MMC_CLK_PIN 12
#define SD_MMC_CMD_PIN 16

//  Debugging
#define DEBUGGING 0
#if (DEBUGGING)
#define DEBUG_ONLY(...)     __VA_ARGS__
#define DEBUG_OUT(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else   // DEBUGGING
#define DEBUG_ONLY(...)
#define DEBUG_OUT(...)
#endif  // DEBUGGING

//  Logging
#define LOGGING   1
#if (LOGGING)
#define LOG_OUT(fmt, ...) Serial.printf(fmt, ##__VA_ARGS__)
#else   // DEBUGGING
#define LOG_OUT(...)
#endif  // DEBUGGING

//  Enable LCD
#define LCD_ON  1
#if (LCD_ON)
#define LCD_OUT(fmt, ...) tft.printf(fmt, ##__VA_ARGS__)
#else   // LCD_ON
#define LCD_OUT(fmt, ...)
#endif  // LCD_ON

#define DEBUG_OR_LOG  (DEBUGGING || LOGGING)

//  Type of file system
#define SDCARD        SD_MMC

//  Asset database reference
sqlite3 *assetDB = nullptr;

//  LED interface
CRGB leds;

#if (LCD_ON)
#include <TFT_eSPI.h>

//  LCD interface
TFT_eSPI tft = TFT_eSPI();
#endif  // LCD_ON

//  AP Information
const char *ssid      = "portablewiki";
const char *password  = "topsecret";
const char *hostname  = "portablewiki";

//  File read buffer size and error detection
//
//  Note: The block size is based on the WiFi MTU and is small enough
//        to be used for a local buffer. If you try to use more than 4KB
//        as a local buffer in a function, the device will trigger some 
//        protection mechanism and reboot. 
//
constexpr size_t blockSize  = 1450 * 2;
constexpr size_t badRead    = (size_t) -1;

//  Content type string length
constexpr size_t ctyStrLen  = 32;

//  Local web server
WebServer server(80);

#if (LCD_ON)
//  Assets served
uint64_t assetsServed   = 0;
uint64_t assetsReported = 0;
#endif  // LCD_ON

//  Output an error and then loop forever. This is typically
//  called during setup() when an error occurs and there is
//  no point continuing execution
void errorForever (const char *msg)
{
  leds = CRGB::Red;
  FastLED.show();

  DEBUG_OUT("%s", msg);
  while (true)
  {
    //  Can't start with this error
    delay(1000);
  }
}

//  Open a database
int openDB (sqlite3 **db, const char *dbfile)
{
   int rc = sqlite3_open(dbfile, db);
   if (rc) 
   {
       DEBUG_OUT("Can't open database: %s\n", sqlite3_errmsg(*db));
   } 
   else 
   {
       DEBUG_OUT("Opened database successfully\n");
   }

   return rc;
}

//  Read a block of data from the binary asset file
inline size_t GetBlock (File binAsset, char *buffer, const size_t size)
{
  auto bytes  = min(size, blockSize);
  auto read   = binAsset.read((uint8_t *) buffer, bytes);
  return read;
}

//  Determine what to do with the incoming URL
bool Director (String inURL)
{
  DEBUG_OUT("Got : %s\n", inURL.c_str());

  //  Looking for the root?
  if (inURL == "/")
  {
    if (SDCARD.exists("/MAIN.TXT"))
    {
      //  Specific URL for the main page exists
      File    main = SDCARD.open("/MAIN.TXT", FILE_READ);
      String  line = main.readStringUntil('\n');

      //  Can close the file
      main.close();

      //  Create a new URL
      inURL = "/" + line;
    }
    else
    {
        //  Good starting point
        inURL += "index.html";
    }
  }

  DEBUG_OUT("In URL now %s\n", inURL.c_str());

  //  Convert %XX formatting to real characters
  String path = server.urlDecode(inURL);

  //  Sanitize the path according to the following rules (follows the convention in the
  //  zim2asset.py script):
  //
  //  Character   Becomes
  //  ,           x
  //  "           y
  //  '           z
  //
  //   This prevents the input path containing characters that would derail a query
  //
  path.replace("%2C", "x");
  path.replace(",", "x");
  path.replace("\"", "y");
  path.replace("'", "z");

  DEBUG_OUT("Now : %s\n", path.c_str());

  //  Build a query to find the actual file. The SQLite database
  //  contains the original file path (URL), an offset into the
  //  binary asset file, a content size and whether the content
  //  was compressed
  const char *tail;
  sqlite3_stmt *res;
  String sql = "select frag, gzip, cty, start, size from assets where origname='" + path + "'";

  DEBUG_OUT("Query: %s\n", sql.c_str());

  int rc = sqlite3_prepare_v2(assetDB, sql.c_str(), -1, &res, &tail);
  if (rc != SQLITE_OK) 
  {
    DEBUG_OUT("SQL error %s\n", sqlite3_errmsg(assetDB));
    return false;
  }

  if (sqlite3_step(res) != SQLITE_ROW)
  {
    sqlite3_finalize(res);
    LOG_OUT("URL not found %s\n", path.c_str());
    return false;
  }

  //  Located the URL - get the file locatation
  //
  //  Note: We must grab this information before closing the
  //        SQL statement, otherwise the information will be
  //        corrupted
  //
  char      assetType [ctyStrLen + 1];
  int       assetFrag   = sqlite3_column_int(res, 0);
  int       assetGZ     = sqlite3_column_int(res, 1);
  char      *tmpType    = (char *) sqlite3_column_text(res, 2);
  uint32_t  assetStart  = sqlite3_column_int(res, 3);
  size_t    assetSize   = sqlite3_column_int(res, 4);

  //  The majority of content in the database has a type, but
  //  we make sure we set something if the type is missing for
  //  some reason
  if (tmpType == nullptr)
  {
    strcpy(assetType, "text/plain");
  }
  else
  {
    if (strlen(tmpType) < sizeof(assetType) - 1)
    {
      strcpy(assetType, tmpType);
    }
    else
    {
      strcpy(assetType, "text/plain");
    }
  } 

  //  Done with the SQL statement
  sqlite3_finalize(res);

  DEBUG_OUT("Got SQL data frag = %ld, gzip file = %ld, type = %s, start = %lu, size = %ld\n", 
    assetFrag, assetGZ, assetType, assetStart, assetSize);

  //  Open a new asset fragment
  char assetName [16];
  snprintf(assetName, sizeof(assetName) - 1, "/ASSET-%02d.BIN", assetFrag);
  if (!SDCARD.exists(assetName))
  {
    LOG_OUT("Cannot locate %s\n", assetName);
    return false;
  }

  File binAsset = SDCARD.open(assetName, FILE_READ);
  if (!binAsset)
  {
    LOG_OUT("Cannot open %s\n", assetName);
    return false;
  }

  //  Compressed output
  if (assetGZ != 0)
  {
    DEBUG_OUT("Setting compressed data\n");
    server.sendHeader("Content-Encoding", "gzip");
  }

  //  We are sending chunked output
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.chunkResponseBegin(assetType);
  
  //  Set up for the first chunk read
  auto remaining = assetSize;
  binAsset.seek(assetStart, SeekSet);

  //  Process all chunks
  char assetBuf [blockSize];
  while (remaining != 0)
  {
    //  Read a block
    auto bytesRead = GetBlock(binAsset, assetBuf, remaining);
    if (bytesRead == badRead)
    {
      LOG_OUT("Asset block failed to read\n");
      return false;
    }

    //  Send the chunk
    server.sendContent(assetBuf, bytesRead);

    //  Adjust remaining
    remaining -= bytesRead;
  }

#if (LCD_ON)
  //  All chunks sent
  assetsServed ++;
#endif  // LCD_ON

  server.chunkResponseEnd();

  DEBUG_OUT("Content sent\n");

  return true;
}
  
void setup (void) 
{
#if (DEBUG_OR_LOG)
  Serial.begin(19200);
  delay(250);
#endif  // DEBUG_OR_LOG

  DEBUG_OUT("Starting...\n");

  //  Initialize LED
  FastLED.addLeds<APA102, LED_DI_PIN, LED_CI_PIN, BGR>(&leds, 1);
  FastLED.setBrightness(25);

  //  Initialize the SDCard, which must be done before the LCD
  SDCARD.setPins(SD_MMC_CLK_PIN, SD_MMC_CMD_PIN, SD_MMC_D0_PIN, SD_MMC_D1_PIN, SD_MMC_D2_PIN, SD_MMC_D3_PIN);

  //  The default begin() call only allows 5 open files. We need 20 or so, so we have to call
  //  a different begin() method to pass the number of open files needed
  if (SDCARD.begin("/sdcard", false, false, BOARD_MAX_SDMMC_FREQ, 20)) 
  {
    leds = CRGB::Green;
    FastLED.show();
  }
  else
  {
    //  Something is wrong
    errorForever("SDCard failed\n");
  }

#if (LCD_ON)
  //  Initialize the LCD
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextFont(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  tft.setCursor(0, 0);
  LCD_OUT("Starting...\n");
#endif  // LCD_ON

  if (!SDCARD.exists("/ASSET.DB"))
  {
    errorForever("Did not find database");
  }

  //  Initialize SQLite
  sqlite3_initialize();

  //  Open the asset database
  if (openDB(&assetDB, "/sdcard/ASSET.DB"))
  {
    errorForever("Cannot open assets\n");
  }

  LCD_OUT("Database opened\n");

  //  Start the AP
  WiFi.softAP(ssid, password);
  WiFi.setSleep(false);

  IPAddress myIP = WiFi.softAPIP();
  LCD_OUT("AP SSID %s\n", ssid);

  //  Allow hostname.local access
  MDNS.begin(hostname);
  
  //  Send all requests to the director() function
  server.onNotFound([]()
  {
    if (!Director(server.uri()))
    {
      server.send(404, "text/plain", "Did not find " + server.uri());
    }
  });

  //  Start the web server
  server.begin();

  LCD_OUT("Web Server IP: %s\n", myIP.toString());
  LCD_OUT("Host %s ready!\n", hostname);
}

void loop (void)
{
  server.handleClient();

#if (LCD_ON)
  if (assetsServed != assetsReported)
  {
    auto resetX = tft.getCursorX();
    auto resetY = tft.getCursorY();
    LCD_OUT("Served: %lld", assetsServed);
    assetsReported = assetsServed;
    tft.setCursor(resetX, resetY);
  }
#endif  // LCD_ON
}
