#include ***REMOVED***gtest/gtest.h***REMOVED***
#include <fstream>
#include <memory>

#include ***REMOVED***logging.h***REMOVED***
#include ***REMOVED***util.h***REMOVED***
#include ***REMOVED***mp4_stream.h***REMOVED***
#include ***REMOVED***blocking_stream.h***REMOVED***
#include ***REMOVED***components.h***REMOVED***

#define AUDIO_DATA_FILE_PATH ***REMOVED***data/encrypted/BigBuckBunny_fragmented_1280x720/audio***REMOVED***

class MP4AudioStreamTest : public ::testing::Test {
  protected:
    MP4AudioStreamTest() {
      std::string rootPath = __DIR__ + std::string(AUDIO_DATA_FILE_PATH);
      initPath_ = rootPath + std::string(***REMOVED***/init.mp4***REMOVED***);
      seg1Path_ = rootPath + std::string(***REMOVED***/seg-1.m4s***REMOVED***);
      dummyMediaFileUrl_ = std::string(***REMOVED***www.example.com***REMOVED***);
      dummyRangeList_ = std::vector<std::string>({***REMOVED***1-100***REMOVED***, ***REMOVED***101-200***REMOVED***, ***REMOVED***201-300***REMOVED***});
    }

    MP4AudioStream stream_;
    std::string initPath_;
    std::string seg1Path_;
    std::string dummyMediaFileUrl_;
    std::vector<std::string> dummyRangeList_;
};

TEST_F(MP4AudioStreamTest, Init) {
  std::vector<uint8_t> data;
  int length = ReadFileData(initPath_, data);
  ASSERT_GT(length, 0);

  size_t codecPrivateDataSize = 0;
  uint8_t *codecPrivateData = nullptr;
  bool ret = stream_.Init(dummyMediaFileUrl_, dummyRangeList_, data.data(), length, 
               codecPrivateData, codecPrivateDataSize);
  ASSERT_TRUE(ret);

  EXPECT_EQ(2, stream_.GetChannels());
  EXPECT_EQ(44100, stream_.GetSamplingRate());
  EXPECT_EQ(16, stream_.GetSampleSize());

  uint8_t adtsHeader[10];
  uint8_t *outPtr = stream_.BuildAdtsHeader(adtsHeader, 1024);

  EXPECT_EQ(stream_.GetAdtsHeaderSize(), outPtr - adtsHeader);
  EXPECT_EQ(adtsHeader[0], 0xff); /* fixed */
  EXPECT_EQ(adtsHeader[1], 0xf1); /* fixed */
  EXPECT_EQ(adtsHeader[2], 0x50); /* AAC LC (objtype=2), 44100 Hz (freq-idx=4), ... */
  EXPECT_EQ(adtsHeader[3], 0x80); /* ... 2 channels, length = */
  EXPECT_EQ(adtsHeader[4], 0x80); /* 1031 bytes - 1024 bytes sample size + */
  EXPECT_EQ(adtsHeader[5], 0xff); /* 7 bytes */
  EXPECT_EQ(adtsHeader[6], 0xfc); /* ADTS header */
}

//////////////////////////////////////////////////////

#define VIDEO_DATA_FILE_PATH ***REMOVED***data/encrypted/jellyfish-fragmented_1920x1080/video/avc1***REMOVED***

class MP4VideoStreamTest : public ::testing::Test {
  protected:
    MP4VideoStreamTest() {
      std::string rootPath = __DIR__ + std::string(VIDEO_DATA_FILE_PATH);
      initPath_ = rootPath + std::string(***REMOVED***/init.mp4***REMOVED***);
      seg1Path_ = rootPath + std::string(***REMOVED***/seg-1.m4s***REMOVED***);
      seg2Path_ = rootPath + std::string(***REMOVED***/seg-2.m4s***REMOVED***);
      dummyMediaFileUrl_ = std::string(***REMOVED***www.example.com***REMOVED***);
      dummyRangeList_ = std::vector<std::string>({***REMOVED***1-100***REMOVED***, ***REMOVED***101-200***REMOVED***, ***REMOVED***201-300***REMOVED***});

      inputStream_ = wvAdapterLib::GetVideoNetStream();
    }

    MP4VideoStream stream_;
    std::string initPath_;
    std::string seg1Path_;
    std::string seg2Path_;
    std::string dummyMediaFileUrl_;
    std::vector<std::string> dummyRangeList_;

    std::shared_ptr<BlockingStream> inputStream_;
};

TEST_F(MP4VideoStreamTest, Init) {
  // DEBUG_PRINT(***REMOVED***initPath: ***REMOVED*** << initPath_);
  std::vector<uint8_t> data;
  int length = ReadFileData(initPath_, data);
  ASSERT_GT(length, 0);

  size_t codecPrivateDataSize = 1;
  uint8_t codecPrivateData[1] = {0};
  bool ret = stream_.Init(dummyMediaFileUrl_, dummyRangeList_, data.data(), length, 
               codecPrivateData, codecPrivateDataSize);
  ASSERT_TRUE(ret);

  EXPECT_TRUE(stream_.isProtected());
  EXPECT_EQ(16, stream_.GetDefaultKeyIdSize());
  EXPECT_GT(stream_.GetTimeScale(), 0);

  EXPECT_EQ(1920, stream_.GetVideoWidth());
  EXPECT_EQ(1080, stream_.GetVideoHeight());
  EXPECT_GT(stream_.GetVideoExtraDataSize(), 0);
  EXPECT_GT(stream_.GetCodecPrivateDataSize(), 0);
  EXPECT_GT(stream_.GetNaluLengthSize(), 0);
}

TEST_F(MP4VideoStreamTest, ParseMoofData) {
  std::vector<uint8_t> data;
  int length = ReadFileData(seg1Path_, data);
  ASSERT_GT(length, 0);

  inputStream_->AddFragment(
      NETWORK_FRAGMENT(
          std::unique_ptr<uint8_t []> (new uint8_t[length]),
          length
      )
  );

  AP4_Size bytesWritten = 0;
  while (bytesWritten < length) {
    AP4_Result ap4Ret = inputStream_->WritePartial(data.data(), length, bytesWritten);
    ASSERT_EQ(AP4_SUCCESS, ap4Ret);
  }

  bool ret;
  std::unique_ptr<DEMUXED_SAMPLE> sample = stream_.ParseMoofMdat(ret);
  ASSERT_TRUE(ret);

  EXPECT_GT(sample->mdatRawDataSize, 0);
  EXPECT_TRUE(sample->mdatRawData);

  ASSERT_TRUE(sample->hasSubSamples);
  EXPECT_TRUE(sample->sampleInfoTable);
  EXPECT_EQ(0, sample->sampleCount);
  EXPECT_FALSE(sample->sampleSizes);
}