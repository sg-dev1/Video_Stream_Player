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
    usage = "Usage: %prog [options] url|asin"
    parser = OptionParser(usage=usage)

    # TODO add more options
    parser.add_option("-r", "--resolution", dest="resolution",
                      help="Choose desired resolution, default is '1920x1080' (full hd)", metavar="RES")

    parser.add_option("-a", "--audio-rep", dest="audioRep",
                      help="Select audio representation in the format 'audioAdaptionSet:representation', e.g. '0:0' "
                           "(use the '-p' option to get these indices)", metavar="REP")
    parser.add_option("-v", "--video-rep", dest="videoRep",
                      help="Select video representation in the format 'representation', e.g. '0' "
                           "(use the '-p' option to get this index)", metavar="REP")

    parser.add_option("-p", "--print-resolutions",
                      action="store_true", dest="printResolutions", default=False,
                      help="Just print available audio/video resolutions an then quit.")

    parser.add_option("-d", "--delete-dumpdir",
                      action="store_true", dest="deleteDumpDir", default=False,
                      help="Deletes the file dump directory under /tmp/widevineadapter which is used when no "
                           "video/audio rendering is implemented.")

    parser.add_option("-s", "--save-remotefile",
                      dest="saveRemoteFile", help="Save a remote file: use 'v' for video, 'a' for audio, or 'b' for both", metavar="FILE_TYPE")
    parser.add_option("-l", "--set-loglevel", dest="loglevel",
                      help="Select the logging level. Default is 'INFO'. Allowed values are 'TRACE', 'DEBUG', 'INFO', and 'WARN'.",
                      metavar="LEVEL")

    (options, args) = parser.parse_args()
    if len(args) != 1:
        parser.error("Incorrect number of arguments!")

    return options, args[0]


