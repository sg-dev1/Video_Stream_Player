import base64
import logging
import os
import json
import uuid
from platform import node
from getpass import getpass

# pyCrypto
from Crypto.Cipher import AES
import requests

import mpd
from constants import SETTINGS_FILE, USER_AGENT, STREAM_TYPE_AUDIO, STREAM_TYPE_VIDEO

LOG = logging.getLogger(__name__)

headers = {
    'Accept': 'application/octet-stream',
    'Accept-Encoding': 'gzip, deflate',
    'Accept-Language': 'de,en-US;q=0.8,en;q=0.6',
    'Cache-Control': 'max-age=0',
    'Connection': 'keep-alive',
    'User-Agent': USER_AGENT
}


def downloadByteRange(url, _range):
    LOG.info(***REMOVED***Trying to download from %s byterange %s***REMOVED*** % (url, _range))
    headers[***REMOVED***Range***REMOVED***] = ***REMOVED***bytes={0}***REMOVED***.format(_range)
    response = requests.get(url, headers=headers)
    if not (response.status_code == 200 or response.status_code == 206):
        msg = ***REMOVED***Status must be 200 OK or 206 Partial Content but was %s***REMOVED*** % response.status_code
        LOG.error(msg)
        raise ApplicationException(msg)
    return response.content


def downloadFile(url):
    LOG.info(***REMOVED***Trying to download from %s***REMOVED*** % url)
    response = requests.get(url, headers=headers)
    if not response.status_code == 200:
        msg = ***REMOVED***Status must be 200 OK or 206 Partial Content but was %s***REMOVED*** % response.status_code
        LOG.error(msg)
        raise ApplicationException(msg)
    return response.content


def saveDataIntoFile(data, file_):
    with open(file_, ***REMOVED***wb***REMOVED***) as f:
        f.write(data)


def _fetchAudioAndVideoAdaptionSets(mpd_):
    audioAdaptionSetLst = []
    videoAdaptionSetLst = []
    for elem in mpd_.adaptionSetLst:
        if elem.contentType == ***REMOVED***audio***REMOVED***:
            audioAdaptionSetLst.append(elem)
        else:
            # video
            videoAdaptionSetLst.append(elem)

    if len(audioAdaptionSetLst) == 0:
        raise ApplicationException(***REMOVED***No audio adaption set found***REMOVED***)
    if len(videoAdaptionSetLst) == 0:
        raise ApplicationException(***REMOVED***No video adaption set found***REMOVED***)

    return audioAdaptionSetLst, videoAdaptionSetLst


