#include "widevineDecryptor.h"
#include "assertion.h"
#include "logging.h"

#include "components.h"

//
// @WidevineBuffer
//
WidevineBuffer::WidevineBuffer(size_t aSize) {
  // INFO_PRINT("Creating WidevineBuffer of size " << aSize);
  // buffer_ = reinterpret_cast<uint8_t*>(malloc(aSize * sizeof(uint8_t)));
  buffer_ = new uint8_t[aSize];
  bufferCapacity_ = aSize;
}

WidevineBuffer::~WidevineBuffer() {}

void WidevineBuffer::Destroy() {
  // INFO_PRINT("WidevineBuffer::Destroy() called ");
  delete[] buffer_;
  buffer_ = nullptr;
  bufferCapacity_ = 0;
  bufferSize_ = 0;
}

uint32_t WidevineBuffer::Capacity() const {
  // INFO_PRINT("WidevineBuffer::Capacity() called ");
  return bufferCapacity_;
}

uint8_t *WidevineBuffer::Data() {
  // INFO_PRINT("WidevineBuffer::Data() called ");
  return buffer_;
}

void WidevineBuffer::SetSize(uint32_t aSize) {
  // INFO_PRINT("Set buffer size to " << aSize);
  bufferSize_ = aSize;
}

uint32_t WidevineBuffer::Size() const {
  // INFO_PRINT("WidevineBuffer::Size() called ");
  return bufferSize_;
}

//
// @WidevineDecryptedBlock
//
WidevineDecryptedBlock::WidevineDecryptedBlock()
    : mBuffer(nullptr), mTimestamp(0) {}

WidevineDecryptedBlock::~WidevineDecryptedBlock() {
  if (mBuffer) {
    mBuffer->Destroy();
    mBuffer = nullptr;
  }
}

void WidevineDecryptedBlock::SetDecryptedBuffer(cdm::Buffer *aBuffer) {
  mBuffer = aBuffer;
}

cdm::Buffer *WidevineDecryptedBlock::DecryptedBuffer() { return mBuffer; }

void WidevineDecryptedBlock::SetTimestamp(int64_t aTimestamp) {
  mTimestamp = aTimestamp;
}

int64_t WidevineDecryptedBlock::Timestamp() const { return mTimestamp; }

//
// @WidevineVideoFrame
//
WidevineVideoFrame::WidevineVideoFrame()
    : mFormat(cdm::VideoFormat::kUnknownVideoFormat), mSize(0, 0),
      mBuffer(nullptr), mTimestamp(0) {
  memset(mPlaneOffsets, 0, sizeof(mPlaneOffsets));
  memset(mPlaneStrides, 0, sizeof(mPlaneStrides));
}

WidevineVideoFrame::~WidevineVideoFrame() {
  if (mBuffer) {
    mBuffer->Destroy();
    mBuffer = nullptr;
  }
}

void WidevineVideoFrame::SetFormat(cdm::VideoFormat aFormat) {
  mFormat = aFormat;
}

cdm::VideoFormat WidevineVideoFrame::Format() const { return mFormat; }

void WidevineVideoFrame::SetSize(cdm::Size aSize) {
  mSize.width = aSize.width;
  mSize.height = aSize.height;
}

cdm::Size WidevineVideoFrame::Size() const { return mSize; }

void WidevineVideoFrame::SetFrameBuffer(cdm::Buffer *aFrameBuffer) {
  // INFO_PRINT("WidevineVideoFrame::SetFrameBuffer to " << aFrameBuffer);
  mBuffer = aFrameBuffer;
}

cdm::Buffer *WidevineVideoFrame::FrameBuffer() { return mBuffer; }

void WidevineVideoFrame::SetPlaneOffset(cdm::VideoFrame::VideoPlane aPlane,
                                        uint32_t aOffset) {
  mPlaneOffsets[aPlane] = aOffset;
}

uint32_t WidevineVideoFrame::PlaneOffset(cdm::VideoFrame::VideoPlane aPlane) {
  return mPlaneOffsets[aPlane];
}

void WidevineVideoFrame::SetStride(cdm::VideoFrame::VideoPlane aPlane,
                                   uint32_t aStride) {
  mPlaneStrides[aPlane] = aStride;
}

uint32_t WidevineVideoFrame::Stride(cdm::VideoFrame::VideoPlane aPlane) {
  return mPlaneStrides[aPlane];
}

void WidevineVideoFrame::SetTimestamp(int64_t timestamp) {
  mTimestamp = timestamp;
}

int64_t WidevineVideoFrame::Timestamp() const { return mTimestamp; }

//
// @WidevineAudioFrame
//
WidevineAudioFrames::~WidevineAudioFrames() {
  if (buffer_) {
    buffer_->Destroy();
    buffer_ = nullptr;
  }
}

