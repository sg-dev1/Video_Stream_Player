#include "video.h"
#include "assertion.h"
#include "content_decryption_module.h"
#include "logging.h"
#include "util.h"
#include "widevineDecryptor.h"

#include <thread>

#include "components.h"

//
// DemuxVideoThread
//
class DemuxVideoThread::Impl {
public:
  void Start(const std::string &name);
  void Join() { thread_.join(); };
  void Shutdown() { isRunning_ = false; };

private:
  void Run();

  bool ProcessSampleDataInPlace(uint8_t *inoutData, uint32_t inoutDataSize);
  bool ProcessSample(uint8_t *inputData, uint32_t inputDataSize,
                     uint8_t *outputData, uint32_t outputDataSize,
                     uint32_t *outputDataSizePtr, unsigned int subsampleCount,
                     const uint16_t *bytes_of_cleartext_data,
                     const uint32_t *bytes_of_encrypted_data);

  std::thread thread_;
  bool isRunning_;
  std::string name_;

  std::shared_ptr<BlockingQueue<std::unique_ptr<DEMUXED_SAMPLE>>> output_;
  std::shared_ptr<MP4VideoStream> stream_;

  uint16_t *bytes_of_cleartext_data_ = nullptr;
  uint32_t *bytes_of_encrypted_data_ = nullptr;
  unsigned int bytesOfDataSize_ = 0;
};

void DemuxVideoThread::Impl::Start(const std::string &name) {
  name_ = name;

  output_ = wvAdapterLib::GetDemuxedVideoSampleQueue();
  stream_ = wvAdapterLib::GetMp4VideoStream();

  isRunning_ = true;
  thread_ = std::thread(&DemuxVideoThread::Impl::Run, this);
}

void DemuxVideoThread::Impl::Run() {
  logging::addThreadIdNameMapping(std::this_thread::get_id(), name_.c_str());

  INFO_PRINT("Starting demux video thread...");
  bool ret;

  while (isRunning_) {
    std::unique_ptr<DEMUXED_SAMPLE> sample = stream_->ParseMoofMdat(ret);
    if (!ret) {
      isRunning_ = false;
      break;
    }

    ret = ProcessSampleDataInPlace(sample->mdatRawData, sample->mdatRawDataSize);
    if (!ret) {
      isRunning_ = false;
      ASSERT(false);
      break;
    }

    // save data
    output_->push(std::move(sample));
  }

  INFO_PRINT("Finishing demux video thread");
}

bool DemuxVideoThread::Impl::ProcessSampleDataInPlace(uint8_t *inoutData,
                                                uint32_t inoutDataSize) {
  if (stream_->GetNaluLengthSize() != 4) {
    ERROR_PRINT("This algorithm only works for nalu length == 4");
    return false;
  }
  uint8_t *inoutDataEnd = inoutData + inoutDataSize;
  uint8_t *inoutDataPtr2 = inoutData;

  while (inoutData < inoutDataEnd) {
    // PRINT_BYTE_ARRAY("inoutData[*]", inoutData, 4);
    uint32_t nalsize = 0;
    for (unsigned int i = 0; i < 4 /* nal size */; i++) {
      nalsize = (nalsize << 8) + *inoutDataPtr2++;
    }
    // add nalu header
    inoutData[0] = inoutData[1] = inoutData[2] = 0;
    inoutData[3] = 1;
    inoutData += 4 + nalsize;
    inoutDataPtr2 += nalsize;
  }

  if (inoutData != inoutDataEnd) {
    ERROR_PRINT("NAL unit definition incomplete inoutData != inoutDataEnd: "
                    << static_cast<unsigned int>(inoutData - inoutDataEnd));
    return false;
  }

  return true;
}

/*
 *
 * Output data space requirements:
 *  inputDataSize +
 *  sps/pps size  +                     [optional]
 *  (4 - naluLength) * nalunitcount     (mostly this will be zero when
 *                                       naluLength == 4)
 *
 *  XXX this function must be adapted to support nalu length's != 4
 *      for nalu length's == 4 ProcessSampleDataInPlace() is fine
 *  NOTE sps/pps data need not be injected here since it can be given to the
 *       cdm's decoder (see
 *         widevineAdapter.cc#AdapterInterface::InitCdmVideoDecoder() )
 *  NOTE there was an issue with nalusize != size_of_a_single_sample which
 *       triggered the error condition
 *        (nalsize + stream_->GetNaluLengthSize() + nalunitsum >
 *            *bytes_of_cleartext_data + *bytes_of_encrypted_data)
 *       so it is best to ignore the subsampleCount, bytes_of_cleartext_data
 *       etc. as done in ProcessSampleDataInPlace()
 *
 */
