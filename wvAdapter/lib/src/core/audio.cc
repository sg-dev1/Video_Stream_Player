#include "audio.h"
#include "assertion.h"
#include "util.h"

#include <thread>

extern "C" {
// libav headers
// e.g. found in Arch/Manjaro Linux under /usr/include/libavformat/
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
//#include <libavutil/error.h>
}

// this pulls in all required dependencies
#include "components.h"

#ifdef RASPBERRY_PI
  #include <chrono>
#endif // RASPBERRY_PI

//
// DemuxAudioThread
//
class DemuxAudioThread::Impl {
public:
  void Start(const std::string &name);
  void Join() { thread_.join(); };
  void Shutdown() { isRunning_ = false; };

private:
  void Run();

  std::thread thread_;
  bool isRunning_;
  std::string name_;
  std::shared_ptr<BlockingQueue<std::unique_ptr<DEMUXED_SAMPLE>>> output_;
  std::shared_ptr<MP4AudioStream> stream_;
};

void DemuxAudioThread::Impl::Start(const std::string &name) {
  name_ = name;
  output_ = wvAdapterLib::GetDemuxedAudioSampleQueue();
  stream_ = wvAdapterLib::GetMp4AudioStream();
  isRunning_ = true;
  thread_ = std::thread(&DemuxAudioThread::Impl::Run, this);
}

void DemuxAudioThread::Impl::Run() {
  logging::addThreadIdNameMapping(std::this_thread::get_id(), name_.c_str());

  INFO_PRINT("Starting demux audio thread...");
  bool ret;

  while (isRunning_) {
    std::unique_ptr<DEMUXED_SAMPLE> sample = stream_->ParseMoofMdat(ret);
    if (!ret) {
      isRunning_ = false;
      break;
    }

    if (stream_->isProtected() && !sample->hasSubSamples) {
      uint32_t newSize = sample->mdatRawDataSize +=
                             stream_->GetAdtsHeaderSize() * sample->sampleCount;
      auto *newRawData = new uint8_t[newSize];
      uint8_t *currPtrNew = newRawData;
      uint8_t *currPtr = sample->mdatRawData;
      // DEBUG_PRINT("Processing # " << sample.sampleCount << " samples.");
      for (size_t i = 0; i < sample->sampleCount; i++) {
        uint32_t sampleSize = sample->sampleSizes[i];
        currPtrNew = stream_->BuildAdtsHeader(currPtrNew, sampleSize);

        // copy data
        memcpy(currPtrNew, currPtr, sampleSize);
        currPtrNew += sampleSize;
        currPtr += sampleSize;
        // DEBUG_PRINT("Sample # " << i << " processed.");
      }

      sample->mdatRawData = newRawData;
      sample->mdatRawDataSize = newSize;
      sample->ptr.reset(newRawData);
    }

    // save data
    output_->push(std::move(sample));
  }

  INFO_PRINT("Finishing demux audio thread");
}

DemuxAudioThread::DemuxAudioThread() : pImpl_(new DemuxAudioThread::Impl()) {}
DemuxAudioThread::~DemuxAudioThread() = default;

void DemuxAudioThread::Start(const std::string &name) {
  pImpl_->Start(name);
}

void DemuxAudioThread::Join() {
  pImpl_->Join();
}

void DemuxAudioThread::Shutdown() {
  pImpl_->Shutdown();
}


//
// DecodeAudioThread
//
class DecodeAudioThread::Impl {
public:
  void Start(const std::string &name);
  void Join() { thread_.join(); }
  void Shutdown() {
    isRunning_ = false;
    globalTimestamp_ = 0;
    if (avCtx_) {
      avcodec_close(avCtx_);
      av_free(avCtx_);
    }
  }

private:
  void Run();

  std::thread thread_;
  bool isRunning_;
  std::string name_;

