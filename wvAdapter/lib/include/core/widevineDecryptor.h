#ifndef WIDEVINE_DECRYPTOR_H_
#define WIDEVINE_DECRYPTOR_H_

#include ***REMOVED***content_decryption_module.h***REMOVED***
#include ***REMOVED***mp4_stream.h***REMOVED*** // DEMUXED_SAMPLE, MP4Stream

#include ***REMOVED***common.h***REMOVED***

#include <string>
#include <vector>
#include <memory>

class WidevineBuffer : public cdm::Buffer {
public:
  explicit WidevineBuffer(size_t aSize);
  ~WidevineBuffer() override;
  void Destroy() override;
  uint32_t Capacity() const override;
  uint8_t *Data() override;
  void SetSize(uint32_t aSize) override;
  uint32_t Size() const override;

private:
  uint8_t *buffer_ = nullptr;
  uint32_t bufferCapacity_ = 0;
  uint32_t bufferSize_ = 0;
};

class WidevineDecryptedBlock : public cdm::DecryptedBlock {
public:
  WidevineDecryptedBlock();
  ~WidevineDecryptedBlock() override;
  void SetDecryptedBuffer(cdm::Buffer *aBuffer) override;
  cdm::Buffer *DecryptedBuffer() override;
  void SetTimestamp(int64_t aTimestamp) override;
  int64_t Timestamp() const override;

private:
  cdm::Buffer *mBuffer;
  int64_t mTimestamp;
};

class WidevineVideoFrame : public cdm::VideoFrame {
public:
  WidevineVideoFrame();
  ~WidevineVideoFrame();

  void SetFormat(cdm::VideoFormat aFormat) override;
  cdm::VideoFormat Format() const override;

  void SetSize(cdm::Size aSize) override;
  cdm::Size Size() const override;

  void SetFrameBuffer(cdm::Buffer *aFrameBuffer) override;
  cdm::Buffer *FrameBuffer() override;

  void SetPlaneOffset(cdm::VideoFrame::VideoPlane aPlane,
                      uint32_t aOffset) override;
  uint32_t PlaneOffset(cdm::VideoFrame::VideoPlane aPlane) override;

  void SetStride(cdm::VideoFrame::VideoPlane aPlane, uint32_t aStride) override;
  uint32_t Stride(cdm::VideoFrame::VideoPlane aPlane) override;

  void SetTimestamp(int64_t aTimestamp) override;
  int64_t Timestamp() const override;

protected:
  cdm::VideoFormat mFormat;
  cdm::Size mSize;
  cdm::Buffer *mBuffer;
  uint32_t mPlaneOffsets[kMaxPlanes];
  uint32_t mPlaneStrides[kMaxPlanes];
  int64_t mTimestamp;
};

class WidevineAudioFrames : public cdm::AudioFrames {
public:
  WidevineAudioFrames()
      : buffer_(nullptr), format_(cdm::AudioFormat::kUnknownAudioFormat) {}
  ~WidevineAudioFrames();

  // Represents decrypted and decoded audio frames. AudioFrames can contain
  // multiple audio output buffers, which are serialized into this format:
  //
  // |<------------------- serialized audio buffer ------------------->|
  // | int64_t timestamp | int64_t length | length bytes of audio data |
  //
  // For example, with three audio output buffers, the AudioFrames will look
  // like this:
  //
  // |<----------------- AudioFrames ------------------>|
  // | audio buffer 0 | audio buffer 1 | audio buffer 2 |
  void SetFrameBuffer(cdm::Buffer *buffer) { buffer_ = buffer; }
  cdm::Buffer *FrameBuffer() { return buffer_; }

  // CDM calls this method and provides a valid format
  // Planar data is stored end to end, e.g.:
  // |ch1 sample1||ch1 sample2|....|ch1 sample_last||ch2 sample1|...
  void SetFormat(cdm::AudioFormat format) { format_ = format; }
  cdm::AudioFormat Format() const { return format_; }

protected:
  cdm::Buffer *buffer_;
  cdm::AudioFormat format_;
};


class WidevineDecryptorThread {
public:
  WidevineDecryptorThread();
  ~WidevineDecryptorThread();

  void Start(const std::string &name, MEDIA_TYPE  type);
  void Join();
  void Shutdown();

private:
  class Impl;
  std::unique_ptr<Impl> pImpl_;
};

// XXX this is still used by video.cc decryptor -.-
void FillCdmInBuffer(std::unique_ptr<DEMUXED_SAMPLE> &sample, cdm::InputBuffer *inBuffer,
                     size_t samplePos, const uint8_t *data, uint32_t dataSize,
                     std::shared_ptr<MP4Stream> stream, unsigned int *maxSubSampleCount,
                     cdm::SubsampleEntry **subsampleBuffer,
                     uint64_t *globalTimestamp, uint32_t headerSize);

#endif // WIDEVINE_DECRYPTOR_H_