def selectVideoAndAudio(mpd_, width, height, audioAdaptionSetIndex=-1, audioIndex=-1, videoIndex=-1):
    audioAdaptionSetLst, videoAdaptionSetLst = _fetchAudioAndVideoAdaptionSets(mpd_)

    # video stuff
    selectedVideoAdaptionSet = videoAdaptionSetLst[0]
    if videoIndex == -1:
        possibleRepresentations = []
        for rep in selectedVideoAdaptionSet.representationLst:
            if rep.width == width and rep.height == height:
                possibleRepresentations.append(rep)
        numPossibleRepresentations = len(possibleRepresentations)
        if numPossibleRepresentations <= 0:
            LOG.error(***REMOVED***No possible video representation found with video format %d:%d***REMOVED*** % (width, height))
            raise ApplicationException(***REMOVED***No possible video representation found!***REMOVED***)

        selectedVideoRepresentation = possibleRepresentations[numPossibleRepresentations - 1]
    else:
        numRepresentations = len(selectedVideoAdaptionSet.representationLst)
        if videoIndex <= 0:
            # default
            videoIndex = 0
        else:
            if videoIndex > numRepresentations - 1:
                videoIndex = numRepresentations - 1
        LOG.info(***REMOVED***Using video representation # %d of adaption set # 0***REMOVED*** % videoIndex)
        selectedVideoRepresentation = selectedVideoAdaptionSet.representationLst[videoIndex]

    # audio stuff
    if audioAdaptionSetIndex == -1:
        audioAdaptionSet = None
        for elem in mpd_.adaptionSetLst:
            if elem.contentType == ***REMOVED***audio***REMOVED***:
                audioAdaptionSet = elem
                break
        if audioAdaptionSet is None:
            raise ApplicationException(***REMOVED***No possible audio representation found***REMOVED***)
        selectedAudioRepresentation = audioAdaptionSet.representationLst[0]
    else:
        numAudioAdaptionSets = len(audioAdaptionSetLst)
        if audioAdaptionSetIndex <= 0:
            audioAdaptionSetIndex = 0
        else:
            if audioAdaptionSetIndex > numAudioAdaptionSets - 1:
                audioAdaptionSetIndex = numAudioAdaptionSets - 1

        selectedAudioAdaptionSet = audioAdaptionSetLst[audioAdaptionSetIndex]
        numRepresentations = len(selectedAudioAdaptionSet.representationLst)
        if audioIndex <= 0:
            audioIndex = 0
        else:
            if audioIndex > numRepresentations - 1:
                audioIndex = numRepresentations - 1
        LOG.info(***REMOVED***Using audio representation # %d of adaption set # %d***REMOVED*** % (audioIndex, audioAdaptionSetIndex))
        selectedAudioRepresentation = selectedAudioAdaptionSet.representationLst[audioIndex]

    LOG.info(***REMOVED***Selected audio: %s***REMOVED*** % selectedAudioRepresentation)
    LOG.info(***REMOVED***Selected video: %s***REMOVED*** % selectedVideoRepresentation)
    return selectedVideoRepresentation, selectedAudioRepresentation


def printResolutions(mpd_):
    audioAdaptionSetLst, videoAdaptionSetLst = _fetchAudioAndVideoAdaptionSets(mpd_)

    idx = 0
    for adaptionSet in videoAdaptionSetLst:
        LOG.info(***REMOVED***----------------------------------***REMOVED***)
        LOG.info(***REMOVED***Video adaption set # %d***REMOVED*** % idx)
        repIdx = 0
        for rep in adaptionSet.representationLst:
            LOG.info(***REMOVED***\tRepresentation # %d: %s***REMOVED*** % (repIdx, rep))
            repIdx = repIdx + 1
        idx = idx + 1

    LOG.info(***REMOVED******REMOVED***)
    LOG.info(***REMOVED***--------------------------------------------------------------------***REMOVED***)
    LOG.info(***REMOVED******REMOVED***)

    idx = 0
    for adaptionSet in audioAdaptionSetLst:
        LOG.info(***REMOVED***Audio adaption set # %d***REMOVED*** % idx)
        repIdx = 0
        for rep in adaptionSet.representationLst:
            LOG.info(***REMOVED***\tRepresentation # %d: %s***REMOVED*** % (repIdx, rep))
            repIdx = repIdx + 1
        LOG.info(***REMOVED***----------------------------------***REMOVED***)
        idx = idx + 1


class SettingsManager(object):
    def __init__(self):
        if not os.path.isfile(SETTINGS_FILE):
            LOG.warning(***REMOVED***No settings file found.***REMOVED***)
            self._settings = {}
        else:
            with open(SETTINGS_FILE, ***REMOVED***r***REMOVED***) as f:
                self._settings = json.load(f)

    def save(self):
        with open(SETTINGS_FILE, ***REMOVED***w***REMOVED***) as f:
            json.dump(self._settings, f)

    def getCredentialsForService(self, serviceId):
        loginKey = ***REMOVED***%s.login***REMOVED*** % serviceId
        pwKey = ***REMOVED***%s.password***REMOVED*** % serviceId
        if loginKey in self._settings and pwKey in self._settings:
            return self._settings[loginKey], decode(self._settings[pwKey])

        return self._askForCredentials(serviceId)

    def _askForCredentials(self, serviceId):
        ***REMOVED******REMOVED******REMOVED***
        Asks the user for his credentials for a certain service.
        This also serializes the credentials into the settings file.

        :param serviceId: A string, e.g. 'amazon'
        :return: loginName, password tuple
        ***REMOVED******REMOVED******REMOVED***
        LOG.info(***REMOVED***Provide your credentials for service %s***REMOVED*** % serviceId)
        login = input(***REMOVED***Login name: ***REMOVED***)
        password = getpass(***REMOVED***Password: ***REMOVED***)
        if not login or not password:
            raise ApplicationException(***REMOVED***ERROR: Email and password must not be empty***REMOVED***)
        self._settings[***REMOVED***%s.login***REMOVED*** % serviceId] = login
        self._settings[***REMOVED***%s.password***REMOVED*** % serviceId] = encode(password)
        self.save()

        return login, password


def encode(data):
    key = getMac()
    cipher = AES.new(key, AES.MODE_CBC, ***REMOVED***\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0***REMOVED***)
    mod_len = len(data) % 16
    pad_len = 0
    if mod_len:
        pad_len = 16 - mod_len
    data = ***REMOVED******REMOVED***.join([data, ***REMOVED*** ***REMOVED*** * pad_len]) # assumes that no password has spaces in the end
    encData = cipher.encrypt(data)
    return base64.b64encode(encData).decode(***REMOVED***utf-8***REMOVED***)


def decode(data):
    if not data:
        return ***REMOVED******REMOVED***
    key = getMac()
    cipher = AES.new(key, AES.MODE_CBC, ***REMOVED***\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0***REMOVED***)
    data = base64.b64decode(data)
    return cipher.decrypt(data).decode(***REMOVED***utf-8***REMOVED***).strip()


# must be deterministic
def getMac():
    mac = uuid.getnode()
    if (mac >> 40) % 2:
        mac = node()
    return uuid.uuid5(uuid.NAMESPACE_DNS, str(mac)).bytes


def buildWidevineInitDataString(mpdObject, streamType=STREAM_TYPE_VIDEO, adaptionSet=0, cencInitDataWv=None, cencInitDataPr=None):
    ***REMOVED******REMOVED******REMOVED***
    Build a whole widevine init data string (CENC PSSH)
    ready to pass to widevineAdapter.CreateSessionAndGenerateRequest().
    (as it was observed by firefox)

    :param mpdObject: The mpd object parsed from the mpd.xml file,
           if cencInitDataWv and cencInitDataPr are given this can be None
    :param contentType: Either 'audio' or 'video'
    :param adaptionSet: Number of adaption set in list, 0 ist the first one
    :param cencInitDataWv: Widevine CENC PSSH data (base64 encoded)
    :param cencInitDataPr: Playready CENC PSSH data (base64 encoded)
    :return: A binary buffer containing the CENC PSSH init data
    ***REMOVED******REMOVED******REMOVED***
    if adaptionSet < 0:
        adaptionSet = 0

    if streamType == STREAM_TYPE_VIDEO:
        contentType = ***REMOVED***video***REMOVED***
    elif streamType == STREAM_TYPE_AUDIO:
        contentType = ***REMOVED***audio***REMOVED***
    else:
        raise Exception(***REMOVED***Wrong stream type %s given.***REMOVED*** % str(STREAM_TYPE))

    if not cencInitDataWv:
        # cencInitDataWv = mpdObject.getFirstWidevineContentProt().cencPsshData
        cencInitDataWv = mpdObject.getWidevineContentProt(contentType, adaptionSet).cencPsshData
    if not cencInitDataPr:
        # cencInitDataPr = mpdObject.getFirstPlayReadyContentProt().cencPsshData
        cencInitDataPr = mpdObject.getPlayReadyContentProt(contentType, adaptionSet).cencPsshData

    if not cencInitDataPr or not cencInitDataWv:
        raise ApplicationException(***REMOVED***Either widevine or playready init data not found***REMOVED***)

    buf = []
    cencInitDataPrDecoded = base64.b64decode(cencInitDataPr)
    prInitDataLen = len(cencInitDataPrDecoded)
    buf.append(bytes.fromhex(***REMOVED***{:08X}***REMOVED***.format(prInitDataLen + 32)))
    buf.append(b***REMOVED***pssh***REMOVED***)
    buf.append(bytes.fromhex(***REMOVED***00000000***REMOVED***))
    buf.append(bytes.fromhex(***REMOVED***9A04F07998404286AB92E65BE0885F95***REMOVED***))
    buf.append(bytes.fromhex(***REMOVED***{:08X}***REMOVED***.format(prInitDataLen)))
    buf.append(cencInitDataPrDecoded)

    cencInitDataWvDecoded = base64.b64decode(cencInitDataWv)
    wvInitDatalen = len(cencInitDataWvDecoded)
    buf.append(bytes.fromhex(***REMOVED***{:08X}***REMOVED***.format(wvInitDatalen + 32)))
    buf.append(b***REMOVED***pssh***REMOVED***)
    buf.append(bytes.fromhex(***REMOVED***00000000***REMOVED***))
    buf.append(bytes.fromhex(***REMOVED***EDEF8BA979D64ACEA3C827DCD51D21ED***REMOVED***))
    buf.append(bytes.fromhex(***REMOVED***{:08X}***REMOVED***.format(wvInitDatalen)))
    buf.append(cencInitDataWvDecoded)

    return b***REMOVED******REMOVED***.join(buf)


class ApplicationException(Exception):
    def __init__(self, message):
        super().__init__()
        self.message = message


# singleton instances
settingsManagerSingleton = SettingsManager()