//
// @ WidevineDecryptorThread
//
class WidevineDecryptorThread::Impl {
public:
  void Start(const std::string &name, MEDIA_TYPE type);
  void Join() { thread_.join(); }
  void Shutdown() {
    isRunning_ = false;
    globalTimestamp_ = 0;
  }

private:
  void Run();

  std::thread thread_;
  bool isRunning_;
  std::string name_;

  std::shared_ptr<BlockingQueue<std::unique_ptr<DEMUXED_SAMPLE>>> input_;
  std::shared_ptr<BlockingQueue<DECRYPTED_SAMPLE>> output_;
  std::shared_ptr<MP4Stream> stream_;
  std::shared_ptr<WidevineAdapter> adapter_;
  // headerSize is e.g. for an ADTS header in case no subsamples are found
  // which is the case for audio
  uint32_t headerSize_;

  // @ FillCdmInBuffer
  uint64_t globalTimestamp_;
  cdm::SubsampleEntry *subsampleBuffer_ = nullptr;
  unsigned int maxSubSampleCount_ = 0;
};

void WidevineDecryptorThread::Impl::Start(const std::string &name, MEDIA_TYPE type) {
  name_ = name;

  if (MEDIA_TYPE::kAudio == type) {
    input_ = wvAdapterLib::GetDemuxedAudioSampleQueue();
    output_ = wvAdapterLib::GetDecryptedAudioSampleQueue();
    stream_ = wvAdapterLib::GetMp4AudioStream();
    adapter_ = wvAdapterLib::GetAudioAdapter();
    headerSize_ = wvAdapterLib::GetMp4AudioStream()->GetAdtsHeaderSize();
  } else {
    input_ = wvAdapterLib::GetDemuxedVideoSampleQueue();
    output_ = wvAdapterLib::GetDecryptedVideoSampleQueue();
    stream_ = wvAdapterLib::GetMp4VideoStream();
    adapter_ = wvAdapterLib::GetVideoAdapter();
    headerSize_ = 0; // TODO what to set for video?
  }

  globalTimestamp_ = 0;
  isRunning_ = true;

  thread_ = std::thread(&WidevineDecryptorThread::Impl::Run, this);
}

void WidevineDecryptorThread::Impl::Run() {
  logging::addThreadIdNameMapping(std::this_thread::get_id(), name_.c_str());

  INFO_PRINT("Starting WidevineDecryptorThread ...");

  while (isRunning_) {
    std::unique_ptr<DEMUXED_SAMPLE> sample = input_->pop();
    size_t numSamples = sample->sampleInfoTable->GetSampleCount();
    cdm::Buffer **allSamples =
        (cdm::Buffer **)calloc(numSamples, sizeof(cdm::Buffer *));
    if (allSamples == nullptr) {
      ERROR_PRINT("Fatal: Memory allocation failed.");
      ASSERT(false);
    }

    uint32_t dataToDecryptSize;
    uint8_t *dataToDecrypt = sample->mdatRawData;
    uint32_t errors = 0;
    uint32_t decryptedDataSize = 0;
    DEBUG_PRINT("Processing # " << numSamples << " samples.");
    for (size_t i = 0; i < numSamples; i++) {
      dataToDecryptSize = 0;
      if (sample->sampleSizes != nullptr) {
        dataToDecryptSize += headerSize_ + sample->sampleSizes[i];
      } else {
        for (size_t j = 0; j < sample->sampleInfoTable->GetSubsampleCount(i);
             j++) {
          uint16_t bytes_of_cleartext_data;
          uint32_t bytes_of_encrypted_data;
          sample->sampleInfoTable->GetSubsampleInfo(
              i, j, bytes_of_cleartext_data, bytes_of_encrypted_data);

          dataToDecryptSize +=
              bytes_of_cleartext_data + bytes_of_encrypted_data;
        }
      }

      cdm::InputBuffer inBuffer;
      // fill buffer
      FillCdmInBuffer(sample, &inBuffer, i, dataToDecrypt, dataToDecryptSize,
                      stream_, &maxSubSampleCount_, &subsampleBuffer_,
                      &globalTimestamp_, headerSize_);

      WidevineDecryptedBlock decryptedBlock;
      cdm::Status ret = adapter_->Decrypt(inBuffer, &decryptedBlock);
      if (ret == cdm::Status::kSuccess) {
        // save data
        allSamples[i] = decryptedBlock.DecryptedBuffer();
        decryptedDataSize += decryptedBlock.DecryptedBuffer()->Size();
        decryptedBlock.SetDecryptedBuffer(nullptr);
        // XXX there seems to be a timing issue in the cdm
        // so put some timeout between the decrypt calls
        std::this_thread::sleep_for(std::chrono::microseconds(900));
      } else if (ret != cdm::Status::kNeedMoreData) {
        ERROR_PRINT("cdm::Decrypt returned error " << ret << " at subsample # "
                                                   << i << " of " << numSamples
                                                   << " subsamples.");
        PRINT_BYTE_ARRAY("inBuffer.iv", inBuffer.iv, inBuffer.iv_size);
        PRINT_BYTE_ARRAY("inBuffer.key_id", inBuffer.key_id,
                         inBuffer.key_id_size);
        PRINT_BYTE_ARRAY("inBuffer.data", inBuffer.data, inBuffer.data_size);
        ASSERT(false);
      } else {
        // kNeedMoreData error
        errors++;
      }

      dataToDecrypt += dataToDecryptSize;
    }

    if (errors > 0) {
      INFO_PRINT("kNeedMoreData errors produced: " << errors);
    }

    uint32_t outDataSize = decryptedDataSize;
    uint8_t *outBuffer = new uint8_t[outDataSize];
    uint8_t *ptr = outBuffer;
    for (size_t i = 0; i < numSamples; i++) {
      if (allSamples[i]) {
        memcpy(ptr, allSamples[i]->Data(), allSamples[i]->Size());
        ptr += allSamples[i]->Size();
        allSamples[i]->Destroy();
      }
    }

    //output_->push({outBuffer, outDataSize, sample.defaultSampleDuration});
    output_->emplace(outBuffer, outDataSize, sample->defaultSampleDuration);
  }

  INFO_PRINT("Finishing WidevineDecryptorThread ...");
}

