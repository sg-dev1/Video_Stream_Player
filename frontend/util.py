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
    LOG.info("Trying to download from %s byterange %s" % (url, _range))
    headers["Range"] = "bytes={0}".format(_range)
    response = requests.get(url, headers=headers)
    if not (response.status_code == 200 or response.status_code == 206):
        msg = "Status must be 200 OK or 206 Partial Content but was %s" % response.status_code
        LOG.error(msg)
        raise ApplicationException(msg)
    return response.content


def downloadFile(url):
    LOG.info("Trying to download from %s" % url)
    response = requests.get(url, headers=headers)
    if not response.status_code == 200:
        msg = "Status must be 200 OK or 206 Partial Content but was %s" % response.status_code
        LOG.error(msg)
        raise ApplicationException(msg)
    return response.content


def saveDataIntoFile(data, file_):
    with open(file_, "wb") as f:
        f.write(data)


def _fetchAudioAndVideoAdaptionSets(mpd_):
    audioAdaptionSetLst = []
    videoAdaptionSetLst = []
    for elem in mpd_.adaptionSetLst:
        if elem.contentType == "audio":
            audioAdaptionSetLst.append(elem)
        else:
            # video
            videoAdaptionSetLst.append(elem)

    if len(audioAdaptionSetLst) == 0:
        raise ApplicationException("No audio adaption set found")
    if len(videoAdaptionSetLst) == 0:
        raise ApplicationException("No video adaption set found")

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
            LOG.error("No possible video representation found with video format %d:%d" % (width, height))
            raise ApplicationException("No possible video representation found!")

        selectedVideoRepresentation = possibleRepresentations[numPossibleRepresentations - 1]
    else:
        numRepresentations = len(selectedVideoAdaptionSet.representationLst)
        if videoIndex <= 0:
            # default
            videoIndex = 0
        else:
            if videoIndex > numRepresentations - 1:
                videoIndex = numRepresentations - 1
        LOG.info("Using video representation # %d of adaption set # 0" % videoIndex)
        selectedVideoRepresentation = selectedVideoAdaptionSet.representationLst[videoIndex]

    # audio stuff
    if audioAdaptionSetIndex == -1:
        audioAdaptionSet = None
        for elem in mpd_.adaptionSetLst:
            if elem.contentType == "audio":
                audioAdaptionSet = elem
                break
        if audioAdaptionSet is None:
            raise ApplicationException("No possible audio representation found")
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
        LOG.info("Using audio representation # %d of adaption set # %d" % (audioIndex, audioAdaptionSetIndex))
        selectedAudioRepresentation = selectedAudioAdaptionSet.representationLst[audioIndex]

    LOG.info("Selected audio: %s" % selectedAudioRepresentation)
    LOG.info("Selected video: %s" % selectedVideoRepresentation)
    return selectedVideoRepresentation, selectedAudioRepresentation


def printResolutions(mpd_):
    audioAdaptionSetLst, videoAdaptionSetLst = _fetchAudioAndVideoAdaptionSets(mpd_)

    idx = 0
    for adaptionSet in videoAdaptionSetLst:
        LOG.info("----------------------------------")
        LOG.info("Video adaption set # %d" % idx)
        repIdx = 0
        for rep in adaptionSet.representationLst:
            LOG.info("\tRepresentation # %d: %s" % (repIdx, rep))
            repIdx = repIdx + 1
        idx = idx + 1

    LOG.info("")
    LOG.info("--------------------------------------------------------------------")
    LOG.info("")

    idx = 0
    for adaptionSet in audioAdaptionSetLst:
        LOG.info("Audio adaption set # %d" % idx)
        repIdx = 0
        for rep in adaptionSet.representationLst:
            LOG.info("\tRepresentation # %d: %s" % (repIdx, rep))
            repIdx = repIdx + 1
        LOG.info("----------------------------------")
        idx = idx + 1