bool DemuxVideoThread::Impl::ProcessSample(uint8_t *inputData, uint32_t inputDataSize,
                                     uint8_t *outputData,
                                     uint32_t outputDataSize,
                                     uint32_t *outputDataSizePtr,
                                     unsigned int subsampleCount,
                                     const uint16_t *bytes_of_cleartext_data,
                                     const uint32_t *bytes_of_encrypted_data) {
  // NAL unit handling: transform transport into bytestream format
  // https://yumichan.net/video-processing/video-compression/introduction-to-h264-nal-unit/
  // https://github.com/peak3d/inputstream.adaptive/blob/master/wvdecrypter/wvdecrypter.cpp#L930
  uint8_t *inputDataEnd = inputData + inputDataSize;
  uint8_t *outputDataEnd = outputData + outputDataSize;
  unsigned int nalunitcount = 0, nalunitsum = 0, sps_pps_size = 0;

  // nalu length range checks
  if (stream_->GetNaluLengthSize() > 4) {
    ERROR_PRINT("NALU length > 4 not supported.");
    return false;
  } else if (stream_->GetNaluLengthSize() <= 0) {
    ERROR_PRINT("NALU length must be > 0.");
    return false;
  }

  while (inputData < inputDataEnd) {
    uint32_t nalsize = 0;
    PRINT_BYTE_ARRAY("inputData[*]", inputData, 430);
    for (unsigned int i = 0; i < stream_->GetNaluLengthSize(); i++) {
      nalsize = (nalsize << 8) + *inputData++;
    }
    DEBUG_PRINT("Using nalsize: " << nalsize);
    DEBUG_PRINT("Using nalunitsum: " << nalunitsum);

    if (outputData + 4 + nalsize + stream_->GetVideoExtraDataSize() >
        outputDataEnd) {
      // NOTE here we are pessimistic and assume that the next if will get
      // executed (copying the extradata)
      // NOTE if it was already executed extradata's size will be 0
      ERROR_PRINT("Too small output data array given. Would get out of range "
                      "in next iteration.");
      return false;
    }

    // check if sequence parameters/ picture paramters (sps/pps) have to be
    // injected
    if (stream_->GetVideoExtraData() &&
        (*inputData & 0x1F) != 9 /*AVC_NAL_AUD*/) {
      DEBUG_PRINT("Copying sps/pps data of size "
                      << stream_->GetVideoExtraDataSize());
      memcpy(outputData, stream_->GetVideoExtraData(),
             stream_->GetVideoExtraDataSize());
      outputData += stream_->GetVideoExtraDataSize();
      sps_pps_size = stream_->GetVideoExtraDataSize();
    }

    // nalu header
    outputData[0] = outputData[1] = outputData[2] = 0;
    outputData[3] = 1;
    outputData += 4;
    // nalu payload
    memcpy(outputData, inputData, nalsize);
    outputData += nalsize;
    inputData += nalsize;
    nalunitcount++;

    if (nalsize + stream_->GetNaluLengthSize() + nalunitsum >
        *bytes_of_cleartext_data + *bytes_of_encrypted_data) {
      ERROR_PRINT(
          "NAL Unit exceeds subsample definition (nalu length size: "
              << static_cast<unsigned int>(stream_->GetNaluLengthSize()) << ") "
              << static_cast<unsigned int>(nalsize + stream_->GetNaluLengthSize() +
                                           nalunitsum)
              << " -> " << (*bytes_of_cleartext_data + *bytes_of_encrypted_data)
              << " ");
      return false;
    } else if (nalsize + stream_->GetNaluLengthSize() + nalunitsum ==
               *bytes_of_cleartext_data + *bytes_of_encrypted_data) {
      DEBUG_PRINT("advancing to next subsample");
      bytes_of_cleartext_data++;
      bytes_of_encrypted_data++;
      subsampleCount--;
      nalunitsum = 0;
    } else {
      DEBUG_PRINT("staying at current subsample");
      nalunitsum = nalsize + stream_->GetNaluLengthSize();
    }
  }

  if (inputData != inputDataEnd || subsampleCount) {
    ERROR_PRINT("NAL Unit definition incomplete (nalu length size: "
                    << static_cast<unsigned int>(stream_->GetNaluLengthSize())
                    << ") " << static_cast<unsigned int>(inputData - inputDataEnd)
                    << " -> " << subsampleCount << " ");
    return false;
  }

  // calculate how much data was copied and store it in outputDataSize
  // NOTE the '4' is needed because if nalu length is 4, the input data also
  // has a header of length 4 (needed to retrieve the nalsize)
  *outputDataSizePtr = inputDataSize + sps_pps_size +
                       (4 - stream_->GetNaluLengthSize()) * nalunitcount;

  return true;
}

