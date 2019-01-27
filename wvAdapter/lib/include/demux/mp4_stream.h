#ifndef MP4_STREAM_H_
#define MP4_STREAM_H_

#include ***REMOVED***content_decryption_module.h***REMOVED***
#include ***REMOVED***common.h***REMOVED***

#include ***REMOVED***Ap4CommonEncryption.h***REMOVED*** // AP4_CencSampleInfoTable

#include <string>
#include <vector>
#include <memory>
#include <functional>


struct DEMUXED_SAMPLE {
  DEMUXED_SAMPLE() : defaultSampleDuration(0), ptr(nullptr),
                     mdatRawData(nullptr), mdatRawDataSize(0),
                     sampleCount(0), sampleSizes(nullptr),
                     sampleInfoTable(nullptr) {}

  uint32_t defaultSampleDuration; // from TFHD atom, needed for time sync

  std::unique_ptr<uint8_t[]> ptr;
  uint32_t mdatRawDataSize;
  uint8_t *mdatRawData;            // a raw pointer into the memory of ptr

  uint32_t sampleCount = 0;
  uint32_t *sampleSizes;           // this only contains encrypted data
                                   // clear text data must be added manually

  // NOTE: sample info table very hard to remove here because it contains subsample info,
  // iv, keyid etc. needed for decrypting frames using Widevine
  // Dependencies in widevineDecryptor.cc (e.g. FillCdmInBuffer()) and video.cc
  // (DecryptVideoThread::Impl::ProcessFrame())
  std::unique_ptr<AP4_CencSampleInfoTable> sampleInfoTable;
  bool hasSubSamples;
};

// TODO move this data structure somewhere else
struct DECRYPTED_SAMPLE {
  DECRYPTED_SAMPLE() : size(0), buffer(nullptr), defaultSampleDuration(0) {}

  DECRYPTED_SAMPLE(uint8_t *buffer, uint32_t size, uint32_t defaultSampleDuration) :
    buffer(buffer), size(size), defaultSampleDuration(defaultSampleDuration) {}

  uint8_t *buffer;
  uint32_t size;

  uint32_t defaultSampleDuration; // from TFHD atom, needed for time sync
};

class WV_ADAPTER_TEST_API MP4Stream {
protected:
  class Impl;

public:
  MP4Stream(Impl *pImpl);
  virtual ~MP4Stream();

  /* Data must be the Init section of the MP4 file */
  virtual bool Init(std::string &mediaFileUrl,
                    std::vector<std::string> &rangeList,
                    const uint8_t *initData, size_t initDataSize,
                    const uint8_t *codecPrivateData,
                    size_t codecPrivateDataSize) = 0;
  virtual void Close() = 0;

  std::unique_ptr<DEMUXED_SAMPLE> ParseMoofMdat(bool &result);

  //
  // getter functions
  //
  std::string &GetMediaFileUrl();
  std::vector<std::string> &GetRangeList();

  const uint8_t *GetDefaultKeyId();
  uint32_t GetDefaultKeyIdSize();

  uint32_t GetTimeScale();

  bool isProtected();
  uint8_t GetDefaultPerSampleIvSize();
  uint8_t GetDefaultConstantIvSize();
  uint8_t GetDefaultCryptByteBlock();
  uint8_t GetDefaultSkipByteBlock();
  uint8_t *GetDefaultConstantIv();

protected:
  std::unique_ptr<Impl> pImpl_;
};


class WV_ADAPTER_TEST_API MP4VideoStream : public MP4Stream {
public:
  MP4VideoStream();
  virtual ~MP4VideoStream();

  bool Init(std::string &mediaFileUrl, std::vector<std::string> &rangeList,
            const uint8_t *initData, size_t initDataSize,
            const uint8_t *codecPrivateData,
            size_t codecPrivateDataSize) override;
  void Close() override;

  //
  // getter functions
  //
  uint32_t GetVideoWidth();
  uint32_t GetVideoHeight();

  // contains sps/pps data
  uint8_t *GetVideoExtraData();
  uint32_t GetVideoExtraDataSize();
  // codec private data got from mpd file
  uint8_t *GetCodecPrivateData();
  uint32_t GetCodecPrivateDataSize();

  cdm::VideoDecoderConfig::VideoCodecProfile GetProfile();
  // https://msdn.microsoft.com/en-us/library/windows/desktop/dd757808(v=vs.85).aspx
  uint8_t GetNaluLengthSize();

private:
  class Impl;
};


class WV_ADAPTER_TEST_API MP4AudioStream : public MP4Stream {
public:
  MP4AudioStream();
  virtual ~MP4AudioStream();

  bool Init(std::string &mediaFileUrl, std::vector<std::string> &rangeList,
            const uint8_t *initData, size_t initDataSize,
            const uint8_t *codecPrivateData,
            size_t codecPrivateDataSize) override;
  void Close() override;

  /*
   * @brief Returns the default ADTS header size of 7 (e.g. no CRC)
   **/
  uint32_t GetAdtsHeaderSize();
  /*
   * @brief Implementation according to https://wiki.multimedia.cx/index.php/ADTS.
   * CRC not supported.
   *
   * @param outptr Memory where to but the build ADTS header. Must be > AdtsHeaderSize (7)
   * @param sampleSize Size of the sample for which the header should be built.
   *        Size + headerSize (7) will be encoded in the header (see spec)
   * @return Pointer to memory after the header, e.g. outptr + 7
   **/
  uint8_t *BuildAdtsHeader(uint8_t *outptr, uint32_t sampleSize);

  //
  // Getter functions
  //
  uint8_t GetEncoding();
  uint32_t GetSamplingRate();
  void SetSamplingRate(uint32_t newSamplingRate);
  uint32_t GetSampleSize();
  uint32_t GetChannels();

  uint8_t GetDecoderSpecificInfoSize();
  uint8_t *GetDecoderSpecificInfo();

private:
  class Impl;
};

#endif // MP4_STREAM_H_
