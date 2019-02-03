#include "adapterInterface.h"

#include "components.h"
#include "widevineAdapter.h"
#include "networking.h"
#include "video.h"
#include "audio.h"
#include "widevineDecryptor.h"


class AdapterInterface::Impl {
public:
  bool Init();
  void Close();

  bool CreateSessionAndGenerateRequest(const uint8_t *initData,
                                       uint32_t initDataSize, STREAM_TYPE type);
  void SetServerCertificate(const uint8_t *certData, uint32_t certDataSize);
  void UpdateCurrentSession(const uint8_t *data, uint32_t dataSize,
                            STREAM_TYPE type);

  std::string GetSessionMessage(STREAM_TYPE type);
  int ValidKeyIdsSize(STREAM_TYPE type);

  bool InitStream(std::string &videoUrl,
                  std::vector<std::string> &videoRangeList,
                  const uint8_t *videoInitData, uint32_t videoInitDataSize,
                  const uint8_t *codecPrivateData,
                  uint32_t codecPrivateDataSize, std::string &audioUrl,
                  std::vector<std::string> &audioRangeList,
                  const uint8_t *audioInitData, uint32_t audioInitDataSize);

  bool StartStream();
  void WaitForStream();
  bool StopStream();

  const char *GetCdmVersion();

private:
  bool isInitialized_ = false;
  bool decoderNotInitializedYet_ = false;
  bool audioDecoderNotInitializedYet_ = false;
  bool streamInitialized_ = false;

  std::shared_ptr<WidevineAdapter> videoAdapter_;
  std::shared_ptr<WidevineAdapter> audioAdapter_;

  NetworkingThread videoNetThread_;
  DemuxVideoThread demuxVideoThread_;
  DecryptVideoThread decryptVideoThread_;
  VideoThread renderVideoThread_;

  NetworkingThread audioNetThread_;
  DemuxAudioThread demuxAudioThread_;
  WidevineDecryptorThread decryptAudioThread_;
  DecodeAudioThread decodeAudioThread_;
  // NOTE: decrypt and decode (not implemented yet)
  // DecryptAudioThread decryptAudioThread_;
  // NOTE: decrypt only
  AudioThread renderAudioThread_;

  std::shared_ptr<MP4VideoStream> videoStream_;
  std::shared_ptr<MP4AudioStream> audioStream_;
};