DemuxVideoThread::DemuxVideoThread() : pImpl_(new DemuxVideoThread::Impl()) {}

DemuxVideoThread::~DemuxVideoThread() = default;

void DemuxVideoThread::Start(const std::string &name) { pImpl_->Start(name); }

void DemuxVideoThread::Join() { pImpl_->Join(); }

void DemuxVideoThread::Shutdown() { pImpl_->Shutdown(); }


//
// DecryptVideoThread
//
class DecryptVideoThread::Impl {
public:
  void Start(const std::string &name);
  void Join() { thread_.join(); }
  void Shutdown() { isRunning_ = false; }

private:
  void Run();

  /**
   * Function for processing a single frame.
   * If it is the first frame processed after video decoder initialization,
   * codecPrivateData must be passed as extraData.
   *
   * @param pos The position of the frame to process (decrypt+decode)
   * @param sample The sample which contains this frame
   * @param dataToDecrypt A pointer to the data which should be decrypted.
   *                      This contains cleartext as well as encrypted data.
   * @param extraData Some extra data which is appended to dataToDecrypt.
   *                  May be a nullptr (then extraDataSize is 0).
   *                  E.g. used to pass the codecPrivateData as well.
   * @param extraDataSize Size of the extraData.
   * @param dataToDecryptSize Pointer to an integer which contains the size of
   *                          the frame at position pos (clear + encrypted
   * data).
   *                          It does not contain the extraData.
   *
   * @return 0 in case of success, 1 in case of cdm::Status::kNeedMoreData
   * error,
   *         -1 in case of any other error
   */
  int ProcessFrame(size_t pos, std::unique_ptr<DEMUXED_SAMPLE> &sample, uint8_t *dataToDecrypt,
                   const uint8_t *extraData, size_t extraDataSize,
                   uint32_t *dataToDecryptSize);

  std::thread thread_;
  bool isRunning_;
  std::string name_;

  std::shared_ptr<BlockingQueue<std::unique_ptr<DEMUXED_SAMPLE>>> input_;
  std::shared_ptr<BlockingQueue<VIDEO_FRAME>> output_;
  std::shared_ptr<WidevineAdapter> videoAdapter_;
  std::shared_ptr<MP4VideoStream> stream_;

  // @ FillCdmInBuffer
  uint64_t globalTimestamp_;
  cdm::SubsampleEntry *subsampleBuffer_ = nullptr;
  unsigned int maxSubSampleCount_ = 0;
};

void DecryptVideoThread::Impl::Start(const std::string &name) {
  name_ = name;
  isRunning_ = true;
  globalTimestamp_ = 0;

  input_ = wvAdapterLib::GetDemuxedVideoSampleQueue();
  output_ = wvAdapterLib::GetVideoFrameQueue();
  videoAdapter_ = wvAdapterLib::GetVideoAdapter();
  stream_ = wvAdapterLib::GetMp4VideoStream();

  thread_ = std::thread(&DecryptVideoThread::Impl::Run, this);
}

