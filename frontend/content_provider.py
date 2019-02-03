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
        """
        Connects to the provider (e.g. log in)
        """
        self.provider.logIn()

    def close(self):
        """
        Close the provider
        """
        self.provider.close()
        widevineAdapter.Close()

    def requestLicenseForEstablishedSession(self, resId, cencInitData):
        """
        Same as requestLicense, but does not call widevineAdapter.Init()
        """
        widevineAdapter.CreateSessionAndGenerateRequest(cencInitData)

        # wait for cdm to process request
        time.sleep(3)

        challenge = widevineAdapter.GetSessionMessage()
        if not challenge:
            LOG.error("Error retrieving challenge for contentType %s" % contentType)
            raise util.ApplicationException()
        b64EncodedChallenge = base64.b64encode(challenge).decode("utf-8")
        serverResponse = self.provider.getWidevine2License(resId, b64EncodedChallenge)

        # LOG.info("")
        # LOG.info("Server response: ")
        # LOG.info(json.dumps(serverResponse, sort_keys=True, indent=4, separators=(',', ': ')))
        # LOG.info("")

        assert ("widevine2License" in serverResponse)
        assert ("license" in serverResponse["widevine2License"])
        assert ("metadata" in serverResponse["widevine2License"])
        assert ("keyMetadata" in serverResponse["widevine2License"]["metadata"])

        license_ = base64.b64decode(serverResponse["widevine2License"]["license"])
        widevineAdapter.UpdateCurrentSession(license_)

        time.sleep(3)
        # assert(False)


    def requestLicense(self, resId, mpdUrl, streamType, adaptionSet=0, mpdObject=None):
        """
        Requests a new License from the provider

        :param resId: The id of the resource where a license should be requested (e.g. asin for Amazon)
        :param mpdUrl: The url of the mpd file
        :param streamType: The stream type, one of constants.STREAM_TYPE_AUDIO or constants.STREAM_TYPE_VIDEO
        :param adaptionSet: Number of adaption set in list, 0 ist the first one
        :return: The parsed MPD object for further usage (e.g. selecting the stream, getting the media file locations)
        """
        if mpdObject:
            mpd_ = mpdObject
        else:
            mpd_ = self.provider.getMpdDataAndExtractInfos(mpdUrl)
        cencInitData = util.buildWidevineInitDataString(mpd_, streamType, adaptionSet)
        # LOG.debug("cenc pssh initdata: %s" % binascii.hexlify(cencInitData).upper())

        # widevineAdapter stuff
        widevineAdapter.Init()
        widevineAdapter.CreateSessionAndGenerateRequest(cencInitData, streamType)

        # wait for cdm to process request
        time.sleep(3)

        challenge = widevineAdapter.GetSessionMessage(streamType)
        if not challenge:
            LOG.error("Error retrieving challenge")
            raise util.ApplicationException()
        # LOG.info("Got Widevine challenge (len: %d): %s" %
        #         (len(challenge), binascii.hexlify(challenge).upper()))
        b64EncodedChallenge = base64.b64encode(challenge).decode("utf-8")

        # send license request to server
        serverResponse = self.provider.getWidevine2License(resId, b64EncodedChallenge)

        # LOG.info("")
        # LOG.info("Server response: ")
        # LOG.info(json.dumps(serverResponse, sort_keys=True, indent=4, separators=(',', ': ')))
        # LOG.info("")

        assert ("widevine2License" in serverResponse)
        assert ("license" in serverResponse["widevine2License"])
        assert ("metadata" in serverResponse["widevine2License"])
        assert ("keyMetadata" in serverResponse["widevine2License"]["metadata"])

        license_ = base64.b64decode(serverResponse["widevine2License"]["license"])
        # LOG.info("Received license (len: %d):\n %s" % (
        #    len(license_), binascii.hexlify(license_).upper()
        # ))

        widevineAdapter.UpdateCurrentSession(license_, streamType)

        if len(serverResponse["widevine2License"]["metadata"]["keyMetadata"]) >= 1 \
                and \
                        serverResponse["widevine2License"]["metadata"]["keyMetadata"][0]["keyType"] \
                        == "SERVICE_CERTIFICATE":
            # this indicates that a certificate must be downloaded as well
            # see https://storage.googleapis.com/wvdocs/Widevine_DRM_Proxy_Integration.pdf
            # section Pre-loading a service certificate
            # the document says: SetServerCertificate must be exected prior
            #  to every playback session to avoid the certificate request
            #
            # BUT I don't know how to get this and firefox/chrome don't request this
            #     so handle this as an error, which should not happend when the cdm
            #     is initialized correctly
            raise util.ApplicationException("SERVICE_CERTIFICATE requested error. "
                                            "We can't handle this.")
        assert (widevineAdapter.ValidKeyIdsSize(streamType) > 0)

        return mpd_