bool AdapterInterface::Impl::Init() {
  if (isInitialized_) {
    // prevent double initialization
    return true;
  }

  videoAdapter_ = wvAdapterLib::GetVideoAdapter();
  bool ret = videoAdapter_->Init();
  if (!ret) {
    return false;
  }

  audioAdapter_ = wvAdapterLib::GetAudioAdapter();
  ret = audioAdapter_->Init();
  if (!ret) {
    return false;
  }

  // TODO set these values programatically, at least for the Raspberry Pi
  //     also take video resolution into account,
  //     e.g. full hd needs more memory than 512x288 --> use smaller queue sizes
  wvAdapterLib::GetDemuxedVideoSampleQueue()->SetUpperBound(DEMUX_VIDEO_SAMPLE_QUEUE_MAX_SIZE);
  wvAdapterLib::GetVideoFrameQueue()->SetUpperBound(VIDEO_FRAME_QUEUE_MAX_SIZE);
  wvAdapterLib::GetDemuxedAudioSampleQueue()->SetUpperBound(DEMUX_AUDIO_SAMPLE_QUEUE_MAX_SIZE);
  wvAdapterLib::GetDecryptedAudioSampleQueue()->SetUpperBound(DEMUX_AUDIO_SAMPLE_QUEUE_MAX_SIZE);
  wvAdapterLib::GetAudioFrameQueue()->SetUpperBound(AUDIO_FRAME_QUEUE_MAX_SIZE);

  // TODO better to set this name uppon construction
  wvAdapterLib::GetDemuxedVideoSampleQueue()->SetName(DEMUX_VIDEO_SAMPLE_QUEUE_NAME);
  wvAdapterLib::GetVideoFrameQueue()->SetName(VIDEO_FRAME_QUEUE_NAME);
  wvAdapterLib::GetDemuxedAudioSampleQueue()->SetName(DEMUX_AUDIO_SAMPLE_QUEUE_NAME);
  wvAdapterLib::GetDecryptedAudioSampleQueue()->SetName(DECRYPTED_AUDIO_SAMPLE_QUEUE_NAME);
  wvAdapterLib::GetAudioFrameQueue()->SetName(AUDIO_FRAME_QUEUE_NAME);

  // Note: Must be kept in sync with mappings above
  wvAdapterLib::GetQueueCounter()->RegisterQueue(DEMUX_VIDEO_SAMPLE_QUEUE_NAME, DEMUX_VIDEO_SAMPLE_QUEUE_MAX_SIZE);
  wvAdapterLib::GetQueueCounter()->RegisterQueue(VIDEO_FRAME_QUEUE_NAME, VIDEO_FRAME_QUEUE_MAX_SIZE);
  wvAdapterLib::GetQueueCounter()->RegisterQueue(DEMUX_AUDIO_SAMPLE_QUEUE_NAME, DEMUX_AUDIO_SAMPLE_QUEUE_MAX_SIZE);
  wvAdapterLib::GetQueueCounter()->RegisterQueue(DECRYPTED_AUDIO_SAMPLE_QUEUE_NAME, DEMUX_AUDIO_SAMPLE_QUEUE_MAX_SIZE);
  wvAdapterLib::GetQueueCounter()->RegisterQueue(AUDIO_FRAME_QUEUE_NAME, AUDIO_FRAME_QUEUE_MAX_SIZE);

  wvAdapterLib::GetQueueCounter()->RegisterQueue(VIDEO_NET_STREAM_NAME, BLOCKING_STREAM_MAX_NETWORK_FRAGMENTS);
  wvAdapterLib::GetQueueCounter()->RegisterQueue(AUDIO_NET_STREAM_NAME, BLOCKING_STREAM_MAX_NETWORK_FRAGMENTS);

  isInitialized_ = true;
  return true;
}

void AdapterInterface::Impl::Close() {
  if (isInitialized_) {
    // TODO move this out here ---> components
    wvAdapterLib::GetVideoRenderer()->Close();
    wvAdapterLib::GetAudioRenderer()->Close();

    decoderNotInitializedYet_ = false; // default
    audioDecoderNotInitializedYet_ = false;
    videoAdapter_->Shutdown();
    audioAdapter_->Shutdown();
    streamInitialized_ = false;
    isInitialized_ = false;
  }
}

bool AdapterInterface::Impl::CreateSessionAndGenerateRequest(
    const uint8_t *initData, uint32_t initDataSize,
    AdapterInterface::STREAM_TYPE type) {
  if (!isInitialized_) {
    // check if initialized
    return false;
  }

  if (type == AdapterInterface::STREAM_TYPE::kAudio) {
    PRINT_BYTE_ARRAY(
        "(AUDIO) CreateSessionAndGenerateRequest called with data :", initData,
        initDataSize);

    audioAdapter_->CreateSessionAndGenerateRequest(initData, initDataSize);
  } else {
    PRINT_BYTE_ARRAY(
        "(VIDEO) CreateSessionAndGenerateRequest called with data :", initData,
        initDataSize);

    videoAdapter_->CreateSessionAndGenerateRequest(initData, initDataSize);
  }

  return true;
}

void AdapterInterface::Impl::SetServerCertificate(const uint8_t *certData,
                                                  uint32_t certDataSize) {
  if (!isInitialized_) {
    // check if initialized
    return;
  }
  videoAdapter_->SetServerCertificate(certData, certDataSize);
}

void AdapterInterface::Impl::UpdateCurrentSession(
    const uint8_t *data, uint32_t dataSize,
    AdapterInterface::STREAM_TYPE type) {
  if (!isInitialized_) {
    // check if initialized
    return;
  }

  if (type == AdapterInterface::STREAM_TYPE::kAudio) {
    audioAdapter_->UpdateSession(data, dataSize);
  } else {
    videoAdapter_->UpdateSession(data, dataSize);
  }
}