void DecryptVideoThread::Impl::Run() {
  logging::addThreadIdNameMapping(std::this_thread::get_id(), name_.c_str());

  INFO_PRINT("Starting decrypt thread...");

  while (isRunning_) {
    std::unique_ptr<DEMUXED_SAMPLE> sample = input_->pop();

    uint32_t dataToDecryptSize;
    uint8_t *dataToDecrypt = sample->mdatRawData;

    //
    // process first frame specially
    //
    int ret = ProcessFrame(
        0, sample, sample->mdatRawData, stream_->GetCodecPrivateData(),
        stream_->GetCodecPrivateDataSize(), &dataToDecryptSize);
    if (ret == -1) {
      // error case
      return;
    }
    DEBUG_PRINT("dataToDecryptSize: " << dataToDecryptSize);
    dataToDecrypt += dataToDecryptSize;

    //
    // decrypt the rest
    //
    uint32_t errors = 0;
    for (size_t i = 1; i < sample->sampleInfoTable->GetSampleCount(); i++) {
      dataToDecryptSize = 0;
      ret = ProcessFrame(i, sample, dataToDecrypt, nullptr, 0,
                         &dataToDecryptSize);
      if (ret == -1) {
        // error case
        return;
      } else if (ret == 1) {
        errors++;
      }
      dataToDecrypt += dataToDecryptSize;
    }

    if (errors > 0) {
      INFO_PRINT("CDM DecryptAndDecodeFrame produced "
                     << errors << " kNeedMoreData errors.");
    }
  }

  INFO_PRINT("Finishing decrypt thread");
}

int DecryptVideoThread::Impl::ProcessFrame(size_t pos, std::unique_ptr<DEMUXED_SAMPLE> &sample,
                                           uint8_t *dataToDecrypt,
                                           const uint8_t *extraData,
                                           size_t extraDataSize,
                                           uint32_t *dataToDecryptSize) {

  uint32_t _dataToDecryptSize = 0;
  for (size_t j = 0; j < sample->sampleInfoTable->GetSubsampleCount(pos); j++) {
    uint16_t bytes_of_cleartext_data;
    uint32_t bytes_of_encrypted_data;
    sample->sampleInfoTable->GetSubsampleInfo(pos, j, bytes_of_cleartext_data,
                                             bytes_of_encrypted_data);

    _dataToDecryptSize += bytes_of_cleartext_data + bytes_of_encrypted_data;
  }
  // NOTE the extra data (e.g. a codec's private data) must not be counted since
  // is not part of the DEMUXED_SAMPLE's data
  *dataToDecryptSize = _dataToDecryptSize;

  // handle extra data
  if (extraDataSize > 0) {
    uint8_t *mdataData = dataToDecrypt;
    uint32_t mdataDataSize = _dataToDecryptSize;

    _dataToDecryptSize += extraDataSize;
    dataToDecrypt = new uint8_t[_dataToDecryptSize];
    memcpy(dataToDecrypt, extraData, extraDataSize);
    memcpy(dataToDecrypt + extraDataSize, mdataData, mdataDataSize);
  }

  // INFO_PRINT("Processing sample # " << pos);
  cdm::InputBuffer inBuffer;
  FillCdmInBuffer(sample, &inBuffer, pos, dataToDecrypt, _dataToDecryptSize,
                  stream_, &maxSubSampleCount_, &subsampleBuffer_,
                  &globalTimestamp_, 0 /* headerSize */);

  if (extraDataSize > 0) {
    // update clear bytes since there is extra data
    inBuffer.subsamples[0].clear_bytes += extraDataSize;
    DEBUG_PRINT("Updated clear bytes of first subsample to "
               << inBuffer.subsamples[0].clear_bytes);
  }

  WidevineVideoFrame frame;
  cdm::Status ret = videoAdapter_->DecryptAndDecodeFrame(inBuffer, &frame);

  int result = 0;
  if (ret == cdm::Status::kSuccess) {
    DEBUG_PRINT("Got new decrypted frame!");

    VIDEO_FRAME frameToSave;
    if (frame.Format() == cdm::VideoFormat::kYv12) {
      frameToSave.encoding = VIDEO_FORMAT_ENUM::kYv12;
    } else if (frame.Format() == cdm::VideoFormat::kI420) {
      frameToSave.encoding = VIDEO_FORMAT_ENUM::kI420;
    } else {
      frameToSave.encoding = VIDEO_FORMAT_ENUM::kUnknownVideoFormat;
    }

    frameToSave.width = frame.Size().width;
    frameToSave.height = frame.Size().height;

    frameToSave.buffer = frame.FrameBuffer()->Data();
    frameToSave.bufferSize = frame.FrameBuffer()->Size();
    frame.SetFrameBuffer(nullptr);

    frameToSave.y_plane_offset =
        frame.PlaneOffset(cdm::VideoFrame::VideoPlane::kYPlane);
    frameToSave.u_plane_offset =
        frame.PlaneOffset(cdm::VideoFrame::VideoPlane::kUPlane);
    frameToSave.v_plane_offset =
        frame.PlaneOffset(cdm::VideoFrame::VideoPlane::kVPlane);
    frameToSave.y_plane_stride =
        frame.Stride(cdm::VideoFrame::VideoPlane::kYPlane);
    frameToSave.u_plane_stride =
        frame.Stride(cdm::VideoFrame::VideoPlane::kUPlane);
    frameToSave.v_plane_stride =
        frame.Stride(cdm::VideoFrame::VideoPlane::kVPlane);

    if (frameToSave.encoding == VIDEO_FORMAT_ENUM::kYv12 &&
        frameToSave.u_plane_offset < frameToSave.v_plane_offset) {
      // Widevine seems to set the encoding wrongly ...
      // XXX this could also be cause due to the fact that the videodecoder
      // is initialized with a hardcoded cdm::VideoFormat::kYv12 format
      // (see widevineAdapter.cc#AdapterInterface::InitCdmVideoDecoder)
      WARN_PRINT("Widevine seemed to set endcoding wrongly to Yv12");
      frameToSave.encoding = VIDEO_FORMAT_ENUM::kI420;
    }

    frameToSave.timestamp = frame.Timestamp();
    frameToSave.frameDuration =
        (double)sample->defaultSampleDuration / stream_->GetTimeScale();

    // save frame
    output_->push(frameToSave);
  } else if (ret != cdm::Status::kNeedMoreData) {
    ERROR_PRINT("CDM DecryptAndDecodeFrame failed with " << ret);
    result = -1;
  } else {
    result = 1;
  }

  if (extraDataSize > 0) {
    // clean up allocated dataToDecrypt array
    delete[] dataToDecrypt;
  }

  return result;
}

