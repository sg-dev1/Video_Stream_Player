# EME

- ISO/IEC 23001-7: MPEG Common Encryption (CENC)
    - Nice discussion: https://github.com/google/shaka-packager/issues/243
    - C++ library for parsing (Bento4) : https://github.com/axiomatic-systems/Bento4
                                         https://libraries.io/github/bitmovin/Bento4
        @Arch Linux package: https://aur.archlinux.org/packages/bento4/
        - better to build it using cmake/make  (Arch package seems to be old/incomplete)
    - public info (not sure if really ISO/IEC 23001-7 spec)
        https://wikileaks.org/sony/docs/05/docs/DECE/TWG/TechnicalWorkGroup_1.0.3%20Specs/TechnicalWorkGroup_Download/1.0.3_459/Specs-1.0.2-1.0.3r1-redline-Word/CFFMediaFormat-1.0.2-1.0.3-redline.txt

- PSSH format:  HEADER; uuid (16 bytes); len-payload (4 bytes)
        where HEADER: len-total (4 bytes); string "pssh"; 4 0x0 bytes

- Widevine ContentProtection element uuid EDEF8BA9-79D6-4ACE-A3C8-27DCD51D21ED
    (see http://dashif.org/identifiers/protection/)
    - Microsoft PlayReady DRM uuid: 9a04f079-9840-4286-ab92-e65be0885f95


# MP4
- Fragment vs Non-Fragment MP4
    https://stackoverflow.com/a/35180327

- To get list of Atoms of MP4 file Bento4's mp4dump can be used, e.g.:
      ./mp4dump --verbosity 3 <mp4-file-path>