std::string AdapterInterface::Impl::GetSessionMessage(STREAM_TYPE type) {
  if (type == STREAM_TYPE::kAudio) {
    if (!audioAdapter_->IsMessageValid()) {
      return "";
    }
    return audioAdapter_->GetMessage();
  } else {
    if (!videoAdapter_->IsMessageValid()) {
      return "";
    }
    return videoAdapter_->GetMessage();
  }
}

int AdapterInterface::Impl::ValidKeyIdsSize(STREAM_TYPE type) {
  if (type == STREAM_TYPE::kAudio) {
    return audioAdapter_->ValidKeyIdsSize();
  } else {
    return videoAdapter_->ValidKeyIdsSize();
  }
}

bool AdapterInterface::Impl::InitStream(
    std::string &videoUrl, std::vector<std::string> &videoRangeList,
    const uint8_t *videoInitData, uint32_t videoInitDataSize,
    const uint8_t *codecPrivateData, uint32_t codecPrivateDataSize,
    std::string &audioUrl, std::vector<std::string> &audioRangeList,
    const uint8_t *audioInitData, uint32_t audioInitDataSize) {
  cdm::Status ret;
  bool result;
  if (!isInitialized_) {
    // check if initialized
    return false;
  }

#ifndef DISABLE_VIDEO
  videoStream_ = wvAdapterLib::GetMp4VideoStream();
  result = videoStream_->Init(videoUrl, videoRangeList, videoInitData,
                              videoInitDataSize, codecPrivateData,
                              codecPrivateDataSize);
  if (!result) {
    ERROR_PRINT("Init Video Stream failed");
    return false;
  }

  ret = videoAdapter_->InitCdmVideoDecoder(
      cdm::VideoDecoderConfig::VideoCodec::kCodecH264, /*only support h264*/
      videoStream_->GetProfile(),
      cdm::VideoFormat::kYv12, /* default TODO make configurable */
      videoStream_->GetVideoWidth(), videoStream_->GetVideoHeight(),
      /*Inject the sps/pps data here so it has not be done into the demuxed
         data*/
      videoStream_->GetVideoExtraData(), videoStream_->GetVideoExtraDataSize());
  if (ret != cdm::Status::kSuccess &&
      ret != cdm::Status::kDeferredInitialization) {
    return false;
  }
  if (ret == cdm::Status::kDeferredInitialization) {
    WARN_PRINT("Init CDM video decoder warning: kDeferredInitialization");
    videoAdapter_->ResetVideoDecoder();
    decoderNotInitializedYet_ = true;
    // TODO add later init
    return false;
  }

  VideoRendererConfig videoCfg;
  videoCfg.width = videoStream_->GetVideoWidth();
  videoCfg.height = videoStream_->GetVideoHeight();
  // To get the real encoding we would have to wait for the first frame
  // so let this as a default
  // Use same default encoding as in WidevineAdapter::InitCdmVideoDecoder()
  videoCfg.encoding = VIDEO_FORMAT_ENUM::kYv12;
  if (!wvAdapterLib::GetVideoRenderer()->Open(&videoCfg)) {
    return false;
  }
#endif /* DISABLE_VIDEO */

#ifndef DISABLE_AUDIO
  audioStream_ = wvAdapterLib::GetMp4AudioStream();
  result = audioStream_->Init(audioUrl, audioRangeList, audioInitData,
                              audioInitDataSize, nullptr, 0);
  if (!result) {
    ERROR_PRINT("Init Audio Stream failed");
    return false;
  }

  AudioRendererConfig audioCfg;
  audioCfg.channels = audioStream_->GetChannels();
  audioCfg.samplingRate = audioStream_->GetSamplingRate();
  audioCfg.sampleSize = audioStream_->GetSampleSize();
  if (!wvAdapterLib::GetAudioRenderer()->Open(&audioCfg)) {
    return false;
  }

  /*  XXX this fails with session error ...
  ret = audioAdapter_.InitCdmAudioDecoder(
  cdm::AudioDecoderConfig::AudioCodec::kCodecAac,
  audioStream_.GetChannels(),
  audioStream_.GetSamplingRate(),
  audioStream_.GetSampleSize(),
  nullptr, 0
);
  if (ret != cdm::Status::kSuccess &&
      ret != cdm::Status::kDeferredInitialization) {
    return false;
  }
  if (ret == cdm::Status::kDeferredInitialization) {
    WARN_PRINT("Init CDM audio decoder warning: kDeferredInitialization");
    audioCdm_->ResetDecoder(cdm::StreamType::kStreamTypeAudio);
    audioDecoderNotInitializedYet_ = true;
    // TODO add later init
    return false;
  }
  */
#endif /* DISABLE_AUDIO */

  streamInitialized_ = true;

  return true;
}

