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

BASE_URL = ***REMOVED***https://www.amazon.de***REMOVED***
LICENSE_SERVER = ***REMOVED***https://atv-ps-eu.amazon.de***REMOVED***
#FILMINFO_RES = ***REMOVED***AudioVideoUrls,CatalogMetadata,ForcedNarratives,SubtitlePresets,SubtitleUrls,TransitionTimecodes,***REMOVED*** \
#               ***REMOVED***TrickplayUrls,CuepointPlaylist,XRayMetadata,PlaybackSettings***REMOVED***
FILMINFO_RES = ***REMOVED***PlaybackUrls,CatalogMetadata,ForcedNarratives,SubtitlePresets,SubtitleUrls,TransitionTimecodes,TrickplayUrls,CuepointPlaylist,XRayMetadata,PlaybackSettings***REMOVED***
WIDEVINE2LICENSE_RES = ***REMOVED***Widevine2License***REMOVED***

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
        return ***REMOVED***[%s] asin=%s, title=%s, imgUrl=%s, synopsisText=%s, synopsisDict=%s***REMOVED*** % \
               (self.__class__.__name__, self.asin, self.title, self.imgUrl, self.synopsisText, self.synopsisDict)


class Amazon(object):
    def __init__(self):
        self.session = requests.Session()
        self.session.headers[***REMOVED***Accept***REMOVED***] = constants.DEFAULT_ACCEPT_HEADER
        self.session.headers[***REMOVED***Accept-Encoding***REMOVED***] = ***REMOVED***gzip, deflate***REMOVED***
        self.session.headers[***REMOVED***Accept-Language***REMOVED***] = ***REMOVED***de,en-US;q=0.8,en;q=0.6***REMOVED***
        self.session.headers[***REMOVED***Cache-Control***REMOVED***] = ***REMOVED***max-age=0***REMOVED***
        self.session.headers[***REMOVED***Connection***REMOVED***] = ***REMOVED***keep-alive***REMOVED***
        self.session.headers[***REMOVED***User-Agent***REMOVED***] = constants.USER_AGENT
        #self.session.headers[***REMOVED***Host***REMOVED***] = BASE_URL.split('//')[1]

    def close(self):
        self.session.close()

    def logIn(self):
        ***REMOVED******REMOVED******REMOVED***
        Login failed using mechanicalsoup and robobrowser.
        (for other mechanize alternatives see:
            https://stackoverflow.com/questions/2662705/are-there-any-alternatives-to-mechanize-in-python
        )

        So fall back to mechanize (however this only support Python 2.x)
        ***REMOVED******REMOVED******REMOVED***
        cj = LWPCookieJar()

        if not os.path.isfile(constants.COOKIE_FILE):
            email, pw = settingsManagerSingleton.getCredentialsForService(***REMOVED***amazon***REMOVED***)
            python2 = subprocess.check_output([***REMOVED***which***REMOVED***, ***REMOVED***python2***REMOVED***])
            python2 = python2.decode(***REMOVED***utf-8***REMOVED***).replace(***REMOVED***\n***REMOVED***, ***REMOVED******REMOVED***)
            LOG.info(***REMOVED***Using python interpreter %s***REMOVED*** % python2)
            ret = subprocess.call([python2, ***REMOVED***amazon-login.py***REMOVED***, email, pw])
            if ret != 0:
                raise ApplicationException(***REMOVED***Login failed.***REMOVED***)

        cj.load(constants.COOKIE_FILE, ignore_discard=True, ignore_expires=True)
        # pass LWPCookieJar directly to requests
        # see https://stackoverflow.com/a/16859266
        self.session.cookies = cj

    def getMpdDataAndExtractInfos(self, url):
        response = self.session.get(url)
        if response.status_code != 200:
            raise ApplicationException(***REMOVED***200 OK expected but got %d***REMOVED*** % response.status_code)
        mpdData = response.text
        soup = BeautifulSoup(mpdData, ***REMOVED***xml***REMOVED***)

        #
        # NOTE code taken from 'old' version
        #  TODO refactor, e.g. using marshmallow
        #
        # get 'base url'
        parsedUrl = urlparse(url)
        baseUrl = ***REMOVED******REMOVED***.join([parsedUrl.scheme, ***REMOVED***://***REMOVED***, parsedUrl.netloc, os.path.dirname(parsedUrl.path)])

        mpd_ = mpd.Mpd()
        adaptionSets = soup.find_all('AdaptationSet')
        for aSet in adaptionSets:
            adaptionSet = mpd.AdaptionSet()
            adaptionSet.contentType = aSet[***REMOVED***contentType***REMOVED***]
            adaptionSet.mimeType = aSet[***REMOVED***mimeType***REMOVED***]
            if adaptionSet.contentType == ***REMOVED***video***REMOVED***:
                adaptionSet.format = aSet[***REMOVED***par***REMOVED***]
            elif adaptionSet.contentType == ***REMOVED***audio***REMOVED***:
                adaptionSet.lang = aSet[***REMOVED***lang***REMOVED***]

            widevineProt = aSet.find('ContentProtection',
                                     attrs={'schemeIdUri': 'urn:uuid:EDEF8BA9-79D6-4ACE-A3C8-27DCD51D21ED'})
            if widevineProt:
                adaptionSet.widevineContentProtection = wvP = mpd.ContentProtection()
                wvP.schemeIdUri = widevineProt[***REMOVED***schemeIdUri***REMOVED***]
                wvP.cencPsshData = widevineProt.contents[0].renderContents().strip()

            playReadyProt = aSet.find('ContentProtection',
                                      attrs={'schemeIdUri': 'urn:uuid:9A04F079-9840-4286-AB92-E65BE0885F95'})
            if playReadyProt:
                adaptionSet.playReadyContentProtection = prP = mpd.ContentProtection()
                prP.schemeIdUri = playReadyProt[***REMOVED***schemeIdUri***REMOVED***]
                prP.cencPsshData = playReadyProt.contents[0].renderContents().strip()

            representations = aSet.find_all('Representation')
            for rep in representations:
                if rep.has_attr(***REMOVED***width***REMOVED***):
                    representation = mpd.VideoRepresentation()
                    representation.width = rep[***REMOVED***width***REMOVED***]
                    representation.height = rep[***REMOVED***height***REMOVED***]
                    representation.codecPrivateData = bytes.fromhex(rep[***REMOVED***codecPrivateData***REMOVED***])
                elif rep.has_attr(***REMOVED***audioSamplingRate***REMOVED***):
                    representation = mpd.AudioRepresentation()
                    representation.samplingRate = rep[***REMOVED***audioSamplingRate***REMOVED***]
                    representation.bandwidth = rep[***REMOVED***bandwidth***REMOVED***]
                else:
                    print(***REMOVED***Warning: Unknown Representation element found: {0}***REMOVED***.format(rep))
                    representation = mpd.Representation()

                representation.mediaFileUrl = ***REMOVED***{0}/{1}***REMOVED***.format(baseUrl,
                                                               rep.BaseURL.renderContents().strip().decode(***REMOVED***utf-8***REMOVED***))
                segmentList = rep.SegmentList
                segLst = mpd.SegmentList()
                segLst.duration = segmentList[***REMOVED***duration***REMOVED***]
                segLst.timescale = segmentList[***REMOVED***timescale***REMOVED***]
                segLst.initRange = segmentList.Initialization[***REMOVED***range***REMOVED***]
                for i in range(1, len(segmentList.contents)):
                    segLst.addMediaRange(segmentList.contents[i]['mediaRange'])
                representation.segmentList = segLst
                adaptionSet.addRepresentation(representation)

            mpd_.addAdaptionSet(adaptionSet)

        return mpd_

    def getWidevine2License(self, asin, widevine2Challenge):
        ***REMOVED******REMOVED******REMOVED***
        Request a Widevine License from the Amazon License server

        :param asin: Id of requested content
        :param widevine2Challenge: Widevine Challenge got from CDM already base64 encoded
        :return: License response from server
        ***REMOVED******REMOVED******REMOVED***
        params = self._getUrlParams()
        params[***REMOVED***asin***REMOVED***] = asin
        params[***REMOVED***desiredResources***REMOVED***] = WIDEVINE2LICENSE_RES
        params[***REMOVED***resourceUsage***REMOVED***] = ***REMOVED***ImmediateConsumption***REMOVED***
        params[***REMOVED***deviceDrmOverride***REMOVED***] = ***REMOVED***CENC***REMOVED***
        params[***REMOVED***deviceStreamingTechnologyOverride***REMOVED***] = ***REMOVED***DASH***REMOVED***

        postData = {
            ***REMOVED***widevine2Challenge***REMOVED***: widevine2Challenge,
            ***REMOVED***includeHdcpTestKeyInLicense***REMOVED***: True
        }

        url = LICENSE_SERVER + ***REMOVED***/cdp/catalog/GetPlaybackResources***REMOVED***
        r = self.session.post(url, params=params, data=postData)

        # LOG.info(***REMOVED***status: %s***REMOVED*** % r.status_code)
        # LOG.info(***REMOVED***json: %s***REMOVED*** % r.json())
        return r.json()

    def getFilmInfo(self, asin):
        ***REMOVED******REMOVED******REMOVED***
        Get infos about a film with given asin

        :param asin: Id of requested content
        :return:
        ***REMOVED******REMOVED******REMOVED***
        params = self._getUrlParams()
        params[***REMOVED***asin***REMOVED***] = asin
        params[***REMOVED***desiredResources***REMOVED***] = FILMINFO_RES
        params[***REMOVED***resourceUsage***REMOVED***] = ***REMOVED***CacheResources***REMOVED***
        params[***REMOVED***deviceDrmOverride***REMOVED***] = ***REMOVED***CENC***REMOVED***
        params[***REMOVED***deviceStreamingTechnologyOverride***REMOVED***] = ***REMOVED***DASH***REMOVED***
        params[***REMOVED***deviceProtocolOverride***REMOVED***] = ***REMOVED***Https***REMOVED***
        params[***REMOVED***supportedDRMKeyScheme***REMOVED***] = ***REMOVED***DUAL_KEY***REMOVED***
        params[***REMOVED***deviceBitrateAdaptationsOverride***REMOVED***] = ***REMOVED***CVBR,CBR***REMOVED***
        params[***REMOVED***titleDecorationScheme***REMOVED***] = ***REMOVED***primary-content***REMOVED***
        params[***REMOVED***subtitleFormat***REMOVED***] = ***REMOVED***TTMLv2***REMOVED***
        params[***REMOVED***languageFeature***REMOVED***] = ***REMOVED***MLFv2***REMOVED***
        params[***REMOVED***xrayDeviceClass***REMOVED***] = ***REMOVED***normal***REMOVED***
        params[***REMOVED***xrayPlaybackMode***REMOVED***] = ***REMOVED***playback***REMOVED***
        params[***REMOVED***xrayToken***REMOVED***] = ***REMOVED***INCEPTION_LITE_FILMO_V2***REMOVED***
        params[***REMOVED***playbackSettingsFormatVersion***REMOVED***] = ***REMOVED***1.0.0***REMOVED***

        # spoof this here; pretend to be Windows ;)
        params[***REMOVED***operatingSystemName***REMOVED***] = ***REMOVED***Windows***REMOVED***
        params[***REMOVED***operatingSystemVersion***REMOVED***] = ***REMOVED***10.0***REMOVED***

        url = LICENSE_SERVER + ***REMOVED***/cdp/catalog/GetPlaybackResources***REMOVED***
        r = self.session.post(url, params=params)

        # LOG.info(***REMOVED***status: %s***REMOVED*** % r.status_code)
        # LOG.info(***REMOVED***json: %s***REMOVED*** % r.json())
        data = r.json()

        # NOTE enable if debug required
        #LOG.info(***REMOVED******REMOVED***)
        #LOG.info(***REMOVED***Film info for film %s: ***REMOVED*** % asin)
        #LOG.info(json.dumps(data, sort_keys=True, indent=4, separators=(',', ': ')))
        #LOG.info(***REMOVED******REMOVED***)

        # parse interesting parts
        # - title, mpd url, video quality == HD (important!)
        title = data[***REMOVED***catalogMetadata***REMOVED***][***REMOVED***catalog***REMOVED***][***REMOVED***title***REMOVED***]
        urlSets = data[***REMOVED***playbackUrls***REMOVED***][***REMOVED***urlSets***REMOVED***]
        result = {}
        for key,val in urlSets.items():
            manifest = val[***REMOVED***urls***REMOVED***][***REMOVED***manifest***REMOVED***]
            videoQuality = manifest[***REMOVED***videoQuality***REMOVED***]
            result[***REMOVED***videoQuality***REMOVED***] = videoQuality
            result[***REMOVED***title***REMOVED***] = title
            result[***REMOVED***mpdUrl***REMOVED***] = manifest[***REMOVED***url***REMOVED***]
            if videoQuality == ***REMOVED***HD***REMOVED***:
                # choose first hd video quality
                break
        return result

    def getVideoPageDetailsMapping(self, pageUrl):
        ***REMOVED******REMOVED******REMOVED***
        Given some Amazon Movie/Series page url it collects
            * all episodes of some season of a series
            * for a movie only a single entry is returned

        :return:
        Result is a map from the title to an AmazonMovieEntry object.
        For a movie this map only contains a single entry.
        ***REMOVED******REMOVED******REMOVED***
        r = self.session.get(pageUrl)
        # LOG.info(***REMOVED***status: %s***REMOVED*** % r.status_code)

        content = r.text

        soup = BeautifulSoup(content, ***REMOVED***html.parser***REMOVED***)
        #print(soup.find_all(***REMOVED***div***REMOVED***))
        listEntries = soup.find_all(***REMOVED***div***REMOVED***, class_=***REMOVED***dv-episode-container***REMOVED***)
        print(listEntries)
        titleAmazonMovieEntryMap = {}
        for entry in listEntries:
            LOG.debug(entry)
            asin = entry[***REMOVED***data-aliases***REMOVED***]
            # if not asin:
            #    asin = entry.find(***REMOVED***div***REMOVED***, class_=***REMOVED***dv-play-button-radial***REMOVED***).span.a[***REMOVED***data-asin***REMOVED***]
            # LOG.debug(***REMOVED***asin=%s***REMOVED*** % asin)

            tmpLst = list(entry.find(***REMOVED***div***REMOVED***, class_=***REMOVED***dv-el-title***REMOVED***).children)
            title = tmpLst[len(tmpLst) - 1].strip()
            imgUrl = entry.find(***REMOVED***div***REMOVED***, class_=***REMOVED***dv-el-packshot-image***REMOVED***)[***REMOVED***style***REMOVED***].replace(***REMOVED***background-image: url(***REMOVED***, ***REMOVED******REMOVED***) \
                .replace(***REMOVED***);***REMOVED***, ***REMOVED******REMOVED***)
            synopsisDiv = entry.find(***REMOVED***div***REMOVED***, class_=***REMOVED***dv-el-synopsis-content***REMOVED***)
            synopsisText = list(synopsisDiv.find(***REMOVED***p***REMOVED***).children)[0]
            synopsisKeys = synopsisDiv.find_all(***REMOVED***span***REMOVED***, class_=***REMOVED***dv-el-attr-key***REMOVED***)
            synopsisValues = synopsisDiv.find_all(***REMOVED***span***REMOVED***, class_=***REMOVED***dv-el-attr-value***REMOVED***)
            synopsisDict = {}
            for keyDiv, valDiv in zip(synopsisKeys, synopsisValues):
                key = keyDiv.string.strip()
                val = valDiv.string.strip()
                synopsisDict[key] = val

            azMovieEntry = AmazonMovieEntry(asin=asin, title=title, imgUrl=imgUrl, synopsisText=synopsisText,
                                            synopsisDict=synopsisDict)
            # LOG.debug(***REMOVED******REMOVED***)
            titleAmazonMovieEntryMap[title] = azMovieEntry

        # try re fallback
        dataAliases = re.findall('<div class=***REMOVED***dv-episode-container aok-clearfix***REMOVED*** .* data-aliases=***REMOVED***(.*)***REMOVED***>', content)
        if dataAliases != None:
            titles = re.findall('<div class=***REMOVED***dv-el-title***REMOVED***> <!-- Title -->([^<>]*)</div> <!-- End Title-->', content)
            titles = [elem.strip() for elem in titles][:len(dataAliases)]
            #print(len(dataAliases))
            #print(len(titles))
            # not working
            #synopsis = re.findall('<div class=***REMOVED***dv-el-synopsis-wrapper***REMOVED***> <!-- Synopsis -->.*<div class=***REMOVED***dv-el-synopsis-content***REMOVED***>.*<p class=***REMOVED***a-color-base a-text-normal***REMOVED***>(.*)</p>.*</div> <!-- End Synopsis -->', content, flags=re.DOTALL)
            #print(synopsis)
            #print(len(synopsis))
            for i in range(0, len(dataAliases)):
                azMovieEntry = AmazonMovieEntry(asin=dataAliases[i], title=titles[i],
                    imgUrl=***REMOVED******REMOVED***, synopsisText=***REMOVED******REMOVED***, synopsisDict={})
                titleAmazonMovieEntryMap[titles[i]] = azMovieEntry

        if titleAmazonMovieEntryMap == {}:
            # if map is empty we have a movie which must be specially parsed
            synopsis = soup.find(***REMOVED***div***REMOVED***, class_=***REMOVED***dv-simple-synopsis***REMOVED***)
            if synopsis:
                strLst = [str(elem) for elem in synopsis.contents]
                synopsisText = ***REMOVED******REMOVED***.join(strLst).replace(***REMOVED***<p>***REMOVED***, ***REMOVED******REMOVED***).replace(***REMOVED***</p>***REMOVED***, ***REMOVED******REMOVED***).strip()
                # LOG.debug(synopsisText)

                metaInfo = soup.find(***REMOVED***dl***REMOVED***, class_=***REMOVED***dv-meta-info***REMOVED***)
                keys = metaInfo.find_all(***REMOVED***dt***REMOVED***)
                vals = metaInfo.find_all(***REMOVED***dd***REMOVED***)
                synopsisDict = {}
                for keyElem, valElem in zip(keys, vals):
                    key = keyElem.string.strip()
                    val = valElem.string.strip()
                    synopsisDict[key] = val

                watchListToggleForm = soup.find(***REMOVED***form***REMOVED***, class_=***REMOVED***dv-watchlist-toggle***REMOVED***)
                # LOG.debug(watchListToggleForm)
                # XXX this throws an exception
                # hiddenAsinInput = watchListToggleForm.find(***REMOVED***input***REMOVED***, name=***REMOVED***ASIN***REMOVED***)
                # asin = hiddenAsinInput[***REMOVED***value***REMOVED***]
                inputs = watchListToggleForm.find_all(***REMOVED***input***REMOVED***)
                asin = None
                for input_ in inputs:
                    # TODO find a better solution - querying just for the name did not work
                    #  --> use regex?
                    if len(input_[***REMOVED***value***REMOVED***]) == 10:
                        asin = input_[***REMOVED***value***REMOVED***]

                assert(asin is not None)

                titleH1 = soup.find(***REMOVED***h1***REMOVED***, id=***REMOVED***aiv-content-title***REMOVED***)
                # LOG.debug(***REMOVED***titleH1: %s***REMOVED*** % titleH1)
                title = list(titleH1.contents)[0].strip()

                iconContainerDic = soup.find(***REMOVED***div***REMOVED***, class_=***REMOVED***dp-meta-icon-container***REMOVED***)
                img = iconContainerDic.img
                imgUrl = img[***REMOVED***src***REMOVED***]

                azMovieEntry = AmazonMovieEntry(asin=asin, title=title, imgUrl=imgUrl,
                                                synopsisText=synopsisText, synopsisDict=synopsisDict)
                titleAmazonMovieEntryMap[title] = azMovieEntry

        return titleAmazonMovieEntryMap

    def queryWatchList(self):
        ***REMOVED******REMOVED******REMOVED***

        :return:
        ***REMOVED******REMOVED******REMOVED***
        # TODO
        pass

    def query(self, searchString):
        ***REMOVED******REMOVED******REMOVED***
        Generic search interface.

        :param searchString:
        :return:
        ***REMOVED******REMOVED******REMOVED***
        # TODO
        pass

    @staticmethod
    def gen_id():
        return hmac.new(constants.USER_AGENT.encode(***REMOVED***utf-8***REMOVED***), uuid.uuid4().bytes, hashlib.sha224).hexdigest()

    def _getUrlParams(self):
        url = BASE_URL + '/gp/deal/ajax/getNotifierResources.html'
        showpage = self.session.get(url).json()

        if not showpage:
            raise ApplicationException(***REMOVED***Error retrieving %s***REMOVED*** % url)

        values = {***REMOVED***deviceTypeID***REMOVED***: deviceTypeID,
                  ***REMOVED***videoMaterialType***REMOVED***: ***REMOVED***Feature***REMOVED***,
                  ***REMOVED***consumptionType***REMOVED***: ***REMOVED***Streaming***REMOVED***,
                  ***REMOVED***firmware***REMOVED***: 1,
                  ***REMOVED***gascEnabled***REMOVED***: ***REMOVED***false***REMOVED***,
                  ***REMOVED***operatingSystemName***REMOVED***: ***REMOVED***Linux***REMOVED***,
                  ***REMOVED***deviceID***REMOVED***: self.gen_id(),
                  ***REMOVED***userWatchSessionId***REMOVED***: str(uuid.uuid4())}

        customerData = showpage['resourceData']['GBCustomerData']
        if 'customerId' not in customerData or 'marketplaceId' not in customerData:
            LOG.debug(***REMOVED***GBCustomerData=%s***REMOVED*** % showpage['resourceData']['GBCustomerData'])
            raise ApplicationException(***REMOVED***Error retrieving customerId/marketplaceId via url %s.***REMOVED*** % url)
        if showpage['resourceData']['GBCustomerData'][***REMOVED***customerId***REMOVED***] == ***REMOVED******REMOVED***:
            LOG.error(***REMOVED***customerData: %s***REMOVED*** % showpage['resourceData']['GBCustomerData'])
            raise ApplicationException(***REMOVED***Login cookies seem to be invalid (empty customerId).***REMOVED***)

        values[***REMOVED***customerID***REMOVED***] = showpage['resourceData']['GBCustomerData'][***REMOVED***customerId***REMOVED***]
        values[***REMOVED***marketplaceID***REMOVED***] = showpage['resourceData']['GBCustomerData'][***REMOVED***marketplaceId***REMOVED***]

        rand = 'onWebToken_' + str(random.randint(0, 484))
        url = BASE_URL + ***REMOVED***/gp/video/streaming/player-token.json?callback=***REMOVED*** + rand
        pltoken = self.session.get(url).text
        # LOG.info(***REMOVED***pltoken: %s***REMOVED*** % pltoken)
        try:
            values['token'] = re.compile('***REMOVED***([^***REMOVED***]*).****REMOVED***([^***REMOVED***]*)***REMOVED***').findall(pltoken)[0][1]
        except IndexError:
            raise ApplicationException(***REMOVED***Error retrieving token via url %s.***REMOVED*** % url)

        return values


if __name__ == ***REMOVED***__main__***REMOVED***:
    import logging_config
    logging_config.configureLogging()

    def testGetFilmInfo():
        az = Amazon()
        az.logIn()

        asin = ***REMOVED***B06XGPDKXT***REMOVED***
        info = az.getFilmInfo(asin)
        LOG.info(***REMOVED******REMOVED***)
        LOG.info(***REMOVED***Film info for film %s: ***REMOVED*** % asin)
        LOG.info(json.dumps(info, sort_keys=True, indent=4, separators=(',', ': ')))
        LOG.info(***REMOVED******REMOVED***)

        LOG.info(***REMOVED***Printing some useful stuff:***REMOVED***)
        LOG.info(***REMOVED***Title: %s***REMOVED*** % info[***REMOVED***catalogMetadata***REMOVED***][***REMOVED***catalog***REMOVED***][***REMOVED***title***REMOVED***])
        LOG.info(***REMOVED***MPD url: %s***REMOVED*** % info['audioVideoUrls']['avCdnUrlSets'][0]['avUrlInfoList'][0]['url'])
        LOG.info(***REMOVED***  Video Quality: %s***REMOVED*** % info['audioVideoUrls']['avCdnUrlSets'][0]['avUrlInfoList'][0]['videoQuality'])

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
        MPD_URL = r***REMOVED***https://a430avoddashs3eu-a.akamaihd.net/d/2$lRrKoyFBKkMSscjrMhPbrBe4WTU~/ondemand/4c35/87c8/6d57/4a97-b002-420cd0c831ac/fda6fd6a-5d11-4f9f-810b-d60b7cc8a441_corrected.mpd***REMOVED***
        asin = ***REMOVED***B06XGPDKXT***REMOVED***

        mpd_ = az.getMpdDataAndExtractInfos(MPD_URL)
        cencInitData = buildWidevineInitDataString(mpd_)
        LOG.debug(***REMOVED***cenc pssh initdata: %s***REMOVED*** % binascii.hexlify(cencInitData).upper())

        # widevineAdapter stuff
        widevineAdapter.Init()
        widevineAdapter.CreateSessionAndGenerateRequest(cencInitData)

        # wait for cdm to process request
        time.sleep(3)

        challenge = widevineAdapter.GetSessionMessage()
        if not challenge:
            LOG.error(***REMOVED***Error retrieving challenge***REMOVED***)
            raise ApplicationException()
        LOG.info(***REMOVED***Got Widevine challenge (len: %d): %s***REMOVED*** %
                 (len(challenge), binascii.hexlify(challenge).upper()))
        b64EncodedChallenge = base64.b64encode(challenge).decode(***REMOVED***utf-8***REMOVED***)

        # send license request to server
        serverResponse = az.getWidevine2License(asin, b64EncodedChallenge)

        LOG.info(***REMOVED******REMOVED***)
        LOG.info(***REMOVED***Server response: ***REMOVED***)
        LOG.info(json.dumps(serverResponse, sort_keys=True, indent=4, separators=(',', ': ')))
        LOG.info(***REMOVED******REMOVED***)

        assert (***REMOVED***widevine2License***REMOVED*** in serverResponse)
        assert (***REMOVED***license***REMOVED*** in serverResponse[***REMOVED***widevine2License***REMOVED***])
        assert (***REMOVED***metadata***REMOVED*** in serverResponse[***REMOVED***widevine2License***REMOVED***])
        assert (***REMOVED***keyMetadata***REMOVED*** in serverResponse[***REMOVED***widevine2License***REMOVED***][***REMOVED***metadata***REMOVED***])

        license_ = base64.b64decode(serverResponse[***REMOVED***widevine2License***REMOVED***][***REMOVED***license***REMOVED***])
        LOG.info(***REMOVED***Received license (len: %d):\n %s***REMOVED*** % (
            len(license_), binascii.hexlify(license_).upper()
        ))

        widevineAdapter.UpdateCurrentSession(license_)
        assert (widevineAdapter.ValidKeyIdsSize() > 0)

        # NOTE: Actual video data processing omitted

        az.close()

    def getVideoPageDetailsMapping():
        url = ***REMOVED***https://www.amazon.de/gp/video/detail/B077QM313L/ref=dv_web_wtls_list_pr_1***REMOVED***
        az = Amazon()
        az.logIn()

        LOG.info(az.getVideoPageDetailsMapping(url))
        LOG.info(***REMOVED******REMOVED***)

        # url = ***REMOVED***https://www.amazon.de/gp/video/detail/B00TFWOFNC/ref=dv_web_wtls_list_pr_2***REMOVED***
        # LOG.info(az.getAsinsFromVideoPage(url))
        # LOG.info(***REMOVED******REMOVED***)

        url = ***REMOVED***https://www.amazon.de/gp/video/detail/B01MRCF2Y0/ref=dv_web_wtls_list_pr_2***REMOVED***
        LOG.info(az.getVideoPageDetailsMapping(url))

    # getVideoPageDetailsMapping()
