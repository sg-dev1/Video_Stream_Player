- Audio clear bytes calculation if no subsamples
    - check out Audio Data Transport Stream (ADTS) header
        https://wiki.multimedia.cx/index.php/ADTS
        https://wiki.multimedia.cx/index.php/MPEG-4_Audio
        http://lists.live555.com/pipermail/live-devel/2009-August/011113.html
        https://chromium.googlesource.com/chromium/src/media/+/master/formats/mp4/mp4_stream_parser.cc#681

- Audio decoding/transforming etc.
    - Audio conversion cheat sheet: http://stefaanlippens.net/audio_conversion_cheat_sheet/
    - SoX (Sound eXchange)
        - http://sox.sourceforge.net/sox.html
        - example 1:
            > sox -r 24k -e float -b 32 -c 2 <rawfile-name.pcm>.raw out.wav
            > mplayer out.wav
        - example 2:
            > play -r 48k -e signed-integer -b 16 -c 2 <s15le-file>.raw
        - https://stackoverflow.com/questions/19955635/sox-converting-files-sample-rate-not-specified
    - Convert AV_SAMPLE_FMT_FLTP (float 32bit planar mode) to AV_SAMPLE_FMT_S16 (signed int 16 bit interleaved)
        - https://stackoverflow.com/questions/22822515/ffmpeg-resampling-from-av-sample-fmt-fltp-to-av-sample-fmt-s16-got-very-bad-so

- Misc
    - Merge audio files dumped to /tmp
        > cat $(ls /tmp/widevineAdapter/*.pcm | sort -n) > /tmp/widevineAdapter/targetfile
