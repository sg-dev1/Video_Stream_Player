
def prettyPrintList(lst):
    ret = str(lst)
    lenLst = len(lst)
    if lenLst > 5:
        ret = ***REMOVED***(len={0})<data ommited>***REMOVED***.format(lenLst)
    return ret


class AdaptionSet(object):
    def __init__(self):
        # e.g. 'audio' or 'video'
        self._contentType = ***REMOVED******REMOVED***
        self._mimeType = ***REMOVED******REMOVED***
        self._lang = ***REMOVED******REMOVED***    # only relevant for audio
        self._format = ***REMOVED******REMOVED***  # only for video, e.g. 16:9
        self._widevineContentProt = None
        self._playReadyContentProt = None
        self._representationLst = []

    @property
    def contentType(self):
        return self._contentType

    @contentType.setter
    def contentType(self, value):
        self._contentType = value

    @property
    def mimeType(self):
        return self._mimeType

    @mimeType.setter
    def mimeType(self, value):
        self._mimeType = value

    @property
    def lang(self):
        return self._lang

    @lang.setter
    def lang(self, value):
        self._lang = value

    @property
    def format(self):
        return self._format

    @format.setter
    def format(self, value):
        self._format = value

    @property
    def widevineContentProtection(self):
        return self._widevineContentProt

    @widevineContentProtection.setter
    def widevineContentProtection(self, value):
        self._widevineContentProt = value

    @property
    def playReadyContentProtection(self):
        return self._playReadyContentProt

    @playReadyContentProtection.setter
    def playReadyContentProtection(self, value):
        self._playReadyContentProt = value
    @property
    def representationLst(self):
        return self._representationLst

    def addRepresentation(self, value):
        self._representationLst.append(value)

    def __str__(self):
        if self._contentType == ***REMOVED***video***REMOVED***:
            part = ***REMOVED***format={0}***REMOVED***.format(self._format)
        elif self._contentType == ***REMOVED***audio***REMOVED***:
            part = ***REMOVED***language={0}***REMOVED***.format(self._lang)
        else:
            part = ***REMOVED******REMOVED***

        strLst = [x.__str__() for x in self._representationLst]
        representationLstStr = ***REMOVED***\n    ***REMOVED***.join(strLst)

        return ***REMOVED***AdaptionSet( contentType={0}, mimeType={1}, {2}, widevineContentProtection={3}, ***REMOVED*** \
          ***REMOVED***playReadyContentProtection={4}, representationLst=\n    {5} )***REMOVED*** \
          .format(self._contentType, self._mimeType, part, self._widevineContentProt,
          self._playReadyContentProt, representationLstStr)


class ContentProtection(object):
    def __init__(self):
        self._schemeIdUri = ***REMOVED******REMOVED***
        self._cencPsshData = ***REMOVED******REMOVED***

    @property
    def schemeIdUri(self):
        return self._schemeIdUri

    @schemeIdUri.setter
    def schemeIdUri(self, value):
        self._schemeIdUri = value

    @property
    def cencPsshData(self):
        return self._cencPsshData

    @cencPsshData.setter
    def cencPsshData(self, value):
        self._cencPsshData = value


class Mpd(object):
    def __init__(self):
        self._adaptionSetLst = []

    @property
    def adaptionSetLst(self):
        return self._adaptionSetLst

    def addAdaptionSet(self, value):
        print(***REMOVED***add adaptionset: %s***REMOVED*** % value)
        self._adaptionSetLst.append(value)

    def getFirstWidevineContentProt(self):
        for aSet in self._adaptionSetLst:
            if aSet.widevineContentProtection:
                return aSet.widevineContentProtection
        return None

    def getWidevineContentProt(self, contentType=***REMOVED***video***REMOVED***, adaptionSet=0):
        wvContentProt = None
        idx = 0
        for aSet in self._adaptionSetLst:
            if aSet.contentType == contentType:
                if aSet.widevineContentProtection:
                    wvContentProt = aSet.widevineContentProtection
                if adaptionSet == idx and wvContentProt:
                    return wvContentProt
                idx = idx + 1
        return wvContentProt

    def getFirstPlayReadyContentProt(self):
        for aSet in self._adaptionSetLst:
            if aSet.playReadyContentProtection:
                return aSet.playReadyContentProtection
        return None

    def getPlayReadyContentProt(self, contentType=***REMOVED***video***REMOVED***, adaptionSet=0):
        prContentProt = None
        idx = 0
        for aSet in self._adaptionSetLst:
            if aSet.contentType == contentType:
                if aSet.playReadyContentProtection:
                    prContentProt = aSet.playReadyContentProtection
                if adaptionSet == idx and prContentProt:
                    return prContentProt
                idx = idx + 1
        return prContentProt

    def __str__(self):
        strLst = [x.__str__() for x in self._adaptionSetLst]
        adaptionSetLstStr = ***REMOVED***\n  ***REMOVED***.join(strLst)
        return ***REMOVED***Mpd( adaptionSetLst=\n  {0} )***REMOVED***.format(adaptionSetLstStr)


