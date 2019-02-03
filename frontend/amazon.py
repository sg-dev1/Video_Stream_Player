import json
import requests
import logging
import uuid
import hmac
import hashlib
import random
import re
import os
import subprocess
from http.cookiejar import LWPCookieJar
from urllib.parse import urlparse
from bs4 import BeautifulSoup

import constants
import mpd
from util import settingsManagerSingleton, ApplicationException, buildWidevineInitDataString

BASE_URL = "https://www.amazon.de"
LICENSE_SERVER = "https://atv-ps-eu.amazon.de"
#FILMINFO_RES = "AudioVideoUrls,CatalogMetadata,ForcedNarratives,SubtitlePresets,SubtitleUrls,TransitionTimecodes," \
#               "TrickplayUrls,CuepointPlaylist,XRayMetadata,PlaybackSettings"
FILMINFO_RES = "PlaybackUrls,CatalogMetadata,ForcedNarratives,SubtitlePresets,SubtitleUrls,TransitionTimecodes,TrickplayUrls,CuepointPlaylist,XRayMetadata,PlaybackSettings"
WIDEVINE2LICENSE_RES = "Widevine2License"

# extracted from request made by firefox
deviceTypeID = 'AOAGZA014O5RE'

LOG = logging.getLogger(__name__)


class AmazonMovieEntry(object):
    def __init__(self, asin=None, title=None, imgUrl=None, synopsisText=None, synopsisDict={}):
        self.asin = asin
        self.title = title
        self.imgUrl = imgUrl
        self.synopsisText = synopsisText
        self.synopsisDict = synopsisDict

    def __repr__(self):
        return "[%s] asin=%s, title=%s, imgUrl=%s, synopsisText=%s, synopsisDict=%s" % \
               (self.__class__.__name__, self.asin, self.title, self.imgUrl, self.synopsisText, self.synopsisDict)


