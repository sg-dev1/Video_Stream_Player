#include ***REMOVED***components.h***REMOVED***

#ifdef RASPBERRY_PI
#include ***REMOVED***alsa_audio_renderer.h***REMOVED***
#include ***REMOVED***mmal_audio_renderer.h***REMOVED***
#include ***REMOVED***mmal_video_renderer.h***REMOVED***
#include ***REMOVED***omx_audio_renderer.h***REMOVED***
#endif

#if defined LIBSDL_RENDERER && !defined RASPBERRY_PI
#include ***REMOVED***sdl_audio_renderer.h***REMOVED***
#include ***REMOVED***sdl_video_renderer.h***REMOVED***
#else
#include ***REMOVED***default_renderer.h***REMOVED***
#endif

namespace wvAdapterLib {
  static const std::shared_ptr<WidevineAdapter> audioAdapter = std::make_shared<WidevineAdapter>();
  static const std::shared_ptr<WidevineAdapter> videoAdapter = std::make_shared<WidevineAdapter>();

  static const std::shared_ptr<MP4VideoStream> mp4VideoStream = std::make_shared<MP4VideoStream>();
  static const std::shared_ptr<BlockingStream> videoNetStream = std::make_shared<BlockingStream>(VIDEO_NET_STREAM_NAME);
  static const auto demuxedVideoSampleQueue =
      std::make_shared<BlockingQueue<std::unique_ptr<DEMUXED_SAMPLE>>>();
  static const auto decryptedVideoSampleQueue =
      std::make_shared<BlockingQueue<DECRYPTED_SAMPLE>>();
  static auto videoFrameQueue =
      std::make_shared<BlockingQueue<VIDEO_FRAME>>();

  static const std::shared_ptr<MP4AudioStream> mp4AudioStream = std::make_shared<MP4AudioStream>();
  static const std::shared_ptr<BlockingStream> audioNetStream = std::make_shared<BlockingStream>(AUDIO_NET_STREAM_NAME);
  static const std::shared_ptr<BlockingQueue<std::unique_ptr<DEMUXED_SAMPLE>>>
      demuxedAudioSampleQueue = std::make_shared<BlockingQueue<std::unique_ptr<DEMUXED_SAMPLE>>>();
  static auto decryptedAudioSampleQueue =
      std::make_shared<BlockingQueue<DECRYPTED_SAMPLE>>();
  static auto audioFrameQueue =
      std::make_shared<BlockingQueue<AUDIO_FRAME>>();

  static auto clock = std::make_shared<Clock>();

  static auto queueCounter = std::make_shared<QueueCounter>();
  static auto timeLogger = std::make_shared<TimeLogger>();

#if defined RASPBERRY_PI
  static auto videoRenderer = std::shared_ptr<Renderer>(new MmalVideoRenderer());
  // static auto audioRenderer = std::shared_ptr<Renderer>(new MmalAudioRenderer());
  static auto audioRenderer = std::shared_ptr<Renderer>(new AlsaAudioRenderer());
  // static auto audioRenderer = std::shared_ptr<Renderer>(new OMXAudioRenderer());
#elif defined LIBSDL_RENDERER && !defined RASPBERRY_PI
  static auto videoRenderer = std::shared_ptr<Renderer>(new SDLVideoRenderer());
  static auto audioRenderer = std::shared_ptr<Renderer>(new SDLAudioRenderer());
#else
  // default Render implementation
  static auto videoRenderer = std::shared_ptr<Renderer>(new DefaultRenderer());
  static auto audioRenderer = std::shared_ptr<Renderer>(new DefaultRenderer());
#endif

  //
  // Accessor Impl
  //

  const std::shared_ptr<WidevineAdapter>& GetVideoAdapter() noexcept {
    return audioAdapter;
  }
  const std::shared_ptr<WidevineAdapter>& GetAudioAdapter() noexcept {
    return videoAdapter;
  }

  const std::shared_ptr<MP4VideoStream>& GetMp4VideoStream() noexcept {
    return mp4VideoStream;
  }
  const std::shared_ptr<BlockingStream>& GetVideoNetStream() noexcept {
    return videoNetStream;
  }
  const std::shared_ptr<BlockingQueue<std::unique_ptr<DEMUXED_SAMPLE>>>&
      GetDemuxedVideoSampleQueue() noexcept {
    return demuxedVideoSampleQueue;
  }
  const std::shared_ptr<BlockingQueue<DECRYPTED_SAMPLE>>&
      GetDecryptedVideoSampleQueue() noexcept {
    return decryptedVideoSampleQueue;
  }
  const std::shared_ptr<BlockingQueue<VIDEO_FRAME>>&
      GetVideoFrameQueue() noexcept {
    return videoFrameQueue;
  }


  //
  // AUDIO
  //
  const std::shared_ptr<MP4AudioStream>& GetMp4AudioStream() noexcept {
    return mp4AudioStream;
  }
  const std::shared_ptr<BlockingStream>& GetAudioNetStream() noexcept {
    return audioNetStream;
  }

  const std::shared_ptr<BlockingQueue<std::unique_ptr<DEMUXED_SAMPLE>>>& GetDemuxedAudioSampleQueue() noexcept {
    return demuxedAudioSampleQueue;
  }
  const std::shared_ptr<BlockingQueue<DECRYPTED_SAMPLE>>&
      GetDecryptedAudioSampleQueue() noexcept {
    return decryptedAudioSampleQueue;
  }
  const std::shared_ptr<BlockingQueue<AUDIO_FRAME>>&
      GetAudioFrameQueue() noexcept {
    return audioFrameQueue;
  }

  //
  // MISC
  //
  const std::shared_ptr<Renderer>& GetVideoRenderer() noexcept {
    return videoRenderer;
  }
  const std::shared_ptr<Renderer>& GetAudioRenderer() noexcept {
    return audioRenderer;
  }

  const std::shared_ptr<Clock>& GetClock() noexcept {
    return clock;
  }

  const std::shared_ptr<QueueCounter>& GetQueueCounter() noexcept {
    return queueCounter;
  }
  const std::shared_ptr<TimeLogger>& GetTimeLogger() noexcept {
    return timeLogger;
  }
}

