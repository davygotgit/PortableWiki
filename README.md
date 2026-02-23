# PortableWiki
Serve wikipedia and other offline content from an ESP32 microcontroller


## Background
In late 2025, I found myself in a situation where I needed to serve offline content (offline maps, medical and technical information). I ended up using some Radxa Cubie A7Z (1GB RAM with GPIO) boards (https://radxa.com/products/cubie/a7z/) running Armbian (https://forum.armbian.com/topic/56130-radxa-cubie-a7aa7z-allwinner-a733/) and Internet in a Box (https://internet-in-a-box.org/). 

If you are interested in self-hosting offline content, there is also Kiwix (https://kiwix.org/en/) and RACHEL (https://worldpossible.org/). Internet in a Box uses some resources from both Kiwix and RACHEL. 

Here are some additional links, that I found useful, for self-hosting a Wikipeda mirror and setting up a Kiwix server on a Raspberry PI:

	https://docs.sweeting.me/s/self-host-a-wikipedia-mirror
	https://www.xda-developers.com/raspberry-pi-kiwix-guide/

The Radxa Cubie A7Z with Internet in a Box solution worked very well for my use case, but I wanted to see if I could serve content from a power efficient device without a sophisticated OS.


## Quick Start
The general process is:

1. Download a ZIM file.
2. Process the ZIM file with some Python scripts.
3. Copy the processed result to an SD Card and insert into a microcontroller.
4. Load a Sketch onto the microcontroller and serve content.

This section assumes you already have some experience with the Arduino IDE and the Linux (or Windows) command line. Before starting, you will need the following components:

  LILYGO T-Dongle-S3 microcontroller
  USB SD Card Reader
  Up to 128 GB SD Card (freshly formatted for FAT32 with the largest allocation block possible)
  USB-A to USB-C Cable
  Arduino IDE
  Python 3
  SQLite 3
  The git or unzip utility

The following instructions assume the user fred. The project uses a LILYGO T-Dongle-S3 microcontroller. Visit https://github.com/Xinyuan-LilyGO/T-Dongle-S3/blob/main/docs/en/t-dongle-s3/REAMDE.MD to install the board for the Arduino IDE.

Access the Kiwix Library (https://library.kiwix.org/) and select the content you would like to serve from the microcontroller. This site contains a number of ZIM files. Make sure you pick content that is under 90 GB so it will it onto the SD Card after processing. We will assume the file content.zim has been downloaded.

Open a terminal (Command Prompt on Windows) and use the following command to download the repository:

  git clone https://github.com/davygotgit/PortableWiki.git
  
You can also visit https://github.com/davygotgit/PortableWiki to download and extract a ZIP file. After this step you should have a ~/PortableWiki directory. For Windows this might be C:\Users\fred\Downloads\PortableWiki (unzip) or C:\Users\fred\PortableWiki (git).

Access the PortableWiki directory by running the following command (adjust for your environment):

	cd PortableWiki

Run the following commands to install the libzim package (first time only):

	python -m venv portablewiki
	portablewiki/bin/pip install libzim

Run the following command to convert the downloaded ZIM file for use on the T-Dongle:

	portablewiki/bin/python scripts/zim2asset.py ~/Downloads/content.zim assets

Adjust ~/Downloads/content.zim to match the ZIM file download or your OS (Windows would be C:\Users\fred\Downloads\content.zim). The assets directory will be created if it does not exist. This phase can take a long as 60 minutes, depending on the type of content downloaded. There should be no errors reported. If so, insert the SD Card into the USB SD Card Reader and then insert the reader into a USB port. Run the following command to:

	python scripts/finish.py assets/ /media/fred/PORTWIKI/

Adjust /media/fred/PORTWIKI to point to the root folder on the SD Card. The transfer of data to the SD Card can take several minutes. After the transfer completes, use the OS to eject the SD Card and insert it into the T-Dongle.

Open the Arduino IDE and use the File -> Open menu option to open the portablewiki.ino Sketch in ~/PortableWiki/src. Adjust this path for your environment. You will also need to use the Arduino Library Manager to install the Sqlite3Esp32 library.

Connect the T-Dongle to a USB port and select the ESP32S3 Dev Module board in the Arduino IDE. Compile and transfer the Sketch to the T-Dongle.

The Sketch will output some information on the screen of the T-Dongle. The Sketch will create a WiFi access point called portablewiki. Connect to the access point (default password is topsecret). You can then access the content using http://portablewiki.local.

Be patient. Some content can take 30-45 seconds to load.

## The Device

I have access to several different microcontollers. I decided to use the T-Dongle as I liked the format factor, as well as the LCD screen and the SD Card. Here's a picture of the T-Dongle next to a USB drive for scale:

<img width="50%" height="50%" alt="image" src="https://github.com/user-attachments/assets/3a12d9cd-9f57-4308-8e1c-c20dab83555e" />


The back of the T-Dongle has a boot button, if you need to use it:

<img width="50%" height="50%" alt="image" src="https://github.com/user-attachments/assets/748c0901-b57d-442f-be85-57c3c72fe4e6" />


If you look very closely, you will see a green insert in the USB-A connector:

<img width="50%" height="50%" alt="image" src="https://github.com/user-attachments/assets/8a2631c6-6b0b-458f-acc1-bb5eb9e80df0" />


The green insert is there to protect the SD Card reader until it is used. This is where the SD Card can be inserted:

<img width="50%" height="50%" alt="image" src="https://github.com/user-attachments/assets/0117e881-04aa-4b93-a154-b0eacbb03dc1" />


## The Running Project

Here’s a picture of the T-Dongle with the application started:

<img width="50%" height="50%" alt="image" src="https://github.com/user-attachments/assets/e689ce2e-d264-4e17-9af7-2037105663c3" />


The project creates an access point called portablewiki, with a web server that can be accessed from a browser using IP address 192.168.4.1 or portablewiki.local after connecting to the access point. The screen on the T-Dongle gives some great user feedback in terms of the access point and host access as well as the amount of content served over time:

<img width="50%" height="50%" alt="image" src="https://github.com/user-attachments/assets/b0587240-b56d-4fc7-acbe-76761e9fa8ad" />


The project can be built to disable screen output to save power, so this project also uses an LED on the back of the T-Dongle to indicate an error (red) or normal operation (green):

<img width="50%" height="50%" alt="image" src="https://github.com/user-attachments/assets/74660397-00ca-4c16-9ebc-078033d8d90a" />


Here’s the output of an astronomy ZIM file hosted locally using the kiwix-serve utility and accessed from Firefox on using localhost:8888:

<img width="50%" height="50%" alt="image" src="https://github.com/user-attachments/assets/61d2b738-9fd5-4559-83cc-1c22f7bd784f" />


Clicking on the “book” gives the following output:

<img width="50%" height="50%" alt="image" src="https://github.com/user-attachments/assets/909f51eb-f905-43d5-bd5f-9dd069fcd4fe" />


The same ZIM content, after processing to fit onto the T-Dongle, looks like this:

<img width="2494" height="1408" alt="image" src="https://github.com/user-attachments/assets/0c1dc8df-f10d-411e-be68-cf23493ff3eb" />


The T-Dongle version goes directly to the content, but takes ~44 seconds to load. The content hosted by the kiwix-serve utility loads in 2-3 seconds. However, this is the difference between running on a Linux laptop with an Intel CoreTM i3-1215U (6 cores @ 3.3GHz), 8GB RAM and NVME storage vs running on a T-Dongle with Xtensa LX7 (2 cores @ 240MHz), 512KB RAM and SD Card storage.

The following table shows some timings for various ZIM files to load using kiwix-serve and the PortableWiki project:

|ZIM File|ZIM Size|Kiwix-serve time|PortableWiki size|PortableWiki time|
|--------|--------|----------------|-----------------|-----------------|
|devdocs_en_python_2026-02.zim|4.1 MB|2 seconds|5.1 MB|5 seconds|
|freecodecamp_en_javascript-algorithms-and-data-structures_2026-02.zim|6.7 MB|2 seconds|13 MB|4 seconds|
|ted_mul_education_2026-01.zim|7.3 GB|4 seconds|7.5 GB|1 minute 40 seconds|
|wikipedia_en_all_nopic_2025-12.zim|48 GB|1 second|66 GB|4 seconds|
|wikipedia_en_astronomy_maxi_2025-11.zim|1.5 GB|2 seconds|1.6 GB|44 seconds|
|wikipedia_en_top1m_maxi_2026-01.zim|46 GB|2 seconds|50 GB|45 seconds|

The Ted ZIM file contains a very large JSON object, in a JavaScript file, which controls the way the content is rendered. The JSON takes a long time to download. After that, a number of thumbnails have to be downloaded for each video which takes extra time.

The Astronomy and Top 1 Million ZIM files have landing pages utilize several images which takes time to download.

The Python, JavaScript and English Wikipedia (no pictures) ZIM files are text based and load in a time that is acceptably close to the kiwix-serve time.

Note that the English Wikipedia content size is significantly larger than the ZIM content. ZIM files use ZSTD compression and also cluster content into 1 MB chunks to improve the compression ratio. The PortableWiki project uses GZIP compression on individual files. This trades simplicity for a lower compression ratio.


## What do I need to build the project?

You will need:

1. A LILYGO T-Dongle-S3 microcontroller (https://lilygo.cc/products/t-dongle-s3?srsltid=AfmBOor0538aF0NND5JlcwYaf1nITRw9S8lsJHexYojBZY3xOpvSrcvK). The microcontroller is also available on a variety of other online stores like AliExpress and Amazon.
2. A PC with Windows, Linux, or a Mac to install the Arduino IDE which can be downloaded here https://www.arduino.cc/en/software/.
3. A USB-A to USB-C cable to connect the PC or Mac to the T-Dongle.
4. A USB SD Card reader for formatting and data transfer (https://www.amazon.com/uni-Reader-Adapter-Aluminum-Memory/dp/B087QG75L7?th=1). 
5. The git utility to access the GIT repository (git clone https://github.com/davygotgit/PortableWiki.git) or visit https://github.com/davygotgit/PortableWiki and download a ZIP file.


## How do I install and configure the tools?

Here are some instructions for downloading and installing GIT https://github.com/git-guides/install-git.

Here are some instructions on how to download and install the Arduino IDE https://docs.arduino.cc/software/ide-v2/tutorials/getting-started/ide-v2-downloading-and-installing/.

ILYGO have a quick start guide here https://github.com/Xinyuan-LilyGO/T-Dongle-S3/blob/main/docs/en/t-dongle-s3/REAMDE.MD. Pay close attention to the list of Arduino IDE configuration options that need to be applied to the board. The only change I made was changing Events Run On: “Core 1” to Core 0 (from the Tools menu):

<img width="50%" height="50%" alt="image" src="https://github.com/user-attachments/assets/f7a64708-6871-4bca-bdf8-9f5d79af4700" />


I don’t recall having to install any serial drivers on my version of Ubuntu 24.04.2 LTS. Some microcontrollers require CH340 or CP210x drivers. It’s possible this is already included in the kernel. On Linux you must add your account to the dialout group by running the following bash command:

	sudo usermod -a -G dialout <your_account>

For example, if your user account is fantasticfred:

	sudo usermod -a -G dialout  fantasticfred

You must log out your current session and log back in again for this change to become active. I would first see if your Arduino IDE can see the T-Dongle device before attempting to install any drivers. You know you are connected to the T-Dongle if you see something similar to the following status (bottom right) in the Arduino IDE:

<img width="50%" height="50%" alt="image" src="https://github.com/user-attachments/assets/88d190cc-d2d9-4cb6-ab12-4a62141e26d2" />

The project uses standard ESP32 and Arduino libraries. The only additional library that needs to be installed, through the Arduino Library Manager, is Sqlite3Esp32.


## How do I prepare the environment?

You need to download the code from the GIT repository. This can be done by visiting https://github.com/davygotgit/PortableWiki and downloading, and then extracting, a ZIP file or by running the following terminal command from bash, a Windows Command Prompt, or any suitable GIT access tool:

	git clone https://github.com/davygotgit/PortableWiki.git

This will create a local copy of the repository in your home directory which will be ~/PortableWiki on Linux and something like C:\Users\<your ID>\PortableWiki on Windows. Just note the path of the extracted repository and use that to adjust these instructions.

You only need to extract the repository for now. The Arduino Sketch will not run correctly unless an SD Card is inserted into the T-Dongle. The Sketch does not attempt to validate the content of the SD Card, but processing will not get far if key files are not in place.

The project uses 128 GB SD Cards as these have been shown work with the T-Dongle. The SD Card must be formatted for FAT32 and must use the largest allocation unit possible, as this improves performance of data seek and transfer from the SD Card.

On Windows, an SD Card can be formatted using Windows Explorer and is fairly intuitive. This is the best way to format an SD Card if you are not familiar with Linux or formatting disks on Linux. Formatting disks on Linux has a wide margin for error. A good guide for formatting SD Cards can be found here https://wiki.hacks.guide/wiki/SD_Clean/Linux. Note that using a USB SD Card reader will likely make the SD Card use a device name like /dev/sda or /dev/sdb. Pay very close attention to the device you are formatting. Once the SD Card is ready to format, use the -s 64 or -s 128 option to the mkfs.vfat utility to use the largest allocation size possible.

Once the SD Card has been formatted, visit the Kiwix library (https://library.kiwix.org/) and download the content you would like to serve. The instructions assume the Top 1 Million Wikipedia articles have been downloaded (wikipedia_en_top1m_maxi_2026-01.zim).

Open a terminal to access the PortableWiki repository directory:

	cd PortableWiki

Create a Python virtual environment and then install the zimlib package:

	python -m venv portablewiki
	portablewiki/bin/pip install libzim

This step only needs to be done once. Run the zim2asset.py script from the PortableWiki repository:

portablewiki/bin/python scripts/zim2asset.py ~/Downloads/wikipedia_en_top1m_maxi_2026-01.zim assets/	


The assets directory will be created by the zim2asset.py script. For every 1000 content files processed, the script output a message similar to the following:

	Complete: 6000/204025 (2.94%)

There should be no errors. Depending on the size, ZIM files can take as long as an hour to process and will output a message similar to the following when finished:

	Complete: 204025/204025 (100.00%)

Run the following script to complete the transfer of data to the SD Card:

	python scripts/finish.py assets/ /media/me/PORTWIKI/

The assets/ parameter is the directory that holds the assets processed by the zim2asset.py script and /media/me/PORTWIKI is where Linux mounted your SD Card. This will likely be a different location on your system.

The finish.py script should not fail with any errors, but you may see the following output when processing some content:

ASSET.CSV:224195: INSERT failed: UNIQUE constraint failed: assets.origname
ASSET.CSV:224246: INSERT failed: UNIQUE constraint failed: assets.origname
ASSET.CSV:837601: INSERT failed: UNIQUE constraint failed: assets.origname
ASSET.CSV:847170: INSERT failed: UNIQUE constraint failed: assets.origname

This is due to duplicate URLs in the ZIM file. The final.py script can take a while to complete, depending on how much data needs to be copied to the SD Card. Once the file transfer has completed, eject the SD Card using the OS and insert the SD Card into the T-Dongle.

The easiest way to build the project is as follows:

1. Start the Arduino IDE.
2. Use the File -> Open  menu option and navigate to the directory where the repository is located and open the src/portablewiki.ino file.

Once you have the initial project saved, you can just load it from File -> Open Recent menu option. With the Sketch loaded, connect the T-Dongle using the USB-A to USB-C cable. Ensure the T-Dongle board is selected (ESP32S3 Dev Module) and the USB port shows a connected status. Press the Upload button on the toolbar. The Sketch will be compiled and transferred to the T-Dongle. The application will start after the transfer completes.


## Do you have any tips for using LILYGO microcontrollers?

As with all projects, I used a number of the examples created by the manufacturer (https://github.com/Xinyuan-LilyGO/T-Dongle-S3) to make sure I could flash the board and that the code worked.

The example code has an example web server that serves content from an SD Card (https://github.com/Xinyuan-LilyGO/T-Dongle-S3/tree/main/examples/fs_webserver), which I used as a starting point for this project.


## Were there any challenges creating this project?

This project had several challenges and pivoted many times before landing on a solution that worked. Initially, I thought I would use wget to download Wikipedia content, but quickly realized that approach would not work due to the amount of time it would take to download the content.

I started researching this area closer and found that a lot of people download the content dumps provided by Wikipedia themselves. I found a very interesting article about processing such content https://jamesthorne.com/blog/processing-wikipedia-in-a-couple-of-hours/. Coupled with PlainTextWikipedia (https://github.com/daveshap/PlainTextWikipedia), I put together a Python script that extracted articles from the Wikipedia dump files. However, these were pure text as the script stripped most of the internal Wikipedia formatting like references/citations etc. I made a slight modification to the script to also locate images, thinking I could at least have basic text and images. However, using wget to download an image from the main Wikipedia website did not work as I expected. When you try to do this, you get an XML or JSON file that gives you metadata on where the actual image is located. Another modification to the script isolated the correct image location, but this process proved to be very slow. At this point, I put the project aside as I felt I was approaching the problem from the wrong direction.

A few weeks later, I was installing Internet in a Box (IIAB) onto some Radxa Cubie A7Z boards. This worked really well, and I thought I could use wget against the IIAB installation to extract content. As most of the content is behind JavaScript, wget was not able to get most of it. I also tried HTTrack, which got more content, but not enough to make the project viable.

I decided to extract one of the ZIM files using the zimdump utility to see if that would yield some results. This was pretty hit or miss. A number of ZIM archives ended up with several files in an _exceptions directory as the content filename could not be translated correctly to a file system filename.

I tried serving ZIM file content using the kiwix-serve utility and extracting the content with wget. This worked well for some ZIM files, but not all. I also burned more time that I want to admit trying extracted content by running firefox index.html (or whatever HTML content had been extracted). I did not realize that, as a security feature, a lot of JavaScript and other functionality is disabled which causes a number of pages to render incorrectly. After I realized this was happening, I started using the Python web server (https://realpython.com/python-http-server/) to test extracted content. Some content was rendered correctly, but other content could not be served. This occurred because some ZIM files have HTML content that don’t have .html file extensions, and the Python web server does not know what content it is serving.

After finding some challenges serving extracted content with the Python web server, I encountered the libzim package and decided to use the library to extract ZIM file content. This method captures all the content and relevant metadata e.g. the type of the content e.g. HTML, CSS, JavaScript etc. The content and metadata is repackaged into a microcontroller friendly format. This method seems to reliable, as long as the repacked content can fit on a 128 GB SD Card.

This was an interesting journey. On the surface, using wget to download web content seems like it should go smoothly. However, this is not always the case if content is hidden behind layers of JavaScript. On top of that, Wikipedia pages have interesting formatting rules, which I did not want to put the time into learning. It was great to be able to leverage the work already done by Kiwix, to serve content in a slightly different way. But, this project is no way a replacement for Kiwix. The project tests the limits of the T-Dongle, but this is no match for Kiwix (especially through nginx) or Internet in a Box. 


## Next Steps

1. To make the content serve a little faster, I may look into the use of the ESP Async Web Server. This presents a challenge for SQLite which is not thread safe. The Sketch will have to be adjusted to place the SQLite access into a function and limit access with a mutex.
2. I may look at reducing JPG and PNG images by 50% to make them load faster.
