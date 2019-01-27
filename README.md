# Video Stream Player

Purpose of this application is to playback video streams from content providers such as Amazon Prime.

The current state of this project is a proof-of-concept with a fully working video playback from
Amazon Prime on Linux desktop PCs. Currently no other content provider than Amazon Prime is supported.
No advanced features such as subtitles or seeking to a certain time in the video stream is currently
supported. Video Rendering on ARM devices, especially the Raspberry Pi 3 is prepared, but not working
fully due to performance issues.

## Installation

Testing was mainly done on Manjaro Linux/Arch Linux which is also the recommended distro for installation.
On the Raspberry Pi 3 Arch Linux and Raspbian where used.

### Install Dependencies

- Python3 (packages: lxml, beautifulsoup4, requests, MechanicalSoup, pyCrypto)
- Python2 (packages: mechanize, beautifulsoup4)
- libav
- libcurl4
- SDL2 (MMAL and ALSA on the Pi)

```
# basic build tools and utility
sudo pacman -S cmake git gcc wget tar
# dependencies
sudo pacman -S python-lxml    # better use the arch package since there were compile errors
sudo pacman -S python-pip && sudo pip install -r requirements.txt
sudo pacman -S nss            # required by libwidevinecdm.so
# TODO ffmpeg (libav)
# TODO SDL2
```

### Get Source Code and Build it

```
git clone <repo-url>

# Get and build dependencies
cd widevine_adapter/lib
# Note the master branch is always tested/used
git clone https://github.com/sg-dev1/Bento4.git
cd Bento4
mkdir cmakebuild
cd cmakebuild
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j 4

# Build the library
cd wvAdapter/lib
mkdir build
cd build
cmake ..
make -j 4

# Build and install the python extension
sudo python setup.py install
```

## Quick Start Example

```
$> cd frontend
# fetch information for a certain season of a series
$> ./prime_video_cli https://www.amazon.de/gp/video/detail/B00ET0K3MM/ref=atv_dp

[   INFO: 01/26/2019 11:32:05 AM] Using amazon url: https://www.amazon.de/gp/video/detail/B00ET0K3MM/ref=atv_dp
[   INFO: 01/26/2019 11:32:07 AM] 1. Initiationsriten: [AmazonMovieEntry] asin=B00FGLE0TK,B00FGLEDHE, title=1. Initiationsriten, imgUrl=, synopsisText=, synopsisDict={}
[   INFO: 01/26/2019 11:32:07 AM]
[   INFO: 01/26/2019 11:32:07 AM] 2. Der Zorn der Wikinger: [AmazonMovieEntry] asin=B00ERM8MI4,B00ERM8L94, title=2. Der Zorn der Wikinger, imgUrl=, synopsisText=, synopsisDict={}
[   INFO: 01/26/2019 11:32:07 AM]
[   INFO: 01/26/2019 11:32:07 AM] 3. Enteignet: [AmazonMovieEntry] asin=B00ET4LQE2,B00ET4LPNO, title=3. Enteignet, imgUrl=, synopsisText=, synopsisDict={}
[   INFO: 01/26/2019 11:32:07 AM]
[   INFO: 01/26/2019 11:32:07 AM] 4. Der Prozess: [AmazonMovieEntry] asin=B00ETB4KQQ,B00ETB4IAY, title=4. Der Prozess, imgUrl=, synopsisText=, synopsisDict={}
[   INFO: 01/26/2019 11:32:07 AM]
[   INFO: 01/26/2019 11:32:07 AM] 5. Der Überfall: [AmazonMovieEntry] asin=B00ERJ81J2,B00ERJ7X3M, title=5. Der Überfall, imgUrl=, synopsisText=, synopsisDict={}
[   INFO: 01/26/2019 11:32:07 AM]
[   INFO: 01/26/2019 11:32:07 AM] 6. Das Begräbnis: [AmazonMovieEntry] asin=B00ERM7618,B00ERM77N0, title=6. Das Begräbnis, imgUrl=, synopsisText=, synopsisDict={}
[   INFO: 01/26/2019 11:32:07 AM]
[   INFO: 01/26/2019 11:32:07 AM] 7. Ein Vermögen: [AmazonMovieEntry] asin=B00EWIXHSI,B00EWIXH9C, title=7. Ein Vermögen, imgUrl=, synopsisText=, synopsisDict={}
[   INFO: 01/26/2019 11:32:07 AM]
[   INFO: 01/26/2019 11:32:07 AM] 8. Das Opfer: [AmazonMovieEntry] asin=B00ESV6NF8,B00ESV6OIO, title=8. Das Opfer, imgUrl=, synopsisText=, synopsisDict={}
[   INFO: 01/26/2019 11:32:07 AM]
[   INFO: 01/26/2019 11:32:07 AM] 9. Veränderungen: [AmazonMovieEntry] asin=B00EVB2UN4,B00EVB2W96, title=9. Veränderungen, imgUrl=, synopsisText=, synopsisDict={}
[   INFO: 01/26/2019 11:32:07 AM]

# e.g. select ***REMOVED***1. Initiationsriten***REMOVED*** with ASIN 'B00FGLE0TK'
$> ./prime_video_cli B00FGLE0TK

[   INFO: 01/26/2019 11:33:24 AM] Launching rendering script for ASIN B00FGLE0TK
...
Press CTRL+C to quit program.
Enter command ('d' ... print diagnostics,
               's' ... stop playback,
               'r' ... resume playback,
               CTRL + C to quit)
```

## Running the Application

Main usage of this application is via its command line interface found in *frontend/*.
Python3 is required to run this application.

- After first start your Amazon credentials will be requested
- These will be stored in settings.json s.t. that have not be typed in again the next time

```
$ ./prime_video_cli -h
#
# help output
#
Usage: prime_video_cli.py [options] url|asin

Options:
  -h, --help            show this help message and exit
  -r RES, --resolution=RES
                        Choose desired resolution, default is '1920x1080'
                        (full hd)
  -a REP, --audio-rep=REP
                        Select audio representation in the format
                        'audioAdaptionSet:representation', e.g. '0:0' (use the
                        '-p' option to get these indices)
  -v REP, --video-rep=REP
                        Select video representation in the format
                        'representation', e.g. '0' (use the '-p' option to get
                        this index)
  -p, --print-resolutions
                        Just print available audio/video resolutions an then
                        quit.
  -d, --delete-dumpdir  Deletes the file dump directory under
                        /tmp/widevineadapter which is used when no video/audio
                        rendering is implemented.
  -s FILE_TYPE, --save-remotefile=FILE_TYPE
                        Save a remote file: use 'v' for video, 'a' for audio,
                        or 'b' for both
```

## Appendix

### Raspberry Pi 3 Installation

Run the following to get setup of Raspbian:
```
sudo su
# basic build tools and utility
apt-get install cmake git gcc screen
# dependencies
#   (1) python stuff
apt-get install python3-bs4 python3-requests python3-crypto python3-mechanicalsoup python3-pip python3-lxml
pip3 install MechanicalSoup
#   (2) ffmpeg (libav)
apt-get install libavcodec-dev libavfilter-dev libavformat-dev
#   (3) libcurl4 (also includes libnss3-dev dependency)
apt-get install libcurl4-nss-dev
#   (4) Fix issue with linker not working due to libEGL named libbrcmEGL and same for libGLESv2
cd /opt/vc/lib
ln -s libbrcmEGL.so libEGL.so
ln -s libbrcmGLESv2.so libGLESv2.so
```

Then follow the [main build instructions](#get-source-code-and-build-it).
