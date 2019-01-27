#include ***REMOVED***default_renderer.h***REMOVED***
#include ***REMOVED***audio_renderer.h***REMOVED***
#include ***REMOVED***logging.h***REMOVED***
#include ***REMOVED***util.h***REMOVED***
#include ***REMOVED***video_renderer.h***REMOVED***

#include <chrono>

bool DefaultRenderer::Open(void *cfg) { return true; }

void DefaultRenderer::Close() {}

bool DefaultRenderer::Reconfigure(void *cfg) { return true; }

void DefaultRenderer::Render(FRAME *frame) {
  DEBUG_PRINT(***REMOVED***DefaultRenderer::Render called***REMOVED***);
  std::chrono::milliseconds ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch());
  std::string filename;

  // https://stackoverflow.com/a/4589292
  AUDIO_FRAME *audioFrame = dynamic_cast<AUDIO_FRAME *>(frame);
  VIDEO_FRAME *videoFrame = dynamic_cast<VIDEO_FRAME *>(frame);
  if (audioFrame) {
    DEBUG_PRINT(***REMOVED***Saving new audio frame.***REMOVED***);
    filename = std::to_string(ms.count()) + ***REMOVED***_***REMOVED*** +
               GetAudioFrameString(*audioFrame) + ***REMOVED***.pcm***REMOVED***;
  } else if (videoFrame) {
    DEBUG_PRINT(***REMOVED***Saving new video frame.***REMOVED***);
    filename = std::to_string(ms.count()) + ***REMOVED***_***REMOVED*** +
               GetVideoFrameString(*videoFrame) + ***REMOVED***.yuv***REMOVED***;
  } else {
    ERROR_PRINT(***REMOVED***FRAME of not supported type given.***REMOVED***);
    return;
  }

  DumpToFile(filename, frame->buffer, frame->bufferSize);
}
