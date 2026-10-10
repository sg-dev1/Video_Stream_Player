
import struct


def prettyPrintList(lst):
    ret = str(lst)
    lenLst = len(lst)
    if lenLst > 5:
        ret = "(len={0})<data ommited>".format(lenLst)
    return ret


class AdaptionSet(object):
    def __init__(self):
        # e.g. 'audio' or 'video'
        self._contentType = ""
        self._mimeType = ""
        self._lang = ""    # only relevant for audio
        self._format = ""  # only for video, e.g. 16:9
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
        if self._contentType == "video":
            part = "format={0}".format(self._format)
        elif self._contentType == "audio":
            part = "language={0}".format(self._lang)
        else:
            part = ""

        strLst = [x.__str__() for x in self._representationLst]
        representationLstStr = "\n    ".join(strLst)

        return "AdaptionSet( contentType={0}, mimeType={1}, {2}, widevineContentProtection={3}, " \
          "playReadyContentProtection={4}, representationLst=\n    {5} )" \
          .format(self._contentType, self._mimeType, part, self._widevineContentProt,
          self._playReadyContentProt, representationLstStr)


class ContentProtection(object):
    def __init__(self):
        self._schemeIdUri = ""
        self._cencPsshData = ""

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
        print("add adaptionset: %s" % value)
        self._adaptionSetLst.append(value)

    def getFirstWidevineContentProt(self):
        for aSet in self._adaptionSetLst:
            if aSet.widevineContentProtection:
                return aSet.widevineContentProtection
        return None

    def getWidevineContentProt(self, contentType="video", adaptionSet=0):
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

    def getPlayReadyContentProt(self, contentType="video", adaptionSet=0):
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
        adaptionSetLstStr = "\n  ".join(strLst)
        return "Mpd( adaptionSetLst=\n  {0} )".format(adaptionSetLstStr)


class SegmentList(object):
    def __init__(self):
        self._duration = 0
        self._timescale = 0
        self._initRange = ""
        self._indexRange = ""
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
    def indexRange(self):
        return self._indexRange

    @indexRange.setter
    def indexRange(self, value):
        self._indexRange = value

    @property
    def mediaRangeLst(self):
        return self._mediaRangeLst

    def addMediaRange(self, value):
        self._mediaRangeLst.append(value)

    def __str__(self):
        mediaRangeLstStr = prettyPrintList(self._mediaRangeLst)

        return "SegmentList( duration={0}, timescale={1}, initRange={2}, mediaRangeLst={3} )" \
            .format(self._duration, self._timescale, self._initRange, mediaRangeLstStr)


def parse_sidx_references(data, absolute_offset):
    """Return byte ranges for direct references in an ISO-BMFF SIDX box."""
    view = memoryview(data)
    offset = 0

    while offset + 8 <= len(view):
        box_size = struct.unpack_from(">I", view, offset)[0]
        box_type = bytes(view[offset + 4:offset + 8])
        header_size = 8

        if box_size == 1:
            if offset + 16 > len(view):
                raise ValueError("Truncated extended MP4 box header")
            box_size = struct.unpack_from(">Q", view, offset + 8)[0]
            header_size = 16
        elif box_size == 0:
            box_size = len(view) - offset

        if box_size < header_size or offset + box_size > len(view):
            raise ValueError("Invalid MP4 box size")
        if box_type != b"sidx":
            offset += box_size
            continue

        payload = offset + header_size
        if payload + 12 > offset + box_size:
            raise ValueError("Truncated SIDX box")

        version = view[payload]
        if version not in (0, 1):
            raise ValueError("Unsupported SIDX version %d" % version)

        cursor = payload + 4  # version and flags
        cursor += 8  # reference_ID and timescale
        time_offset_size = 8 if version == 1 else 4
        cursor += time_offset_size * 2  # earliest presentation time, first_offset
        cursor += 2  # reserved
        if cursor + 2 > offset + box_size:
            raise ValueError("Truncated SIDX reference count")
        reference_count = struct.unpack_from(">H", view, cursor)[0]
        cursor += 2

        if cursor + reference_count * 12 > offset + box_size:
            raise ValueError("Truncated SIDX references")

        first_offset_position = payload + 4 + 8 + time_offset_size
        if version == 1:
            first_offset = struct.unpack_from(">Q", view, first_offset_position)[0]
        else:
            first_offset = struct.unpack_from(">I", view, first_offset_position)[0]

        next_range_start = absolute_offset + offset + box_size + first_offset
        ranges = []
        for _ in range(reference_count):
            reference = struct.unpack_from(">I", view, cursor)[0]
            reference_type = reference >> 31
            referenced_size = reference & 0x7fffffff
            cursor += 12  # reference, duration, SAP

            if reference_type:
                raise ValueError("Hierarchical SIDX references are unsupported")
            if referenced_size == 0:
                raise ValueError("SIDX reference has zero size")

            range_end = next_range_start + referenced_size - 1
            ranges.append("%d-%d" % (next_range_start, range_end))
            next_range_start = range_end + 1

        return ranges

    raise ValueError("No SIDX box found in SegmentBase index range")


class Representation(object):
    def __init__(self):
        self._mediaFileUrl = ""
        self._segmentList = None
        self._initData = ""
        self._codecPrivateData = ""

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
        super().__init__()
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
        return "VideoRepresentation( {0} x {1},  mediaFileUrl={2}, segmentList={3})" \
            .format(self._width, self._height,
            self.mediaFileUrl, self.segmentList)

class AudioRepresentation(Representation):
    def __init__(self):
        super().__init__()
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
        return "AudioRepresentation( samplingRate={0}, bandwidth={1},  mediaFileUrl={2}, segmentList={3})" \
            .format(self._samplingRate, self._bandwidth,
            self.mediaFileUrl, self.segmentList)