bool AdapterInterface::Impl::StartStream() {
  if (!isInitialized_ || !streamInitialized_) {
    ERROR_PRINT("Either the adapter and/or the stream was not initialized.");
    return false;
  }

  // start all threads
#ifndef DISABLE_VIDEO
  videoNetThread_.Start("videoNetThread", MEDIA_TYPE::kVideo, videoStream_->GetMediaFileUrl(), videoStream_->GetRangeList());
  demuxVideoThread_.Start("demuxVideoThread");
  decryptVideoThread_.Start("decryptVideoThread");
  renderVideoThread_.Start("renderVideoThread");
#endif /* DISABLE_VIDEO */

#ifndef DISABLE_AUDIO
  audioNetThread_.Start("audioNetThread", MEDIA_TYPE::kAudio, audioStream_->GetMediaFileUrl(),
                        audioStream_->GetRangeList());
  demuxAudioThread_.Start("demuxAudioThread");
  if (audioStream_->isProtected()) {
    decryptAudioThread_.Start("decryptAudioThread", MEDIA_TYPE::kAudio);
    decodeAudioThread_.Start("decodeAudioThread");

  } else {
    decodeAudioThread_.Start("decodeAudioThread");
  }

  renderAudioThread_.Start("renderAudioThread");
#endif /* DISABLE_AUDIO */

  return true;
}

void AdapterInterface::Impl::WaitForStream() {
  if (!isInitialized_ || !streamInitialized_) {
    ERROR_PRINT("Either the adapter and/or the stream was not initialized.");
    return;
  }
  INFO_PRINT("AdapterInterface::Impl::WaitForStream()");

  // sync threads
#ifndef DISABLE_VIDEO
  videoNetThread_.Join();
  demuxVideoThread_.Join();
  decryptVideoThread_.Join();
  renderVideoThread_.Join();
#endif /* DISABLE_VIDEO */

#ifndef DISABLE_AUDIO
  audioNetThread_.Join();
  demuxAudioThread_.Join();
  if (audioStream_->isProtected()) {
    decryptAudioThread_.Join();
    decodeAudioThread_.Join();
  } else {
    decodeAudioThread_.Join();
  }
  renderAudioThread_.Join();
#endif /* DISABLE_AUDIO */
}

bool AdapterInterface::Impl::StopStream() {
  if (!isInitialized_ || !streamInitialized_) {
    ERROR_PRINT("Either the adapter and/or the stream was not initialized.");
    return false;
  }

  // stop reader and all threads
  wvAdapterLib::GetClock()->Shutdown();

#ifndef DISABLE_VIDEO
  videoNetThread_.Shutdown();
  demuxVideoThread_.Shutdown();
  decryptVideoThread_.Shutdown();
  renderVideoThread_.Shutdown();

  wvAdapterLib::GetVideoNetStream()->Shutdown();
  wvAdapterLib::GetDemuxedVideoSampleQueue()->Shutdown();
  wvAdapterLib::GetVideoFrameQueue()->Shutdown();

  videoStream_->Close();
#endif /* DISABLE_VIDEO */

#ifndef DISABLE_AUDIO
  audioNetThread_.Shutdown();
  demuxAudioThread_.Shutdown();
  if (audioStream_->isProtected()) {
    decryptAudioThread_.Shutdown();
    decodeAudioThread_.Shutdown();
  } else {
    decodeAudioThread_.Shutdown();
  }
  renderAudioThread_.Shutdown();

  // TODO move this out --> components
  wvAdapterLib::GetAudioNetStream()->Shutdown();
  wvAdapterLib::GetDemuxedAudioSampleQueue()->Shutdown();
  wvAdapterLib::GetDecryptedAudioSampleQueue()->Shutdown();
  wvAdapterLib::GetAudioFrameQueue()->Shutdown();

  audioStream_->Close();
#endif /* DISABLE_AUDIO */

  return true;
}

