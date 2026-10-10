import struct
import unittest
from unittest.mock import Mock, patch

import mpd
import util
from amazon import Amazon


MPD_DATA = """<MPD xmlns="urn:mpeg:dash:schema:mpd:2011" type="static">
  <Period>
    <BaseURL>../assets/</BaseURL>
    <AdaptationSet contentType="video" mimeType="video/mp4">
      <Representation id="video-1" width="960" height="440" bandwidth="1000000">
        <BaseURL>/media/video.mp4</BaseURL>
        <SegmentBase indexRange="1000-1055">
          <Initialization range="0-999"/>
        </SegmentBase>
      </Representation>
    </AdaptationSet>
    <AdaptationSet contentType="audio" mimeType="audio/mp4">
      <Representation id="audio-1" audioSamplingRate="48000" bandwidth="128000">
        <BaseURL>audio.mp4</BaseURL>
        <SegmentBase indexRange="80-135">
          <Initialization range="0-79"/>
        </SegmentBase>
      </Representation>
    </AdaptationSet>
    <AdaptationSet contentType="text" mimeType="application/mp4">
      <Representation id="subtitle-1"/>
    </AdaptationSet>
  </Period>
</MPD>"""


def make_sidx(version=0, reference_sizes=(100, 50), first_offset=4):
    if version == 0:
        timing = struct.pack(">II", 0, first_offset)
    else:
        timing = struct.pack(">QQ", 0, first_offset)
    payload = struct.pack(">B3sII", version, b"\0\0\0", 1, 1000)
    payload += timing + struct.pack(">HH", 0, len(reference_sizes))
    for reference_size in reference_sizes:
        payload += struct.pack(">III", reference_size, 1000, 0x90000000)
    return struct.pack(">I4s", 8 + len(payload), b"sidx") + payload


class SegmentBaseParserTests(unittest.TestCase):
    def test_mpd_parser_keeps_init_and_index_ranges(self):
        response = Mock(status_code=200, url="https://cdn.example/path/manifest.mpd",
                        text=MPD_DATA)
        response.raise_for_status.return_value = None
        client = Amazon()
        client.session = Mock()
        client.session.get.return_value = response

        parsed = client.getMpdDataAndExtractInfos(response.url)

        self.assertEqual([item.contentType for item in parsed.adaptionSetLst],
                         ["video", "audio"])
        video = parsed.adaptionSetLst[0].representationLst[0]
        self.assertEqual(video.mediaFileUrl, "https://cdn.example/media/video.mp4")
        self.assertEqual(video.segmentList.initRange, "0-999")
        self.assertEqual(video.segmentList.indexRange, "1000-1055")
        audio = parsed.adaptionSetLst[1].representationLst[0]
        self.assertEqual(audio.mediaFileUrl, "https://cdn.example/assets/audio.mp4")

    def test_sidx_v0_and_v1_expand_to_inclusive_ranges(self):
        self.assertEqual(mpd.parse_sidx_references(make_sidx(), 1000),
                         ["1060-1159", "1160-1209"])
        self.assertEqual(mpd.parse_sidx_references(make_sidx(version=1), 1000),
                         ["1068-1167", "1168-1217"])

    def test_segmentbase_fetch_uses_range_and_checks_response(self):
        representation = mpd.VideoRepresentation()
        representation.mediaFileUrl = "https://cdn.example/video.mp4"
        representation.segmentList = mpd.SegmentList()
        representation.segmentList.indexRange = "1000-1055"
        response = Mock(status_code=206, content=make_sidx(),
                        headers={"Content-Range": "bytes 1000-1055/5000"})

        with patch.object(util.requests, "get", return_value=response) as get:
            util.resolveSegmentBaseRanges(representation)

        self.assertEqual(representation.segmentList.mediaRangeLst,
                         ["1060-1159", "1160-1209"])
        self.assertEqual(get.call_args.kwargs["headers"]["Range"], "bytes=1000-1055")

    def test_segmentbase_rejects_unexpected_content_range(self):
        representation = mpd.VideoRepresentation()
        representation.mediaFileUrl = "https://cdn.example/video.mp4"
        representation.segmentList = mpd.SegmentList()
        representation.segmentList.indexRange = "1000-1055"
        response = Mock(status_code=206, content=make_sidx(),
                        headers={"Content-Range": "bytes 0-55/5000"})

        with patch.object(util.requests, "get", return_value=response):
            with self.assertRaises(util.ApplicationException):
                util.resolveSegmentBaseRanges(representation)

    def test_resolution_selection_falls_back_to_best_available(self):
        manifest = mpd.Mpd()
        video = mpd.AdaptionSet()
        video.contentType = "video"
        for width, height in ((464, 208), (720, 328), (960, 440)):
            representation = mpd.VideoRepresentation()
            representation.width = width
            representation.height = height
            video.addRepresentation(representation)

        audio = mpd.AdaptionSet()
        audio.contentType = "audio"
        audio.addRepresentation(mpd.AudioRepresentation())
        manifest.addAdaptionSet(video)
        manifest.addAdaptionSet(audio)

        selectedVideo, _ = util.selectVideoAndAudio(manifest, 1920, 1080)

        self.assertEqual((selectedVideo.width, selectedVideo.height), (960, 440))


if __name__ == "__main__":
    unittest.main()