class Amazon(object):
    def __init__(self):
        self.session = requests.Session()
        self.session.headers["Accept"] = constants.DEFAULT_ACCEPT_HEADER
        self.session.headers["Accept-Encoding"] = "gzip, deflate"
        self.session.headers["Accept-Language"] = "de,en-US;q=0.8,en;q=0.6"
        self.session.headers["Cache-Control"] = "max-age=0"
        self.session.headers["Connection"] = "keep-alive"
        self.session.headers["User-Agent"] = constants.USER_AGENT
        #self.session.headers["Host"] = BASE_URL.split('//')[1]

    def close(self):
        self.session.close()

    def logIn(self):
        """
        Login failed using mechanicalsoup and robobrowser.
        (for other mechanize alternatives see:
            https://stackoverflow.com/questions/2662705/are-there-any-alternatives-to-mechanize-in-python
        )

        So fall back to mechanize (however this only support Python 2.x)
        """
        cj = LWPCookieJar()

        if not os.path.isfile(constants.COOKIE_FILE):
            email, pw = settingsManagerSingleton.getCredentialsForService("amazon")
            python2 = subprocess.check_output(["which", "python2"])
            python2 = python2.decode("utf-8").replace("\n", "")
            LOG.info("Using python interpreter %s" % python2)
            ret = subprocess.call([python2, "amazon-login.py", email, pw])
            if ret != 0:
                raise ApplicationException("Login failed.")

        cj.load(constants.COOKIE_FILE, ignore_discard=True, ignore_expires=True)
        # pass LWPCookieJar directly to requests
        # see https://stackoverflow.com/a/16859266
        self.session.cookies = cj

    def getMpdDataAndExtractInfos(self, url):
        response = self.session.get(url)
        if response.status_code != 200:
            raise ApplicationException("200 OK expected but got %d" % response.status_code)
        mpdData = response.text
        soup = BeautifulSoup(mpdData, "xml")

        #
        # NOTE code taken from 'old' version
        #  TODO refactor, e.g. using marshmallow
        #
        # get 'base url'
        parsedUrl = urlparse(url)
        baseUrl = "".join([parsedUrl.scheme, "://", parsedUrl.netloc, os.path.dirname(parsedUrl.path)])

        mpd_ = mpd.Mpd()
        adaptionSets = soup.find_all('AdaptationSet')
        for aSet in adaptionSets:
            adaptionSet = mpd.AdaptionSet()
            adaptionSet.contentType = aSet["contentType"]
            adaptionSet.mimeType = aSet["mimeType"]
            if adaptionSet.contentType == "video":
                adaptionSet.format = aSet["par"]
            elif adaptionSet.contentType == "audio":
                adaptionSet.lang = aSet["lang"]

            widevineProt = aSet.find('ContentProtection',
                                     attrs={'schemeIdUri': 'urn:uuid:EDEF8BA9-79D6-4ACE-A3C8-27DCD51D21ED'})
            if widevineProt:
                adaptionSet.widevineContentProtection = wvP = mpd.ContentProtection()
                wvP.schemeIdUri = widevineProt["schemeIdUri"]
                wvP.cencPsshData = widevineProt.contents[0].renderContents().strip()

            playReadyProt = aSet.find('ContentProtection',
                                      attrs={'schemeIdUri': 'urn:uuid:9A04F079-9840-4286-AB92-E65BE0885F95'})
            if playReadyProt:
                adaptionSet.playReadyContentProtection = prP = mpd.ContentProtection()
                prP.schemeIdUri = playReadyProt["schemeIdUri"]
                prP.cencPsshData = playReadyProt.contents[0].renderContents().strip()

            representations = aSet.find_all('Representation')
            for rep in representations:
                if rep.has_attr("width"):
                    representation = mpd.VideoRepresentation()
                    representation.width = rep["width"]
                    representation.height = rep["height"]
                    representation.codecPrivateData = bytes.fromhex(rep["codecPrivateData"])
                elif rep.has_attr("audioSamplingRate"):
                    representation = mpd.AudioRepresentation()
                    representation.samplingRate = rep["audioSamplingRate"]
                    representation.bandwidth = rep["bandwidth"]
                else:
                    print("Warning: Unknown Representation element found: {0}".format(rep))
                    representation = mpd.Representation()

                representation.mediaFileUrl = "{0}/{1}".format(baseUrl,
                                                               rep.BaseURL.renderContents().strip().decode("utf-8"))
                segmentList = rep.SegmentList
                segLst = mpd.SegmentList()
                segLst.duration = segmentList["duration"]
                segLst.timescale = segmentList["timescale"]
                segLst.initRange = segmentList.Initialization["range"]
                for i in range(1, len(segmentList.contents)):
                    segLst.addMediaRange(segmentList.contents[i]['mediaRange'])
                representation.segmentList = segLst
                adaptionSet.addRepresentation(representation)

            mpd_.addAdaptionSet(adaptionSet)

        return mpd_

    def getWidevine2License(self, asin, widevine2Challenge):
        """
        Request a Widevine License from the Amazon License server

        :param asin: Id of requested content
        :param widevine2Challenge: Widevine Challenge got from CDM already base64 encoded
        :return: License response from server
        """
        params = self._getUrlParams()
        params["asin"] = asin
        params["desiredResources"] = WIDEVINE2LICENSE_RES
        params["resourceUsage"] = "ImmediateConsumption"
        params["deviceDrmOverride"] = "CENC"
        params["deviceStreamingTechnologyOverride"] = "DASH"

        postData = {
            "widevine2Challenge": widevine2Challenge,
            "includeHdcpTestKeyInLicense": True
        }

        url = LICENSE_SERVER + "/cdp/catalog/GetPlaybackResources"
        r = self.session.post(url, params=params, data=postData)

        # LOG.info("status: %s" % r.status_code)
        # LOG.info("json: %s" % r.json())
        return r.json()

    def getFilmInfo(self, asin):
        """
        Get infos about a film with given asin

        :param asin: Id of requested content
        :return:
        """
        params = self._getUrlParams()
        params["asin"] = asin
        params["desiredResources"] = FILMINFO_RES
        params["resourceUsage"] = "CacheResources"
        params["deviceDrmOverride"] = "CENC"
        params["deviceStreamingTechnologyOverride"] = "DASH"
        params["deviceProtocolOverride"] = "Https"
        params["supportedDRMKeyScheme"] = "DUAL_KEY"
        params["deviceBitrateAdaptationsOverride"] = "CVBR,CBR"
        params["titleDecorationScheme"] = "primary-content"
        params["subtitleFormat"] = "TTMLv2"
        params["languageFeature"] = "MLFv2"
        params["xrayDeviceClass"] = "normal"
        params["xrayPlaybackMode"] = "playback"
        params["xrayToken"] = "INCEPTION_LITE_FILMO_V2"
        params["playbackSettingsFormatVersion"] = "1.0.0"

        # spoof this here; pretend to be Windows ;)
        params["operatingSystemName"] = "Windows"
        params["operatingSystemVersion"] = "10.0"

        url = LICENSE_SERVER + "/cdp/catalog/GetPlaybackResources"
        r = self.session.post(url, params=params)

        # LOG.info("status: %s" % r.status_code)
        # LOG.info("json: %s" % r.json())
        data = r.json()

        # NOTE enable if debug required
        #LOG.info("")
        #LOG.info("Film info for film %s: " % asin)
        #LOG.info(json.dumps(data, sort_keys=True, indent=4, separators=(',', ': ')))
        #LOG.info("")

        # parse interesting parts
        # - title, mpd url, video quality == HD (important!)
        title = data["catalogMetadata"]["catalog"]["title"]
        urlSets = data["playbackUrls"]["urlSets"]
        result = {}
        for key,val in urlSets.items():
            manifest = val["urls"]["manifest"]
            videoQuality = manifest["videoQuality"]
            result["videoQuality"] = videoQuality
            result["title"] = title
            result["mpdUrl"] = manifest["url"]
            if videoQuality == "HD":
                # choose first hd video quality
                break
        return result

    def getVideoPageDetailsMapping(self, pageUrl):
        """
        Given some Amazon Movie/Series page url it collects
            * all episodes of some season of a series
            * for a movie only a single entry is returned

        :return:
        Result is a map from the title to an AmazonMovieEntry object.
        For a movie this map only contains a single entry.
        """
        r = self.session.get(pageUrl)
        # LOG.info("status: %s" % r.status_code)

        content = r.text

        soup = BeautifulSoup(content, "html.parser")
        #print(soup.find_all("div"))
        listEntries = soup.find_all("div", class_="dv-episode-container")
        print(listEntries)
        titleAmazonMovieEntryMap = {}
        for entry in listEntries:
            LOG.debug(entry)
            asin = entry["data-aliases"]
            # if not asin:
            #    asin = entry.find("div", class_="dv-play-button-radial").span.a["data-asin"]
            # LOG.debug("asin=%s" % asin)

            tmpLst = list(entry.find("div", class_="dv-el-title").children)
            title = tmpLst[len(tmpLst) - 1].strip()
            imgUrl = entry.find("div", class_="dv-el-packshot-image")["style"].replace("background-image: url(", "") \
                .replace(");", "")
            synopsisDiv = entry.find("div", class_="dv-el-synopsis-content")
            synopsisText = list(synopsisDiv.find("p").children)[0]
            synopsisKeys = synopsisDiv.find_all("span", class_="dv-el-attr-key")
            synopsisValues = synopsisDiv.find_all("span", class_="dv-el-attr-value")
            synopsisDict = {}
            for keyDiv, valDiv in zip(synopsisKeys, synopsisValues):
                key = keyDiv.string.strip()
                val = valDiv.string.strip()
                synopsisDict[key] = val

            azMovieEntry = AmazonMovieEntry(asin=asin, title=title, imgUrl=imgUrl, synopsisText=synopsisText,
                                            synopsisDict=synopsisDict)
            # LOG.debug("")
            titleAmazonMovieEntryMap[title] = azMovieEntry

        # try re fallback
        dataAliases = re.findall('<div class="dv-episode-container aok-clearfix" .* data-aliases="(.*)">', content)
        if dataAliases != None:
            titles = re.findall('<div class="dv-el-title"> <!-- Title -->([^<>]*)</div> <!-- End Title-->', content)
            titles = [elem.strip() for elem in titles][:len(dataAliases)]
            #print(len(dataAliases))
            #print(len(titles))
            # not working
            #synopsis = re.findall('<div class="dv-el-synopsis-wrapper"> <!-- Synopsis -->.*<div class="dv-el-synopsis-content">.*<p class="a-color-base a-text-normal">(.*)</p>.*</div> <!-- End Synopsis -->', content, flags=re.DOTALL)
            #print(synopsis)
            #print(len(synopsis))
            for i in range(0, len(dataAliases)):
                azMovieEntry = AmazonMovieEntry(asin=dataAliases[i], title=titles[i],
                    imgUrl="", synopsisText="", synopsisDict={})
                titleAmazonMovieEntryMap[titles[i]] = azMovieEntry

        if titleAmazonMovieEntryMap == {}:
            # if map is empty we have a movie which must be specially parsed
            synopsis = soup.find("div", class_="dv-simple-synopsis")
            if synopsis:
                strLst = [str(elem) for elem in synopsis.contents]
                synopsisText = "".join(strLst).replace("<p>", "").replace("</p>", "").strip()
                # LOG.debug(synopsisText)

                metaInfo = soup.find("dl", class_="dv-meta-info")
                keys = metaInfo.find_all("dt")
                vals = metaInfo.find_all("dd")
                synopsisDict = {}
                for keyElem, valElem in zip(keys, vals):
                    key = keyElem.string.strip()
                    val = valElem.string.strip()
                    synopsisDict[key] = val

                watchListToggleForm = soup.find("form", class_="dv-watchlist-toggle")
                # LOG.debug(watchListToggleForm)
                # XXX this throws an exception
                # hiddenAsinInput = watchListToggleForm.find("input", name="ASIN")
                # asin = hiddenAsinInput["value"]
                inputs = watchListToggleForm.find_all("input")
                asin = None
                for input_ in inputs:
                    # TODO find a better solution - querying just for the name did not work
                    #  --> use regex?
                    if len(input_["value"]) == 10:
                        asin = input_["value"]

                assert(asin is not None)

                titleH1 = soup.find("h1", id="aiv-content-title")
                # LOG.debug("titleH1: %s" % titleH1)
                title = list(titleH1.contents)[0].strip()

                iconContainerDic = soup.find("div", class_="dp-meta-icon-container")
                img = iconContainerDic.img
                imgUrl = img["src"]

                azMovieEntry = AmazonMovieEntry(asin=asin, title=title, imgUrl=imgUrl,
                                                synopsisText=synopsisText, synopsisDict=synopsisDict)
                titleAmazonMovieEntryMap[title] = azMovieEntry

        return titleAmazonMovieEntryMap

    def queryWatchList(self):
        """

        :return:
        """
        # TODO
        pass

    def query(self, searchString):
        """
        Generic search interface.

        :param searchString:
        :return:
        """
        # TODO
        pass

    @staticmethod
    def gen_id():
        return hmac.new(constants.USER_AGENT.encode("utf-8"), uuid.uuid4().bytes, hashlib.sha224).hexdigest()

    def _getUrlParams(self):
        url = BASE_URL + '/gp/deal/ajax/getNotifierResources.html'
        showpage = self.session.get(url).json()

        if not showpage:
            raise ApplicationException("Error retrieving %s" % url)

        values = {"deviceTypeID": deviceTypeID,
                  "videoMaterialType": "Feature",
                  "consumptionType": "Streaming",
                  "firmware": 1,
                  "gascEnabled": "false",
                  "operatingSystemName": "Linux",
                  "deviceID": self.gen_id(),
                  "userWatchSessionId": str(uuid.uuid4())}

        customerData = showpage['resourceData']['GBCustomerData']
        if 'customerId' not in customerData or 'marketplaceId' not in customerData:
            LOG.debug("GBCustomerData=%s" % showpage['resourceData']['GBCustomerData'])
            raise ApplicationException("Error retrieving customerId/marketplaceId via url %s." % url)
        if showpage['resourceData']['GBCustomerData']["customerId"] == "":
            LOG.error("customerData: %s" % showpage['resourceData']['GBCustomerData'])
            raise ApplicationException("Login cookies seem to be invalid (empty customerId).")

        values["customerID"] = showpage['resourceData']['GBCustomerData']["customerId"]
        values["marketplaceID"] = showpage['resourceData']['GBCustomerData']["marketplaceId"]

        rand = 'onWebToken_' + str(random.randint(0, 484))
        url = BASE_URL + "/gp/video/streaming/player-token.json?callback=" + rand
        pltoken = self.session.get(url).text
        # LOG.info("pltoken: %s" % pltoken)
        try:
            values['token'] = re.compile('"([^"]*).*"([^"]*)"').findall(pltoken)[0][1]
        except IndexError:
            raise ApplicationException("Error retrieving token via url %s." % url)

        return values


