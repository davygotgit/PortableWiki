//
//  This Sketch uses a LILYGO T-Dongle S3 to serve Kiwix content from an SD Card, and
//  an asynchronous version of portablewiki.ino.
//
//  However, this Sketch is slower than portablewiki.ino and adds complexity. The Sketch
//  has been included for completeness, but is not expected to be used.
//
//  License:  MIT. See the LICENSE file in the project root for more details.
//
#include <FS.h>
#include <WiFi.h>
#include <SD_MMC.h>
#include <ESPmDNS.h>
#include <sqlite3.h>
#include <FastLED.h>
#include <ESPAsyncWebServer.h>

#include <mutex>
#include <atomic>

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

//  File read error detection
constexpr size_t badRead    = (size_t) -1;

//  Content type string length
constexpr size_t ctyStrLen  = 32;

//  Local web server
AsyncWebServer server(80);

#if (LCD_ON)
//  Assets served
uint64_t assetsServed   = 0;
uint64_t assetsReported = 0;
#endif  // LCD_ON

//  Number of active connections
//
//  Note: If you set the maximum number of requests too high (16+) the
//        chances of running out of file handles and memory goes up
//        significantly
//
constexpr size_t    maxRequests    = 8;
std::atomic<size_t> pendingRequests (0);

//  Mutex to serialize access to SQLite
std::mutex SQLLock;

//  Request state codes
enum class RQState : uint8_t {RQOK, RQError, RQRetry};

//  Prepended to redirected URL
constexpr char *redirectStr = "/67is61";

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
   auto rc = sqlite3_open(dbfile, db);
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