  std::shared_ptr<BlockingQueue<DECRYPTED_SAMPLE>> input_;
  std::shared_ptr<BlockingQueue<AUDIO_FRAME>> output_;
  std::shared_ptr<MP4AudioStream> stream_;
  std::shared_ptr<Renderer> audioRenderer_;

  // TODO convert to unique ptrs
  AVCodec *codec_ = nullptr;
  AVCodecContext *avCtx_ = nullptr;

  uint64_t globalTimestamp_;
};

void DecodeAudioThread::Impl::Start(const std::string &name) {
  name_ = name;

  input_ = wvAdapterLib::GetDecryptedAudioSampleQueue();
  output_ = wvAdapterLib::GetAudioFrameQueue();
  stream_ = wvAdapterLib::GetMp4AudioStream();
  audioRenderer_ = wvAdapterLib::GetAudioRenderer();

  // init libav stuff
  av_register_all();
  // NOTE AAC is hard coded for now
  codec_ = avcodec_find_decoder(AV_CODEC_ID_AAC);
  if (!codec_) {
    ERROR_PRINT("Could not find AAC codec.");
    return;
  }
  avCtx_ = avcodec_alloc_context3(codec_);
  avCtx_->channels = stream_->GetChannels();
  avCtx_->sample_rate = stream_->GetSamplingRate();
  avCtx_->sample_fmt = AV_SAMPLE_FMT_FLTP;
  // avCtx_->extradata = stream->GetDecoderSpecificInfo();
  // avCtx_->extradata_size = stream->GetDecoderSpecificInfoSize();

  DEBUG_PRINT("Using AAC codec with "
                  << avCtx_->channels << " channels and " << avCtx_->sample_rate
                  << " sample rate and number of samples (frame_size): "
                  << avCtx_->frame_size);
  int ret = avcodec_open2(avCtx_, codec_, NULL);
  if (ret < 0) {
    ERROR_PRINT("avcodec_open2 failed with code " << ret);
    ASSERT(false);
  }

  isRunning_ = true;
  globalTimestamp_ = 0;
  thread_ = std::thread(&DecodeAudioThread::Impl::Run, this);
}