if __name__ == "__main__":
    import logging_config
    logging_config.configureLogging()

    def testGetFilmInfo():
        az = Amazon()
        az.logIn()

        asin = "B06XGPDKXT"
        info = az.getFilmInfo(asin)
        LOG.info("")
        LOG.info("Film info for film %s: " % asin)
        LOG.info(json.dumps(info, sort_keys=True, indent=4, separators=(',', ': ')))
        LOG.info("")

        LOG.info("Printing some useful stuff:")
        LOG.info("Title: %s" % info["catalogMetadata"]["catalog"]["title"])
        LOG.info("MPD url: %s" % info['audioVideoUrls']['avCdnUrlSets'][0]['avUrlInfoList'][0]['url'])
        LOG.info("  Video Quality: %s" % info['audioVideoUrls']['avCdnUrlSets'][0]['avUrlInfoList'][0]['videoQuality'])

        az.close()

    testGetFilmInfo()

    def testLicenseRequest():
        import binascii
        import time
        import base64
        import widevineAdapter

        az = Amazon()
        az.logIn()

        # Amazon 'Burning Man' (Folge 6 - You Are Wanted)
        MPD_URL = r"https://a430avoddashs3eu-a.akamaihd.net/d/2$lRrKoyFBKkMSscjrMhPbrBe4WTU~/ondemand/4c35/87c8/6d57/4a97-b002-420cd0c831ac/fda6fd6a-5d11-4f9f-810b-d60b7cc8a441_corrected.mpd"
        asin = "B06XGPDKXT"

        mpd_ = az.getMpdDataAndExtractInfos(MPD_URL)
        cencInitData = buildWidevineInitDataString(mpd_)
        LOG.debug("cenc pssh initdata: %s" % binascii.hexlify(cencInitData).upper())

        # widevineAdapter stuff
        widevineAdapter.Init()
        widevineAdapter.CreateSessionAndGenerateRequest(cencInitData)

        # wait for cdm to process request
        time.sleep(3)

        challenge = widevineAdapter.GetSessionMessage()
        if not challenge:
            LOG.error("Error retrieving challenge")
            raise ApplicationException()
        LOG.info("Got Widevine challenge (len: %d): %s" %
                 (len(challenge), binascii.hexlify(challenge).upper()))
        b64EncodedChallenge = base64.b64encode(challenge).decode("utf-8")

        # send license request to server
        serverResponse = az.getWidevine2License(asin, b64EncodedChallenge)

        LOG.info("")
        LOG.info("Server response: ")
        LOG.info(json.dumps(serverResponse, sort_keys=True, indent=4, separators=(',', ': ')))
        LOG.info("")

        assert ("widevine2License" in serverResponse)
        assert ("license" in serverResponse["widevine2License"])
        assert ("metadata" in serverResponse["widevine2License"])
        assert ("keyMetadata" in serverResponse["widevine2License"]["metadata"])

        license_ = base64.b64decode(serverResponse["widevine2License"]["license"])
        LOG.info("Received license (len: %d):\n %s" % (
            len(license_), binascii.hexlify(license_).upper()
        ))

        widevineAdapter.UpdateCurrentSession(license_)
        assert (widevineAdapter.ValidKeyIdsSize() > 0)

        # NOTE: Actual video data processing omitted

        az.close()

    def getVideoPageDetailsMapping():
        url = "https://www.amazon.de/gp/video/detail/B077QM313L/ref=dv_web_wtls_list_pr_1"
        az = Amazon()
        az.logIn()

        LOG.info(az.getVideoPageDetailsMapping(url))
        LOG.info("")

        # url = "https://www.amazon.de/gp/video/detail/B00TFWOFNC/ref=dv_web_wtls_list_pr_2"
        # LOG.info(az.getAsinsFromVideoPage(url))
        # LOG.info("")

        url = "https://www.amazon.de/gp/video/detail/B01MRCF2Y0/ref=dv_web_wtls_list_pr_2"
        LOG.info(az.getVideoPageDetailsMapping(url))

    # getVideoPageDetailsMapping()
