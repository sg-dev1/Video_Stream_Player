#ifndef WV_ADAPTER_LIB_COMPONENTS_H
#define WV_ADAPTER_LIB_COMPONENTS_H

#include <memory>

#include ***REMOVED***common.h***REMOVED***
#include ***REMOVED***audio_renderer.h***REMOVED***
#include ***REMOVED***video_renderer.h***REMOVED***
#include ***REMOVED***blocking_stream.h***REMOVED***
#include ***REMOVED***mp4_stream.h***REMOVED***
#include ***REMOVED***widevineAdapter.h***REMOVED***
#include ***REMOVED***blocking_queue.h***REMOVED***
#include ***REMOVED***queue_counter.h***REMOVED***
#include ***REMOVED***time_logger.h***REMOVED***

namespace wvAdapterLib {
  const WV_ADAPTER_TEST_API std::shared_ptr<WidevineAdapter>& GetVideoAdapter() noexcept;
  const WV_ADAPTER_TEST_API std::shared_ptr<WidevineAdapter>& GetAudioAdapter() noexcept;

  const WV_ADAPTER_TEST_API std::shared_ptr<MP4VideoStream>& GetMp4VideoStream() noexcept;
  const WV_ADAPTER_TEST_API std::shared_ptr<BlockingStream>& GetVideoNetStream() noexcept;
  const std::shared_ptr<BlockingQueue<std::unique_ptr<DEMUXED_SAMPLE>>>&
      GetDemuxedVideoSampleQueue() noexcept;
  // NOTE this is currently not used
  // It would only be used if WidevineDecryptorThread is used in the Video pipeline
  // which is currently not the case
  const WV_ADAPTER_TEST_API std::shared_ptr<BlockingQueue<DECRYPTED_SAMPLE>>&
      GetDecryptedVideoSampleQueue() noexcept;
  const WV_ADAPTER_TEST_API std::shared_ptr<BlockingQueue<VIDEO_FRAME>>&
      GetVideoFrameQueue() noexcept;

  const WV_ADAPTER_TEST_API std::shared_ptr<MP4AudioStream>& GetMp4AudioStream() noexcept;
  const WV_ADAPTER_TEST_API std::shared_ptr<BlockingStream>& GetAudioNetStream() noexcept;
  const WV_ADAPTER_TEST_API std::shared_ptr<BlockingQueue<std::unique_ptr<DEMUXED_SAMPLE>>>&
    GetDemuxedAudioSampleQueue() noexcept;
  const WV_ADAPTER_TEST_API std::shared_ptr<BlockingQueue<DECRYPTED_SAMPLE>>&
    GetDecryptedAudioSampleQueue() noexcept;
  const WV_ADAPTER_TEST_API std::shared_ptr<BlockingQueue<AUDIO_FRAME>>&
    GetAudioFrameQueue() noexcept;

  const WV_ADAPTER_TEST_API std::shared_ptr<Renderer>& GetVideoRenderer() noexcept;
  const WV_ADAPTER_TEST_API std::shared_ptr<Renderer>& GetAudioRenderer() noexcept;

  const WV_ADAPTER_TEST_API std::shared_ptr<Clock>& GetClock() noexcept;

  const WV_ADAPTER_TEST_API std::shared_ptr<QueueCounter>& GetQueueCounter() noexcept;
  const WV_ADAPTER_TEST_API std::shared_ptr<TimeLogger>& GetTimeLogger() noexcept;
}



#endif //WV_ADAPTER_LIB_COMPONENTS_H
