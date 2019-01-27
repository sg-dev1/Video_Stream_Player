import binascii
import time
import base64
import json
import logging

import widevineAdapter
import amazon
import util

LOG = logging.getLogger(__name__)



class ContentProvider(object):
    def __init__(self, provider=None):
        if provider:
            self.provider = provider
        else:
            self.provider = amazon.Amazon()

    def connect(self):
        ***REMOVED******REMOVED******REMOVED***
        Connects to the provider (e.g. log in)
        ***REMOVED******REMOVED******REMOVED***
        self.provider.logIn()

    def close(self):
        ***REMOVED******REMOVED******REMOVED***
        Close the provider
        ***REMOVED******REMOVED******REMOVED***
        self.provider.close()
        widevineAdapter.Close()

    def requestLicenseForEstablishedSession(self, resId, cencInitData):
        ***REMOVED******REMOVED******REMOVED***
        Same as requestLicense, but does not call widevineAdapter.Init()
        ***REMOVED******REMOVED******REMOVED***
        widevineAdapter.CreateSessionAndGenerateRequest(cencInitData)

        # wait for cdm to process request
        time.sleep(3)

        challenge = widevineAdapter.GetSessionMessage()
        if not challenge:
            LOG.error(***REMOVED***Error retrieving challenge for contentType %s***REMOVED*** % contentType)
            raise util.ApplicationException()
        b64EncodedChallenge = base64.b64encode(challenge).decode(***REMOVED***utf-8***REMOVED***)
        serverResponse = self.provider.getWidevine2License(resId, b64EncodedChallenge)

        # LOG.info(***REMOVED******REMOVED***)
        # LOG.info(***REMOVED***Server response: ***REMOVED***)
        # LOG.info(json.dumps(serverResponse, sort_keys=True, indent=4, separators=(',', ': ')))
        # LOG.info(***REMOVED******REMOVED***)

        assert (***REMOVED***widevine2License***REMOVED*** in serverResponse)
        assert (***REMOVED***license***REMOVED*** in serverResponse[***REMOVED***widevine2License***REMOVED***])
        assert (***REMOVED***metadata***REMOVED*** in serverResponse[***REMOVED***widevine2License***REMOVED***])
        assert (***REMOVED***keyMetadata***REMOVED*** in serverResponse[***REMOVED***widevine2License***REMOVED***][***REMOVED***metadata***REMOVED***])

        license_ = base64.b64decode(serverResponse[***REMOVED***widevine2License***REMOVED***][***REMOVED***license***REMOVED***])
        widevineAdapter.UpdateCurrentSession(license_)

        time.sleep(3)
        # assert(False)


    def requestLicense(self, resId, mpdUrl, streamType, adaptionSet=0, mpdObject=None):
        ***REMOVED******REMOVED******REMOVED***
        Requests a new License from the provider

        :param resId: The id of the resource where a license should be requested (e.g. asin for Amazon)
        :param mpdUrl: The url of the mpd file
        :param streamType: The stream type, one of constants.STREAM_TYPE_AUDIO or constants.STREAM_TYPE_VIDEO
        :param adaptionSet: Number of adaption set in list, 0 ist the first one
        :return: The parsed MPD object for further usage (e.g. selecting the stream, getting the media file locations)
        ***REMOVED******REMOVED******REMOVED***
        if mpdObject:
            mpd_ = mpdObject
        else:
            mpd_ = self.provider.getMpdDataAndExtractInfos(mpdUrl)
        cencInitData = util.buildWidevineInitDataString(mpd_, streamType, adaptionSet)
        # LOG.debug(***REMOVED***cenc pssh initdata: %s***REMOVED*** % binascii.hexlify(cencInitData).upper())

        # widevineAdapter stuff
        widevineAdapter.Init()
        widevineAdapter.CreateSessionAndGenerateRequest(cencInitData, streamType)

        # wait for cdm to process request
        time.sleep(3)

        challenge = widevineAdapter.GetSessionMessage(streamType)
        if not challenge:
            LOG.error(***REMOVED***Error retrieving challenge***REMOVED***)
            raise util.ApplicationException()
        # LOG.info(***REMOVED***Got Widevine challenge (len: %d): %s***REMOVED*** %
        #         (len(challenge), binascii.hexlify(challenge).upper()))
        b64EncodedChallenge = base64.b64encode(challenge).decode(***REMOVED***utf-8***REMOVED***)

        # send license request to server
        serverResponse = self.provider.getWidevine2License(resId, b64EncodedChallenge)

        # LOG.info(***REMOVED******REMOVED***)
        # LOG.info(***REMOVED***Server response: ***REMOVED***)
        # LOG.info(json.dumps(serverResponse, sort_keys=True, indent=4, separators=(',', ': ')))
        # LOG.info(***REMOVED******REMOVED***)

        assert (***REMOVED***widevine2License***REMOVED*** in serverResponse)
        assert (***REMOVED***license***REMOVED*** in serverResponse[***REMOVED***widevine2License***REMOVED***])
        assert (***REMOVED***metadata***REMOVED*** in serverResponse[***REMOVED***widevine2License***REMOVED***])
        assert (***REMOVED***keyMetadata***REMOVED*** in serverResponse[***REMOVED***widevine2License***REMOVED***][***REMOVED***metadata***REMOVED***])

        license_ = base64.b64decode(serverResponse[***REMOVED***widevine2License***REMOVED***][***REMOVED***license***REMOVED***])
        # LOG.info(***REMOVED***Received license (len: %d):\n %s***REMOVED*** % (
        #    len(license_), binascii.hexlify(license_).upper()
        # ))

        widevineAdapter.UpdateCurrentSession(license_, streamType)

        if len(serverResponse[***REMOVED***widevine2License***REMOVED***][***REMOVED***metadata***REMOVED***][***REMOVED***keyMetadata***REMOVED***]) >= 1 \
                and \
                        serverResponse[***REMOVED***widevine2License***REMOVED***][***REMOVED***metadata***REMOVED***][***REMOVED***keyMetadata***REMOVED***][0][***REMOVED***keyType***REMOVED***] \
                        == ***REMOVED***SERVICE_CERTIFICATE***REMOVED***:
            # this indicates that a certificate must be downloaded as well
            # see https://storage.googleapis.com/wvdocs/Widevine_DRM_Proxy_Integration.pdf
            # section Pre-loading a service certificate
            # the document says: SetServerCertificate must be exected prior
            #  to every playback session to avoid the certificate request
            #
            # BUT I don't know how to get this and firefox/chrome don't request this
            #     so handle this as an error, which should not happend when the cdm
            #     is initialized correctly
            raise util.ApplicationException(***REMOVED***SERVICE_CERTIFICATE requested error. ***REMOVED***
                                            ***REMOVED***We can't handle this.***REMOVED***)
        assert (widevineAdapter.ValidKeyIdsSize(streamType) > 0)

        return mpd_
