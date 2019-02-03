#ifndef AUDIO_RENDERER_
#define AUDIO_RENDERER_

#include <cstdint>
#include <string>

#include "renderer.h"

struct AUDIO_FRAME : public FRAME {
  uint32_t channels;
  // bits per sample, e.g. 16
  uint32_t sampleSize;
  uint64_t samplingRate;
};

struct AudioRendererConfig {
  uint32_t channels;
  uint64_t samplingRate;
  uint32_t sampleSize;
};

std::string GetAudioFrameString(const AUDIO_FRAME &frame);

#endif // AUDIO_RENDERER_
