import signal
import logging
import json
import re
import os
import sys
from optparse import OptionParser

import widevineAdapter

from amazon import Amazon
from content_provider import ContentProvider
from constants import STREAM_TYPE_AUDIO, STREAM_TYPE_VIDEO
from util import downloadByteRange, selectVideoAndAudio, printResolutions, saveDataIntoFile, downloadFile
import util
import constants
import logging_config

LOG = logging.getLogger(__name__)
URL_REGEX = re.compile('http[s]?://(?:[a-zA-Z]|[0-9]|[$-_@.&+]|[!*\(\),]|(?:%[0-9a-fA-F][0-9a-fA-F]))+')


def parseArgs():
    usage = ***REMOVED***Usage: %prog [options] url|asin***REMOVED***
    parser = OptionParser(usage=usage)

    # TODO add more options
    parser.add_option(***REMOVED***-r***REMOVED***, ***REMOVED***--resolution***REMOVED***, dest=***REMOVED***resolution***REMOVED***,
                      help=***REMOVED***Choose desired resolution, default is '1920x1080' (full hd)***REMOVED***, metavar=***REMOVED***RES***REMOVED***)

    parser.add_option(***REMOVED***-a***REMOVED***, ***REMOVED***--audio-rep***REMOVED***, dest=***REMOVED***audioRep***REMOVED***,
                      help=***REMOVED***Select audio representation in the format 'audioAdaptionSet:representation', e.g. '0:0' ***REMOVED***
                           ***REMOVED***(use the '-p' option to get these indices)***REMOVED***, metavar=***REMOVED***REP***REMOVED***)
    parser.add_option(***REMOVED***-v***REMOVED***, ***REMOVED***--video-rep***REMOVED***, dest=***REMOVED***videoRep***REMOVED***,
                      help=***REMOVED***Select video representation in the format 'representation', e.g. '0' ***REMOVED***
                           ***REMOVED***(use the '-p' option to get this index)***REMOVED***, metavar=***REMOVED***REP***REMOVED***)

    parser.add_option(***REMOVED***-p***REMOVED***, ***REMOVED***--print-resolutions***REMOVED***,
                      action=***REMOVED***store_true***REMOVED***, dest=***REMOVED***printResolutions***REMOVED***, default=False,
                      help=***REMOVED***Just print available audio/video resolutions an then quit.***REMOVED***)

    parser.add_option(***REMOVED***-d***REMOVED***, ***REMOVED***--delete-dumpdir***REMOVED***,
                      action=***REMOVED***store_true***REMOVED***, dest=***REMOVED***deleteDumpDir***REMOVED***, default=False,
                      help=***REMOVED***Deletes the file dump directory under /tmp/widevineadapter which is used when no ***REMOVED***
                           ***REMOVED***video/audio rendering is implemented.***REMOVED***)

    parser.add_option(***REMOVED***-s***REMOVED***, ***REMOVED***--save-remotefile***REMOVED***,
                      dest=***REMOVED***saveRemoteFile***REMOVED***, help=***REMOVED***Save a remote file: use 'v' for video, 'a' for audio, or 'b' for both***REMOVED***, metavar=***REMOVED***FILE_TYPE***REMOVED***)
    parser.add_option(***REMOVED***-l***REMOVED***, ***REMOVED***--set-loglevel***REMOVED***, dest=***REMOVED***loglevel***REMOVED***,
                      help=***REMOVED***Select the logging level. Default is 'INFO'. Allowed values are 'TRACE', 'DEBUG', 'INFO', and 'WARN'.***REMOVED***,
                      metavar=***REMOVED***LEVEL***REMOVED***)

    (options, args) = parser.parse_args()
    if len(args) != 1:
        parser.error(***REMOVED***Incorrect number of arguments!***REMOVED***)

    return options, args[0]