//  Run a SQL query
RQState RunQuery (String path, int &assetFrag, int &assetGZ, char *assetType, uint32_t &assetStart, size_t &assetSize)
{
  //  Build a query to find the actual file. The SQLite database
  //  contains the original file path (URL), an offset into the
  //  binary asset file, a content size and whether the content
  //  was compressed
  auto sql = "select frag, gzip, cty, start, size from assets where origname='" + path + "'";

  //  Make sure this is the only query running
  std::lock_guard<std::mutex> lck(SQLLock);
  
  DEBUG_OUT("Query: %s\n", sql.c_str());

  const char *tail;
  sqlite3_stmt *res;
  auto rc = sqlite3_prepare_v2(assetDB, sql.c_str(), -1, &res, &tail);
  if (rc != SQLITE_OK) 
  {
    DEBUG_OUT("SQL error %s\n", sqlite3_errmsg(assetDB));
    return RQState::RQError;
  }
  
  rc = sqlite3_step(res);
  if (rc == SQLITE_NOMEM) 
  {
    //  Out of memory - can retry
    sqlite3_finalize(res);
    return RQState::RQRetry;
  }
  else
  if (rc != SQLITE_ROW)
  {
    sqlite3_finalize(res);
    LOG_OUT("URL not found (%d) %s\n", rc, path.c_str());
    return RQState::RQError;
  }

  //  Located the URL - get the file locatation
  //
  //  Note: We must grab this information before closing the
  //        SQL statement, otherwise the information will be
  //        corrupted
  //
  assetFrag     = sqlite3_column_int(res, 0);
  assetGZ       = sqlite3_column_int(res, 1);
  auto tmpType  = (char *) sqlite3_column_text(res, 2);
  assetStart    = sqlite3_column_int(res, 3);
  assetSize     = sqlite3_column_int(res, 4);

  //  The majority of content in the database has a type, but
  //  we make sure we set something if the type is missing for
  //  some reason
  if (tmpType == nullptr)
  {
    strcpy(assetType, "text/plain");
  }
  else
  {
    if (strlen(tmpType) < ctyStrLen - 1)
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

  return RQState::RQOK;
}

//  Send a retry request to get the browser to try a URL again. This
//  is sent if we hit a request threshold or run into errors that can
//  be retried like a low memory condition
//
//  We prepend the redirectStr to the URL to make it look different
//
void SendRetry (AsyncWebServerRequest *request)
{
  delay(250);
  auto inURL = request->url();
  request->redirect(String(redirectStr) + inURL);
}

//  Determine what to do with the incoming URL
RQState Director (AsyncWebServerRequest *request)
{
  auto path = request->url();

  DEBUG_OUT("Got : %s\n", path.c_str());

  //  Looking for the root?
  if (path == "/")
  {
    if (SDCARD.exists("/MAIN.TXT"))
    {
      //  Specific URL for the main page exists
      auto main = SDCARD.open("/MAIN.TXT", FILE_READ);
      auto line = main.readStringUntil('\n');

      //  Can close the file
      main.close();

      //  Create a new URL
      path = "/" + line;
    }
    else
    {
      //  Good starting point
      path += "index.html";
    }
  }

  DEBUG_OUT("In URL now %s\n", path.c_str());

  //  Remove any %XX encoding
  path = request->urlDecode(path);

  //  Sanitize the path according to the following rules (follows the convention in the
  //  convertzim.py script):
  //
  //  Character   Becomes
  //  ,           x
  //  "           y
  //  '           z
  //
  //   This prevents the input path containing characters that would derail a query
  //
  path.replace(",", "x");
  path.replace("\"", "y");
  path.replace("'", "z");

  //  Chew off the redirection string
  path.replace(redirectStr, "");

  DEBUG_OUT("Now : %s\n", path.c_str());

  //  Query the database to obtain metadata for this URL
  int       assetFrag;
  int       assetGZ;
  char      assetType [ctyStrLen + 1];
  uint32_t  assetStart;
  size_t    assetSize;  
  auto      rc = RunQuery (path, assetFrag, assetGZ, assetType, assetStart, assetSize);
  if (rc != RQState::RQOK)
  {
    return rc;
  }

  //  Open a new asset fragment
  char assetName [16];
  snprintf(assetName, sizeof(assetName) - 1, "/ASSET-%02d.BIN", assetFrag);
  if (!SDCARD.exists(assetName))
  {
    LOG_OUT("Cannot locate %s\n", assetName);
    return RQState::RQError;
  }

  //  Track pending requests so we don't overrun the system
  pendingRequests ++;

  //  Use a lambda function to handle the chunked response. This will accept the asset 
  //  fragment filename, and the content offset and size. The lambda function opens
  //  the file each time it executes, sets the correct file offset and reads a block
  //  of data to send to the browser
  //
  auto response = request->beginChunkedResponse(assetType, [assetName, assetStart, assetSize] (uint8_t *buffer, size_t maxLen, size_t index) -> size_t 
  {
    //  Open the asset fragment file
    auto binAsset = SDCARD.open(assetName, FILE_READ);
    if (!binAsset)
    {
      LOG_OUT("Cannot open %s\n", assetName);
      return 0;
    }

    //  Calculate the correct offset into the file
    auto offset = assetStart + index;
    if (index >= assetSize
    ||  offset >= (assetStart + assetSize))
    {
      //  We are done reading the file!
      binAsset.close();
      pendingRequests --;
      return 0;
    }

    //  Position to the correct offset
    binAsset.seek(offset);
            
    // Determine how much to read
    auto thisLen    = min(maxLen, assetSize - index);
    auto bytesRead  = binAsset.read(buffer, thisLen);
            
    //  Close the file
    binAsset.close();

    if (bytesRead == badRead)
    {
      //  We had a bad read - stop sending data
      bytesRead = 0;
      pendingRequests --;
      LOG_OUT("Block read error\n");
    }

    return bytesRead;
  });

  //  Compressed output
  if (assetGZ != 0)
  {
    DEBUG_OUT("Setting compressed data\n");
    response->addHeader("Content-Encoding", "gzip");
  }

  request->send(response);

#if (LCD_ON)
  //  All chunks sent
  assetsServed ++;
#endif  // LCD_ON

  DEBUG_OUT("Content sent\n");

  return RQState::RQOK;
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

  //  The default begin() call only allows 5 open files. We need more, so we have to call
  //  a different begin() method to pass the number of open files needed
  if (SDCARD.begin("/sdcard", false, false, BOARD_MAX_SDMMC_FREQ, maxRequests + 4)) 
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
  
  //  Send all requests to the Director() function
  server.onNotFound([](AsyncWebServerRequest *request)
  {
    //  If we don't limit the amount of pending requests, we end up running
    //  out of memory and SQLite queries cannot execute
    if (pendingRequests >= maxRequests) 
    {
      //  If we reach a request threshold, we need to ask the browser to retry the URL. Initially, 
      //  this sent the following response:
      //
      //    request->send(503, "text/plain", "Service Unavailable: Max Requests Reached");
      //
      //  But, the browser does not retry the URL. This was then changed to:
      //
      //    auto response = request->beginResponse(429, "text/plain", "Too Many Requests");
      //    response->addHeader("Retry-After", "10");
      //    request->send(response);
      //
      //  Same situation - no retries.
      //
      //  The Sketch now sends back a redirect response (302) so the browser will retry the URL
      //
      SendRetry(request);
      return;
    }

    //  See if we can serve this URL
    auto rc = Director(request);
    if (rc == RQState::RQRetry) 
    {
        SendRetry(request);
    }
    else
    if (rc == RQState::RQError)
    {
      request->send(404, "text/plain", "Page " + request->url() + " was not found");
    }
  });

  //  Start the web server
  server.begin();

  LCD_OUT("Web Server IP: %s\n", myIP.toString());
  LCD_OUT("Host %s ready!\n", hostname);
}

void loop (void)
{
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