DecryptVideoThread::DecryptVideoThread() : pImpl_(new DecryptVideoThread::Impl()) {}

DecryptVideoThread::~DecryptVideoThread() = default;

void DecryptVideoThread::Start(const std::string &name) { pImpl_->Start(name); }

void DecryptVideoThread::Join() { pImpl_->Join(); }

void DecryptVideoThread::Shutdown() { pImpl_->Shutdown(); }


//
// VideoThread
//
class VideoThread::Impl {
public:
  void Start(const std::string &name);
  void Join() { thread_.join(); };
  void Shutdown() { isRunning_ = false; }

private:
  void Run();

  std::thread thread_;
  bool isRunning_;
  std::string name_;
  std::shared_ptr<BlockingQueue<VIDEO_FRAME>> input_;

  std::shared_ptr<Renderer> videoRenderer_;
  std::shared_ptr<Clock> clock_;
};

void VideoThread::Impl::Start(const std::string &name) {
  name_ = name;

  input_ = wvAdapterLib::GetVideoFrameQueue();
  videoRenderer_ = wvAdapterLib::GetVideoRenderer();
  clock_ = wvAdapterLib::GetClock();

  isRunning_ = true;
  thread_ = std::thread(&VideoThread::Impl::Run, this);
}

void VideoThread::Impl::Run() {
  logging::addThreadIdNameMapping(std::this_thread::get_id(), name_.c_str());

  INFO_PRINT("Starting render video thread...");

  while (isRunning_) {
    clock_->SetStartTime(Clock::MEDIA_TYPE::VIDEO);
    VIDEO_FRAME frameToRender = input_->pop();

    videoRenderer_->Render(&frameToRender);

    clock_->Sync(frameToRender.frameDuration);
    // NOTE for now we are just deleting the memory at this point
    delete[] frameToRender.buffer;
  }

  INFO_PRINT("Finishing render video thread");
}

VideoThread::VideoThread() : pImpl_(new VideoThread::Impl()) {}

VideoThread::~VideoThread() = default;

void VideoThread::Start(const std::string &name) { pImpl_->Start(name); }

void VideoThread::Join() { pImpl_->Join(); }

void VideoThread::Shutdown() { pImpl_->Shutdown(); }