if __name__ == ***REMOVED***__main__***REMOVED***:
    logging_config.configureLogging()
    options, amazonUrlOrAsin = parseArgs()

    if options.loglevel:
        logLevelInt = 3 # default INFO
        if options.loglevel == ***REMOVED***TRACE***REMOVED***:
            logLevelInt = 1
        elif options.loglevel == ***REMOVED***DEBUG***REMOVED***:
            logLevelInt = 2
        elif options.loglevel == ***REMOVED***INFO***REMOVED***:
            logLevelInt = 3
        elif options.loglevel == ***REMOVED***WARN***REMOVED***:
            logLevelInt = 4
        else:
            LOG.warning(***REMOVED***Unknown Log level %s given. Defaulting to INFO.***REMOVED*** % options.loglevel)
        widevineAdapter.SetLogLevel(logLevelInt)

    if options.deleteDumpDir:
        os.system(***REMOVED***rm {0}/****REMOVED***.format(constants.DUMP_DIRECTORY))

    if not os.path.isdir(constants.DUMP_DIRECTORY):
        os.mkdir(constants.DUMP_DIRECTORY)

    az = Amazon()

    if URL_REGEX.match(amazonUrlOrAsin):
        LOG.info(***REMOVED***Using amazon url: %s***REMOVED*** % amazonUrlOrAsin)
        # login is needed else e.g. no asin can be retrieved
        az.logIn()

        titleAmazonMovieEntryMap = az.getVideoPageDetailsMapping(amazonUrlOrAsin)
        for key, val in titleAmazonMovieEntryMap.items():
            LOG.info(***REMOVED***%s: %s***REMOVED*** % (key, val))
            LOG.info(***REMOVED******REMOVED***)
        if {} == titleAmazonMovieEntryMap:
            LOG.warning(***REMOVED***No video data could be found! Mapping was empty.***REMOVED***)
    else:
        LOG.info(***REMOVED***Launching rendering script for ASIN %s***REMOVED*** % amazonUrlOrAsin)

        width = 1920
        height = 1080
        if options.resolution:
            try:
                tmpArr = options.resolution.split(***REMOVED***x***REMOVED***)
                width = int(tmpArr[0])
                height = int(tmpArr[1])
            except Exception as e:
                LOG.warning(***REMOVED***Exception while parsing resolution: %s.\nUsing full hd as default.***REMOVED*** % e)
                width = 1920
                height = 1080

        provider = ContentProvider(provider=az)
        provider.connect()

        info = az.getFilmInfo(amazonUrlOrAsin)

        mpdUrl = info[***REMOVED***mpdUrl***REMOVED***]
        LOG.info(***REMOVED******REMOVED***)
        LOG.info(***REMOVED***Printing some useful stuff:***REMOVED***)
        LOG.info(***REMOVED***Title: %s***REMOVED*** % info[***REMOVED***title***REMOVED***])
        LOG.info(***REMOVED***MPD url: %s***REMOVED*** % mpdUrl)
        LOG.info(***REMOVED***Video Quality: %s***REMOVED*** % info['videoQuality'])
        LOG.info(***REMOVED******REMOVED***)

        if options.printResolutions:
            mpd_ = az.getMpdDataAndExtractInfos(mpdUrl)
            printResolutions(mpd_)
        else:
            audioAdaptionSetIndex = -1
            audioIndex = -1
            if options.audioRep:
                tmpArr = options.audioRep.split(***REMOVED***:***REMOVED***)
                audioAdaptionSetIndex = int(tmpArr[0])
                audioIndex = int(tmpArr[1])
            videoIndex = -1
            if options.videoRep:
                videoIndex = int(options.videoRep)

            mpd_ = provider.requestLicense(amazonUrlOrAsin, mpdUrl, STREAM_TYPE_VIDEO)
            try:
                provider.requestLicense(amazonUrlOrAsin, mpdUrl, STREAM_TYPE_AUDIO, adaptionSet=audioAdaptionSetIndex, mpdObject=mpd_)
            except Exception as e:
                LOG.warning(***REMOVED***Requesting license for AUDIO failed with %s***REMOVED*** % e)

            LOG.info(***REMOVED***Using widevinecdm version '%s'***REMOVED*** % widevineAdapter.GetCdmVersion())

            ***REMOVED******REMOVED******REMOVED***
            if mpd_.getWidevineContentProt(***REMOVED***audio***REMOVED***, audioAdaptionSetIndex):
                # provider.requestLicense(amazonUrlOrAsin, mpdUrl, ***REMOVED***audio***REMOVED***, audioAdaptionSetIndex, mpd_)
                # provider.requestLicenseForEstablishedSession(amazonUrlOrAsin, mpd_, ***REMOVED***audio***REMOVED***, audioAdaptionSetIndex)
                for aSet in mpd_.adaptionSetLst:
                    if aSet.contentType == ***REMOVED***audio***REMOVED*** and aSet.widevineContentProtection and aSet.playReadyContentProtection:
                        wv = aSet.widevineContentProtection.cencPsshData
                        pr = aSet.playReadyContentProtection.cencPsshData
                        cencInitData = util.buildWidevineInitDataString(None, cencInitDataWv=wv, cencInitDataPr=pr)
                        provider.requestLicenseForEstablishedSession(amazonUrlOrAsin, cencInitData)
            ***REMOVED******REMOVED******REMOVED***
            selectedVideoRepresentation, selectedAudioRepresentation = \
                selectVideoAndAudio(mpd_, width=width, height=height, audioAdaptionSetIndex=audioAdaptionSetIndex,
                                    audioIndex=audioIndex, videoIndex=videoIndex)
            videoFileUrl = selectedVideoRepresentation.mediaFileUrl
            audioFileUrl = selectedAudioRepresentation.mediaFileUrl

            if options.saveRemoteFile:
                if options.saveRemoteFile == ***REMOVED***a***REMOVED***:
                    saveDataIntoFile(downloadFile(audioFileUrl), ***REMOVED***audio.mp4***REMOVED***)
                elif options.saveRemoteFile == ***REMOVED***v***REMOVED***:
                    saveDataIntoFile(downloadFile(videoFileUrl), ***REMOVED***video.mp4***REMOVED***)
                elif options.saveRemoteFile == ***REMOVED***b***REMOVED***:
                    saveDataIntoFile(downloadFile(audioFileUrl), ***REMOVED***audio.mp4***REMOVED***)
                    saveDataIntoFile(downloadFile(videoFileUrl), ***REMOVED***video.mp4***REMOVED***)
                else:
                    LOG.error(***REMOVED***Save Remote file option %s unkown.***REMOVED*** % options.saveRemoteFile)
                    sys.exit(1)
                sys.exit(0)

            # download InitRange
            videoInitData = downloadByteRange(videoFileUrl, selectedVideoRepresentation.segmentList.initRange)
            audioInitData = downloadByteRange(audioFileUrl, selectedAudioRepresentation.segmentList.initRange)
            widevineAdapter.InitStream(
                videoFileUrl, selectedVideoRepresentation.segmentList.mediaRangeLst,
                videoInitData, selectedVideoRepresentation.codecPrivateData,
                audioFileUrl, selectedAudioRepresentation.segmentList.mediaRangeLst, audioInitData)
            del videoInitData
            del audioInitData

            #
            # Main video processing with v2 interface
            #
            widevineAdapter.StartStream()

            running = True
            def handler(signum, frame):
                print('Signal handler called with signal', signum)
                running = False
                widevineAdapter.StopStream()

            signal.signal(signal.SIGINT, handler)

            print(***REMOVED***Press CTRL+C to quit program.***REMOVED***)
            while running:
                # TODO fix issue with blocking when CTRL + C was already pressed
                command = input(***REMOVED***Enter command ('d' ... print diagnostics,\n***REMOVED*** \
                                ***REMOVED***               's' ... stop playback,\n***REMOVED*** \
                                ***REMOVED***               'r' ... resume playback,\n***REMOVED*** \
                                ***REMOVED***               CTRL + C to quit):\n***REMOVED***)
                if command == ***REMOVED***d***REMOVED***:
                    widevineAdapter.Diagnostics()
                elif command == ***REMOVED***s***REMOVED***:
                    LOG.info(***REMOVED***Stopping playback...***REMOVED***)
                    widevineAdapter.StopPlayback()
                elif command == ***REMOVED***r***REMOVED***:
                    LOG.info(***REMOVED***Resuming playback...***REMOVED***)
                    widevineAdapter.ResumePlayback()
                else:
                    print(***REMOVED***Unknown command: '%s'***REMOVED*** % command)

            # blocking call
            signal.pause()  # this is import, else the signal does not get handled
            # blocking call
            widevineAdapter.WaitForStream()

            #
            # Clean up
            #
            widevineAdapter.Close()
            provider.close()