class SettingsManager(object):
    def __init__(self):
        if not os.path.isfile(SETTINGS_FILE):
            LOG.warning("No settings file found.")
            self._settings = {}
        else:
            with open(SETTINGS_FILE, "r") as f:
                self._settings = json.load(f)

    def save(self):
        with open(SETTINGS_FILE, "w") as f:
            json.dump(self._settings, f)

    def getCredentialsForService(self, serviceId):
        loginKey = "%s.login" % serviceId
        pwKey = "%s.password" % serviceId
        if loginKey in self._settings and pwKey in self._settings:
            return self._settings[loginKey], decode(self._settings[pwKey])

        return self._askForCredentials(serviceId)

    def _askForCredentials(self, serviceId):
        """
        Asks the user for his credentials for a certain service.
        This also serializes the credentials into the settings file.

        :param serviceId: A string, e.g. 'amazon'
        :return: loginName, password tuple
        """
        LOG.info("Provide your credentials for service %s" % serviceId)
        login = input("Login name: ")
        password = getpass("Password: ")
        if not login or not password:
            raise ApplicationException("ERROR: Email and password must not be empty")
        self._settings["%s.login" % serviceId] = login
        self._settings["%s.password" % serviceId] = encode(password)
        self.save()

        return login, password


def encode(data):
    key = getMac()
    cipher = AES.new(key, AES.MODE_CBC, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0")
    mod_len = len(data) % 16
    pad_len = 0
    if mod_len:
        pad_len = 16 - mod_len
    data = "".join([data, " " * pad_len]) # assumes that no password has spaces in the end
    encData = cipher.encrypt(data)
    return base64.b64encode(encData).decode("utf-8")


def decode(data):
    if not data:
        return ""
    key = getMac()
    cipher = AES.new(key, AES.MODE_CBC, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0")
    data = base64.b64decode(data)
    return cipher.decrypt(data).decode("utf-8").strip()


# must be deterministic
def getMac():
    mac = uuid.getnode()
    if (mac >> 40) % 2:
        mac = node()
    return uuid.uuid5(uuid.NAMESPACE_DNS, str(mac)).bytes


def buildWidevineInitDataString(mpdObject, streamType=STREAM_TYPE_VIDEO, adaptionSet=0, cencInitDataWv=None, cencInitDataPr=None):
    """
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
    """
    if adaptionSet < 0:
        adaptionSet = 0

    if streamType == STREAM_TYPE_VIDEO:
        contentType = "video"
    elif streamType == STREAM_TYPE_AUDIO:
        contentType = "audio"
    else:
        raise Exception("Wrong stream type %s given." % str(STREAM_TYPE))

    if not cencInitDataWv:
        # cencInitDataWv = mpdObject.getFirstWidevineContentProt().cencPsshData
        cencInitDataWv = mpdObject.getWidevineContentProt(contentType, adaptionSet).cencPsshData
    if not cencInitDataPr:
        # cencInitDataPr = mpdObject.getFirstPlayReadyContentProt().cencPsshData
        cencInitDataPr = mpdObject.getPlayReadyContentProt(contentType, adaptionSet).cencPsshData

    if not cencInitDataPr or not cencInitDataWv:
        raise ApplicationException("Either widevine or playready init data not found")

    buf = []
    cencInitDataPrDecoded = base64.b64decode(cencInitDataPr)
    prInitDataLen = len(cencInitDataPrDecoded)
    buf.append(bytes.fromhex("{:08X}".format(prInitDataLen + 32)))
    buf.append(b"pssh")
    buf.append(bytes.fromhex("00000000"))
    buf.append(bytes.fromhex("9A04F07998404286AB92E65BE0885F95"))
    buf.append(bytes.fromhex("{:08X}".format(prInitDataLen)))
    buf.append(cencInitDataPrDecoded)

    cencInitDataWvDecoded = base64.b64decode(cencInitDataWv)
    wvInitDatalen = len(cencInitDataWvDecoded)
    buf.append(bytes.fromhex("{:08X}".format(wvInitDatalen + 32)))
    buf.append(b"pssh")
    buf.append(bytes.fromhex("00000000"))
    buf.append(bytes.fromhex("EDEF8BA979D64ACEA3C827DCD51D21ED"))
    buf.append(bytes.fromhex("{:08X}".format(wvInitDatalen)))
    buf.append(cencInitDataWvDecoded)

    return b"".join(buf)


class ApplicationException(Exception):
    def __init__(self, message):
        super().__init__()
        self.message = message


# singleton instances
settingsManagerSingleton = SettingsManager()