class SegmentList(object):
    def __init__(self):
        self._duration = 0
        self._timescale = 0
        self._initRange = ***REMOVED******REMOVED***
        self._mediaRangeLst = []

    @property
    def duration(self):
        return self._duration

    @duration.setter
    def duration(self, value):
        self._duration = int(value)

    @property
    def timescale(self):
        return self._timescale

    @timescale.setter
    def timescale(self, value):
        self._timescale = int(value)

    @property
    def initRange(self):
        return self._initRange

    @initRange.setter
    def initRange(self, value):
        self._initRange = value

    @property
    def mediaRangeLst(self):
        return self._mediaRangeLst

    def addMediaRange(self, value):
        self._mediaRangeLst.append(value)

    def __str__(self):
        mediaRangeLstStr = prettyPrintList(self._mediaRangeLst)

        return ***REMOVED***SegmentList( duration={0}, timescale={1}, initRange={2}, mediaRangeLst={3} )***REMOVED*** \
            .format(self._duration, self._timescale, self._initRange, mediaRangeLstStr)


class Representation(object):
    def __init__(self):
        self._mediaFileUrl = ***REMOVED******REMOVED***
        self._segmentList = None
        self._initData = ***REMOVED******REMOVED***
        self._codecPrivateData = ***REMOVED******REMOVED***

    @property
    def mediaFileUrl(self):
        return self._mediaFileUrl

    @mediaFileUrl.setter
    def mediaFileUrl(self, value):
        self._mediaFileUrl = value

    @property
    def segmentList(self):
        return self._segmentList

    @segmentList.setter
    def segmentList(self, value):
        self._segmentList = value

    @property
    def initData(self):
        return self._initData

    @initData.setter
    def initData(self, value):
        self._initData = value
    @property
    def codecPrivateData(self):
        return self._codecPrivateData

    @codecPrivateData.setter
    def codecPrivateData(self, value):
        self._codecPrivateData = value


class VideoRepresentation(Representation):
    def __init__(self):
        self._width = 0
        self._height = 0

    @property
    def width(self):
        return self._width

    @width.setter
    def width(self, value):
        self._width = int(value)

    @property
    def height(self):
        return self._height

    @height.setter
    def height(self, value):
        self._height = int(value)

    def __str__(self):
        return ***REMOVED***VideoRepresentation( {0} x {1},  mediaFileUrl={2}, segmentList={3})***REMOVED*** \
            .format(self._width, self._height,
            self.mediaFileUrl, self.segmentList)

class AudioRepresentation(Representation):
    def __init__(self):
        self._samplingRate = 0
        self._bandwidth = 0

    @property
    def samplingRate(self):
        return self._samplingRate

    @samplingRate.setter
    def samplingRate(self, value):
        self._samplingRate = value

    @property
    def bandwidth(self):
        return self._bandwidth

    @bandwidth.setter
    def bandwidth(self, value):
        self._bandwidth = value

    def __str__(self):
        return ***REMOVED***AudioRepresentation( samplingRate={0}, bandwidth={1},  mediaFileUrl={2}, segmentList={3})***REMOVED*** \
            .format(self._samplingRate, self._bandwidth,
            self.mediaFileUrl, self.segmentList)