const char *AdapterInterface::Impl::GetCdmVersion() {
  return videoAdapter_->GetCdmVersion();
}

//
// AdapterInterface
//
AdapterInterface::AdapterInterface() : pImpl_(new AdapterInterface::Impl()) {}
AdapterInterface::~AdapterInterface() = default;

bool AdapterInterface::Init() { return pImpl_->Init(); }
void AdapterInterface::Close() { pImpl_->Close(); }

bool AdapterInterface::CreateSessionAndGenerateRequest(
    const uint8_t *initData, uint32_t initDataSize, STREAM_TYPE type) {
  return pImpl_->CreateSessionAndGenerateRequest(initData, initDataSize, type);
}
void AdapterInterface::SetServerCertificate(const uint8_t *certData, uint32_t certDataSize) {
  pImpl_->SetServerCertificate(certData, certDataSize);
}
void AdapterInterface::UpdateCurrentSession(const uint8_t *data, uint32_t dataSize,
                                            AdapterInterface::STREAM_TYPE type) {
  pImpl_->UpdateCurrentSession(data, dataSize, type);
}

std::string AdapterInterface::GetSessionMessage(AdapterInterface::STREAM_TYPE type) {
  return pImpl_->GetSessionMessage(type);
}
int AdapterInterface::ValidKeyIdsSize(AdapterInterface::STREAM_TYPE type) { return pImpl_->ValidKeyIdsSize(type); }

bool AdapterInterface::InitStream(std::string &videoUrl,
                std::vector<std::string> &videoRangeList,
                const uint8_t *videoInitData, uint32_t videoInitDataSize,
                const uint8_t *codecPrivateData,
                uint32_t codecPrivateDataSize, std::string &audioUrl,
                std::vector<std::string> &audioRangeList,
                const uint8_t *audioInitData, uint32_t audioInitDataSize) {
  return pImpl_->InitStream(videoUrl, videoRangeList, videoInitData, videoInitDataSize,
                            codecPrivateData, codecPrivateDataSize,
                            audioUrl, audioRangeList, audioInitData, audioInitDataSize);
}

bool AdapterInterface::StartStream() { return pImpl_->StartStream(); }
void AdapterInterface::WaitForStream() { pImpl_->WaitForStream(); }
bool AdapterInterface::StopStream() { return pImpl_->StopStream(); }

const char *AdapterInterface::GetCdmVersion() { return pImpl_->GetCdmVersion(); }

void AdapterInterface::SetLogLevel(const logging::LogLevel &level) {
  logging::setLogLevel(level);
}

void AdapterInterface::PrintQueueCounterSummary() {
  const int arrSize = 7;
  const char* queueNames[] = {VIDEO_NET_STREAM_NAME, AUDIO_NET_STREAM_NAME,
                              DEMUX_VIDEO_SAMPLE_QUEUE_NAME, VIDEO_FRAME_QUEUE_NAME,
                              DEMUX_AUDIO_SAMPLE_QUEUE_NAME, DECRYPTED_AUDIO_SAMPLE_QUEUE_NAME,
                              AUDIO_FRAME_QUEUE_NAME};
  QueueCounterSummary summaries[arrSize];
  INFO_PRINT("-----------------------------------------");
#ifndef DISABLE_QUEUE_COUNTERS
  for(size_t i = 0; i < arrSize; i++) {
    wvAdapterLib::GetQueueCounter()->GetSummary(queueNames[i], summaries[i]);
    INFO_PRINT("Summary for Queue '" << queueNames[i] << ":\n" <<
               summaries[i].toString());
  }
#else
  INFO_PRINT("Queue counters were disabled in this build. So no queue counter summary can be printed.");
#endif // DISABLE_QUEUE_COUNTERS
  INFO_PRINT("-----------------------------------------");
}

void AdapterInterface::StopPlayback() {
  wvAdapterLib::GetAudioFrameQueue()->Stop();
  wvAdapterLib::GetVideoFrameQueue()->Stop();
}

void AdapterInterface::ResumePlayback() {
  wvAdapterLib::GetVideoFrameQueue()->Resume();
  wvAdapterLib::GetAudioFrameQueue()->Resume();
}