void DecodeAudioThread::Impl::Run() {
  logging::addThreadIdNameMapping(std::this_thread::get_id(), name_.c_str());

  INFO_PRINT("Starting decode audio thread...");

  AVFrame *decodedFrame = av_frame_alloc();
  while (isRunning_) {
    auto sample = input_->pop();
    // DEBUG_PRINT("Processing next DEMUXED_SAMPLE...");
    AVPacket avPacket;
    av_init_packet(&avPacket);
    avPacket.size = sample.size;
    avPacket.data = sample.buffer;

    // https://blogs.gentoo.org/lu_zero/2016/03/29/new-avcodec-api/
    int ret = avcodec_send_packet(avCtx_, &avPacket);
    if (ret == AVERROR(EAGAIN)) {
      // TODO how to handle this correctly ??
      ERROR_PRINT(
          "avcodec_send_packet failed with AVERROR(EAGAIN)). Skipping packet.");
      goto wend;
    }
    if (ret < 0) {
      char errbuf[AV_ERROR_MAX_STRING_SIZE];
      av_strerror(ret, errbuf, AV_ERROR_MAX_STRING_SIZE);
      ERROR_PRINT("avcodec_send_packet failed with '" << errbuf
                                                      << "'. Skipping packet.");

      ASSERT(false);
      break;
    }

    while (!ret) {
      ret = avcodec_receive_frame(avCtx_, decodedFrame);
      if (!ret) {
        if (static_cast<uint32_t>(decodedFrame->sample_rate) !=
            stream_->GetSamplingRate()) {
          // NOTE: always check if the sample rate has changed since the AAC
          // decoder may change it.
          // see e.g.
          // https://github.com/FFmpeg/FFmpeg/blob/master/libavcodec/aacdec_template.c#L3265
          INFO_PRINT("Samping rate has changed to " << decodedFrame->sample_rate
                                                    << ". Updating...");
          stream_->SetSamplingRate(decodedFrame->sample_rate);
          AudioRendererConfig cfg;
          cfg.channels = stream_->GetChannels();
          cfg.samplingRate = stream_->GetSamplingRate();
          cfg.sampleSize = stream_->GetSampleSize();
          audioRenderer_->Reconfigure(&cfg);
        }

        // add AUDIO_FRAME to output
        AUDIO_FRAME audioFrame;
        audioFrame.channels = stream_->GetChannels();
        audioFrame.samplingRate = stream_->GetSamplingRate();
        audioFrame.sampleSize = stream_->GetSampleSize();
        audioFrame.timestamp = globalTimestamp_;
        globalTimestamp_ += sample.defaultSampleDuration;
        audioFrame.frameDuration =
            (double)sample.defaultSampleDuration / stream_->GetTimeScale();

        if (decodedFrame->format != AV_SAMPLE_FMT_FLTP) {
          ERROR_PRINT(
              "The given format "
                  << av_get_sample_fmt_name((AVSampleFormat)decodedFrame->format)
                  << " is not supported. Only AV_SAMPLE_FMT_FLTP!");
          ASSERT(false);
        }

        // conversion from fltp (float 32 bit planar) to s16_le (signed 16
        // bit integer little endian) by hand so no additional conversion
        // library is needed
        int16_t *interleave_buf;
        int data_size = sizeof(*interleave_buf) * 2 * decodedFrame->nb_samples;
        interleave_buf = (int16_t *)av_malloc(data_size);
        if (!interleave_buf)
          exit(1);
        float *inputChannel0 = (float *)decodedFrame->data[0];
        float *inputChannel1 = (float *)decodedFrame->data[1];
        for (int i = 0; i < decodedFrame->nb_samples; i++) {
          float s1 = *inputChannel0++;
          float s2 = *inputChannel1++;

          if (s1 < -1.0f)
            s1 = -1.0f;
          else if (s1 > 1.0f)
            s1 = 1.0f;
          if (s2 < -1.0f)
            s2 = -1.0f;
          else if (s2 > 1.0f)
            s2 = 1.0f;

          interleave_buf[i * 2] = (int16_t)round(s1 * 32767.0f);
          interleave_buf[i * 2 + 1] = (int16_t)round(s2 * 32767.0f);
        }

        audioFrame.bufferSize = data_size;
        audioFrame.buffer = (uint8_t *)interleave_buf;
        output_->push(audioFrame);
      }
    }

    wend:
    delete sample.buffer;
  }
  av_frame_free(&decodedFrame);

  DEBUG_PRINT("Stopping decode audio thread...");
}

DecodeAudioThread::DecodeAudioThread() : pImpl_(new DecodeAudioThread::Impl()) {}

DecodeAudioThread::~DecodeAudioThread() = default;

void DecodeAudioThread::Start(const std::string &name) { pImpl_->Start(name); }

void DecodeAudioThread::Join() { pImpl_->Join(); }

void DecodeAudioThread::Shutdown() { pImpl_->Shutdown(); }