if __name__ == "__main__":
    logging_config.configureLogging()
    options, amazonUrlOrAsin = parseArgs()

    if options.loglevel:
        logLevelInt = 3 # default INFO
        if options.loglevel == "TRACE":
            logLevelInt = 1
        elif options.loglevel == "DEBUG":
            logLevelInt = 2
        elif options.loglevel == "INFO":
            logLevelInt = 3
        elif options.loglevel == "WARN":
            logLevelInt = 4
        else:
            LOG.warning("Unknown Log level %s given. Defaulting to INFO." % options.loglevel)
        widevineAdapter.SetLogLevel(logLevelInt)

    if options.deleteDumpDir:
        os.system("rm {0}/*".format(constants.DUMP_DIRECTORY))

    if not os.path.isdir(constants.DUMP_DIRECTORY):
        os.mkdir(constants.DUMP_DIRECTORY)

    az = Amazon()

    if URL_REGEX.match(amazonUrlOrAsin):
        LOG.info("Using amazon url: %s" % amazonUrlOrAsin)
        # login is needed else e.g. no asin can be retrieved
        az.logIn()

        titleAmazonMovieEntryMap = az.getVideoPageDetailsMapping(amazonUrlOrAsin)
        for key, val in titleAmazonMovieEntryMap.items():
            LOG.info("%s: %s" % (key, val))
            LOG.info("")
        if {} == titleAmazonMovieEntryMap:
            LOG.warning("No video data could be found! Mapping was empty.")
    else:
        LOG.info("Launching rendering script for ASIN %s" % amazonUrlOrAsin)

        width = 1920
        height = 1080
        if options.resolution:
            try:
                tmpArr = options.resolution.split("x")
                width = int(tmpArr[0])
                height = int(tmpArr[1])
            except Exception as e:
                LOG.warning("Exception while parsing resolution: %s.\nUsing full hd as default." % e)
                width = 1920
                height = 1080

        provider = ContentProvider(provider=az)
        provider.connect()

        info = az.getFilmInfo(amazonUrlOrAsin)

        mpdUrl = info["mpdUrl"]
        LOG.info("")
        LOG.info("Printing some useful stuff:")
        LOG.info("Title: %s" % info["title"])
        LOG.info("MPD url: %s" % mpdUrl)
        LOG.info("Video Quality: %s" % info['videoQuality'])
        LOG.info("")

        if options.printResolutions:
            mpd_ = az.getMpdDataAndExtractInfos(mpdUrl)
            printResolutions(mpd_)
        else:
            audioAdaptionSetIndex = -1
            audioIndex = -1
            if options.audioRep:
                tmpArr = options.audioRep.split(":")
                audioAdaptionSetIndex = int(tmpArr[0])
                audioIndex = int(tmpArr[1])
            videoIndex = -1
            if options.videoRep:
                videoIndex = int(options.videoRep)

            mpd_ = provider.requestLicense(amazonUrlOrAsin, mpdUrl, STREAM_TYPE_VIDEO)
            try:
                provider.requestLicense(amazonUrlOrAsin, mpdUrl, STREAM_TYPE_AUDIO, adaptionSet=audioAdaptionSetIndex, mpdObject=mpd_)
            except Exception as e:
                LOG.warning("Requesting license for AUDIO failed with %s" % e)

            LOG.info("Using widevinecdm version '%s'" % widevineAdapter.GetCdmVersion())

            """
            if mpd_.getWidevineContentProt("audio", audioAdaptionSetIndex):
                # provider.requestLicense(amazonUrlOrAsin, mpdUrl, "audio", audioAdaptionSetIndex, mpd_)
                # provider.requestLicenseForEstablishedSession(amazonUrlOrAsin, mpd_, "audio", audioAdaptionSetIndex)
                for aSet in mpd_.adaptionSetLst:
                    if aSet.contentType == "audio" and aSet.widevineContentProtection and aSet.playReadyContentProtection:
                        wv = aSet.widevineContentProtection.cencPsshData
                        pr = aSet.playReadyContentProtection.cencPsshData
                        cencInitData = util.buildWidevineInitDataString(None, cencInitDataWv=wv, cencInitDataPr=pr)
                        provider.requestLicenseForEstablishedSession(amazonUrlOrAsin, cencInitData)
            """
            selectedVideoRepresentation, selectedAudioRepresentation = \
                selectVideoAndAudio(mpd_, width=width, height=height, audioAdaptionSetIndex=audioAdaptionSetIndex,
                                    audioIndex=audioIndex, videoIndex=videoIndex)
            videoFileUrl = selectedVideoRepresentation.mediaFileUrl
            audioFileUrl = selectedAudioRepresentation.mediaFileUrl

            if options.saveRemoteFile:
                if options.saveRemoteFile == "a":
                    saveDataIntoFile(downloadFile(audioFileUrl), "audio.mp4")
                elif options.saveRemoteFile == "v":
                    saveDataIntoFile(downloadFile(videoFileUrl), "video.mp4")
                elif options.saveRemoteFile == "b":
                    saveDataIntoFile(downloadFile(audioFileUrl), "audio.mp4")
                    saveDataIntoFile(downloadFile(videoFileUrl), "video.mp4")
                else:
                    LOG.error("Save Remote file option %s unkown." % options.saveRemoteFile)
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

            print("Press CTRL+C to quit program.")
            while running:
                # TODO fix issue with blocking when CTRL + C was already pressed
                command = input("Enter command ('d' ... print diagnostics,\n" \
                                "               's' ... stop playback,\n" \
                                "               'r' ... resume playback,\n" \
                                "               CTRL + C to quit):\n")
                if command == "d":
                    widevineAdapter.Diagnostics()
                elif command == "s":
                    LOG.info("Stopping playback...")
                    widevineAdapter.StopPlayback()
                elif command == "r":
                    LOG.info("Resuming playback...")
                    widevineAdapter.ResumePlayback()
                else:
                    print("Unknown command: '%s'" % command)

            # blocking call
            signal.pause()  # this is import, else the signal does not get handled
            # blocking call
            widevineAdapter.WaitForStream()

            #
            # Clean up
            #
            widevineAdapter.Close()
            provider.close()
