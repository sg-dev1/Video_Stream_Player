#include "default_renderer.h"
#include "audio_renderer.h"
#include "logging.h"
#include "util.h"
#include "video_renderer.h"

#include <chrono>

bool DefaultRenderer::Open(void *cfg) { return true; }

void DefaultRenderer::Close() {}

bool DefaultRenderer::Reconfigure(void *cfg) { return true; }

void DefaultRenderer::Render(FRAME *frame) {
  DEBUG_PRINT("DefaultRenderer::Render called");
  std::chrono::milliseconds ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch());
  std::string filename;

  // https://stackoverflow.com/a/4589292
  AUDIO_FRAME *audioFrame = dynamic_cast<AUDIO_FRAME *>(frame);
  VIDEO_FRAME *videoFrame = dynamic_cast<VIDEO_FRAME *>(frame);
  if (audioFrame) {
    DEBUG_PRINT("Saving new audio frame.");
    filename = std::to_string(ms.count()) + "_" +
               GetAudioFrameString(*audioFrame) + ".pcm";
  } else if (videoFrame) {
    DEBUG_PRINT("Saving new video frame.");
    filename = std::to_string(ms.count()) + "_" +
               GetVideoFrameString(*videoFrame) + ".yuv";
  } else {
    ERROR_PRINT("FRAME of not supported type given.");
    return;
  }

  DumpToFile(filename, frame->buffer, frame->bufferSize);
}