//
// DecryptAudioThread
//
/* XXX not used
class DecryptAudioThread::Impl {
public:
  void Start();
  void Join() { thread_.join(); }
  void Shutdown() {
    isRunning_ = false;
    globalTimestamp_ = 0;
  }

private:
  void Run();

  std::thread thread_;
  bool isRunning_;
  std::shared_ptr<BlockingQueue<DEMUXED_SAMPLE>> input_;
  std::shared_ptr<BlockingQueue<AUDIO_FRAME>> output_;
  std::shared_ptr<MP4AudioStream> stream_;
  std::shared_ptr<WidevineAdapter> audioAdapter_;

  uint64_t globalTimestamp_;
};

void DecryptAudioThread::Impl::Start() {
  input_ = wvAdapterLib::GetDemuxedAudioSampleQueue();
  output_ = wvAdapterLib::GetAudioFrameQueue();
  stream_ = wvAdapterLib::GetMp4AudioStream();
  audioAdapter_ = wvAdapterLib::GetAudioAdapter();

  isRunning_ = true;
  globalTimestamp_ = 0;
  thread_ = std::thread(&DecryptAudioThread::Impl::Run, this);
}

// NOTE: decrypt and decode audio
// For decrypt only see widevineDecryptor::WidevineDecryptorThread
void DecryptAudioThread::Impl::Run() {
  INFO_PRINT("Starting DecryptAudio thread ...");

  // TODO implement this when required ...

  INFO_PRINT("DecryptAudio thread finished ...");
}

DecryptAudioThread::DecryptAudioThread() : pImpl_(new DecryptAudioThread::Impl()) {}

DecryptAudioThread::~DecryptAudioThread() = default;

void DecryptAudioThread::Start() { pImpl_->Start(); }

void DecryptAudioThread::Join() { pImpl_->Join(); }

void DecryptAudioThread::Shutdown() { pImpl_->Shutdown(); }
*/

//
// AudioThread
//
class AudioThread::Impl {
public:
  void Start(const std::string &name);
  void Join()  { thread_.join(); };
  void Shutdown() { isRunning_ = false; };

private:
  void Run();

  std::thread thread_;
  bool isRunning_;
  std::string name_;
  std::shared_ptr<BlockingQueue<AUDIO_FRAME>> input_;

  std::shared_ptr<Renderer> audioRenderer_;
  std::shared_ptr<Clock> clock_;
};

void AudioThread::Impl::Start(const std::string &name) {
  input_ = wvAdapterLib::GetAudioFrameQueue();
  audioRenderer_ = wvAdapterLib::GetAudioRenderer();
  clock_ = wvAdapterLib::GetClock();
  name_ = name;

  isRunning_ = true;
  thread_ = std::thread(&AudioThread::Impl::Run, this);
}

void AudioThread::Impl::Run() {
  logging::addThreadIdNameMapping(std::this_thread::get_id(), name_.c_str());

  INFO_PRINT("Starting render audio thread...");

  while (isRunning_) {
#ifdef RASPBERRY_PI
    std::chrono::steady_clock::time_point startTime =
        std::chrono::steady_clock::now();
#endif // RASPBERRY_PI
    clock_->SetStartTime(Clock::MEDIA_TYPE::AUDIO);
    AUDIO_FRAME frame = input_->pop();
    // DEBUG_PRINT(
    //    "Rendering next frame: audio frame queue.size=" << input_->size());

    audioRenderer_->Render(&frame);

#ifndef RASPBERRY_PI
    // DEBUG_PRINT("Syncing with frame duration: " << frame.frameDuration);
    clock_->Sync(frame.frameDuration);
#endif // RASPBERRY_PI
    // clean up to prevent memory leak
    av_free(frame.buffer);

#ifdef RASPBERRY_PI
    std::chrono::steady_clock::time_point currTime =
        std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = currTime - startTime;
    double timeToSleepDbl = frame.frameDuration - diff.count();
    int timeToSleepMicros = timeToSleepDbl * 1000000;
    // INFO_PRINT("Sleeping for " << timeToSleepMicros << " microseconds.");
    std::this_thread::sleep_for(std::chrono::microseconds(timeToSleepMicros));

    clock_->UpdateAudioTime(frame.frameDuration);
#endif // RASPBERRY_PI
  }

  INFO_PRINT("Finishing render audio thread");
}

AudioThread::AudioThread() : pImpl_(new AudioThread::Impl()) {}

AudioThread::~AudioThread() = default;

void AudioThread::Start(const std::string &name) { pImpl_->Start(name); }

void AudioThread::Join() { pImpl_->Join(); }

void AudioThread::Shutdown() { pImpl_->Shutdown(); }