WidevineDecryptorThread::WidevineDecryptorThread() : pImpl_(new WidevineDecryptorThread::Impl()) {}

WidevineDecryptorThread::~WidevineDecryptorThread() = default;

void WidevineDecryptorThread::Start(const std::string &name, MEDIA_TYPE  type) {  pImpl_->Start(name, type); }

void WidevineDecryptorThread::Join() { pImpl_->Join(); }

void WidevineDecryptorThread::Shutdown() {  pImpl_->Shutdown(); }


void FillCdmInBuffer(std::unique_ptr<DEMUXED_SAMPLE> &sample, cdm::InputBuffer *inBuffer,
                     size_t samplePos, const uint8_t *data, uint32_t dataSize,
                     std::shared_ptr<MP4Stream> stream, unsigned int *maxSubSampleCount,
                     cdm::SubsampleEntry **subsampleBuffer,
                     uint64_t *globalTimestamp, uint32_t headerSize) {
  uint8_t ivSize = sample->sampleInfoTable->GetIvSize();
  const uint8_t *iv = sample->sampleInfoTable->GetIv(samplePos);
  // PRINT_BYTE_ARRAY("\tiv", iv, ivSize);
  unsigned int subsampleCount = 1;
  bool hasSubSamples = false;
  if (sample->sampleSizes == nullptr) {
    subsampleCount = sample->sampleInfoTable->GetSubsampleCount(samplePos);
    hasSubSamples = true;
  }

  inBuffer->data = data;
  inBuffer->data_size = dataSize;
  inBuffer->key_id = stream->GetDefaultKeyId();
  inBuffer->key_id_size = stream->GetDefaultKeyIdSize();
  // PRINT_BYTE_ARRAY("\tkey_id", inBuffer->key_id, inBuffer->key_id_size);
  inBuffer->iv = iv;
  inBuffer->iv_size = ivSize;
  inBuffer->num_subsamples = subsampleCount;

  if (subsampleCount > *maxSubSampleCount) {
    *subsampleBuffer = (cdm::SubsampleEntry *)realloc(
        *subsampleBuffer, subsampleCount * sizeof(cdm::SubsampleEntry));
    *maxSubSampleCount = subsampleCount;
  }
  inBuffer->subsamples = *subsampleBuffer;

  if (hasSubSamples) {
    for (size_t j = 0; j < subsampleCount; j++) {
      uint16_t bytes_of_cleartext_data;
      uint32_t bytes_of_encrypted_data;
      sample->sampleInfoTable->GetSubsampleInfo(
          samplePos, j, bytes_of_cleartext_data, bytes_of_encrypted_data);

      inBuffer->subsamples[j].clear_bytes = bytes_of_cleartext_data;
      inBuffer->subsamples[j].cipher_bytes = bytes_of_encrypted_data;

      DEBUG_PRINT("\t\tsubsample "
                 << j << ": clear_bytes: " << bytes_of_cleartext_data
                 << ", cipher_bytes: " << bytes_of_encrypted_data);
    }
  } else {
    inBuffer->subsamples[0].clear_bytes = headerSize;
    inBuffer->subsamples[0].cipher_bytes = sample->sampleSizes[samplePos];
  }

  inBuffer->timestamp = *globalTimestamp;
  *globalTimestamp += sample->defaultSampleDuration;
}
