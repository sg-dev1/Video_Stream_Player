# Video Formats

- YV12: 12bpp, 1x1 Y, 2x2 VU sample
    https://msdn.microsoft.com/en-us/library/windows/desktop/dd206750(v=vs.85).aspx#yv12
    - 4:2:0 format, 12 bits per pixel   (YVU)
- I420: 12bpp, 1x1 Y, 2x2 UV sample
    https://wiki.videolan.org/YUV/#I420
    - 4:2:0 fomrat, 12 bits per pixel   (YUV)


# Image Stride

- bytes in a line (incl. padding)
- in vlc called pitch: e.g. plane_t::i_pitch
    https://www.videolan.org/developers/vlc/doc/doxygen/html/structplane__t.html#ad583ae84ac5172b4c0e1e0e5068b16eb
    https://msdn.microsoft.com/en-us/library/windows/desktop/aa473780(v=vs.85).aspx


# Sample Aspect Ratio (SAR)

e.g. 16:9, 1920x1080  --> 1080/1920/9*16 = 1 (1/1)
     16:9, 720:480    --> 480 ÷ 720 ÷ 9 x 16 = 1.185185185 (32/27)
        --> calculate ggt (größter gemeinsamer Teiler) to get ratio
  - https://de.wikipedia.org/wiki/Euklidischer_Algorithmus#Beispiel
            - 480 * 16 = 7680 |
              720 * 9  = 6480 | --> 7680/6480
            - ggt(7680, 6480) = 240
            --> 7680/6480 == (7680/240)/(6480/240) == 32/27
            see http://forum.doom9.org/showthread.php?t=106656

- MMAL num, den:
    * Describes a rational number
      * den: Denominator  q  (Nenner)
      * num: Numerator    p  (Zähler)
          --> p / q
    - see https://goo.gl/A5CHZt


# Rendering

- Simple solution: dump to file and use program such as yay (yet another yuv viewer) to view it
    https://github.com/macooma/yay

- Use Simple DirectMedia Layer (SDL)

## SDL2
- https://www.libsdl.org/
- libSDL2: very good for simple baseline implementation
- Supports OpenGL ES and should work on the raspberry without X (theoretically)
- If used on raspberry self compiled is best solution (since distros have enabled X per default)

- manual compile (may be needed on raspberry pi)
```
# see https://www.libsdl.org/download-2.0.php for current release
wget https://www.libsdl.org/release/SDL2-2.0.7.zip
unzip SDL2-2.0.7.zip
# see https://discourse.libsdl.org/t/problems-using-sdl2-on-raspberry-pi-without-x11-running/22621/4
./configure --host=arm-raspberry-linux-gnueabihf --disable-video-opengl --disable-video-x11 --disable-pulseaudio --disable-esd --disable-video-mir --disable-video-wayland
make -j 4
```

- additional (optional/alternative) steps
```
# (optional - absolute paths need to be adapted)
mkdir install
libtool --mode=install /usr/bin/install -c build/libSDL2.la ~/tmp/SDL2-2.0.7/install

# (alternative)
# use build script from https://github.com/simple2d/simple2d#on-linux
# e.g.
url='https://raw.githubusercontent.com/simple2d/simple2d/master/bin/simple2d.sh'; which curl > /dev/null && cmd='curl -fsSL' || cmd='wget -qO -'; bash <($cmd $url) install --sdl
# NOTE '--sdl' flag just installs sdl lib
# see https://discourse.libsdl.org/t/problems-using-sdl2-on-raspberry-pi-without-x11-running/22621/5
# WARNING could be problematic if libsdl2 is allready installed
#    - e.g. as dependency of some lib (e.g. ffmpeg)

# get compile params --> use sdl2-config script
sdl2-config --cflags --libs
# e.g. gcc `sdl2-config --cflags --libs` -v -o myprog main.c > compile.log 2>&1
```

## Raspberry Pi

- SDL2 (as explained above)
    - Issues: dependency to X server --> compile it manually
    - Other programs depending on libSDL2 (e.g. ffmpeg)
        - So the version requiring the X server could be already installed
- use MMAL
    examples:
      https://github.com/videolan/vlc/tree/master/modules/hw/mmal
      - check Open(), Close() and vd_display() functions
- use OpenMAX IL
    is this possible at all?
    - seems to be not possible
        https://github.com/raspberrypi/firmware/issues/218
- use OpenGL ES
    - complicated

## Timing

- example: 24 fps --> frameDuration of ~0.041667
- frameDuration is the timespan where one single frame will be displayed


# Encrypted Format (e.g. used by Widevine)

- Must consider NALU header, NALU length etc.
    https://msdn.microsoft.com/en-us/library/windows/desktop/dd757808(v=vs.85).aspx
