#include "audio_renderer.h"

std::string GetAudioFrameString(const AUDIO_FRAME &frame) {
  return "channels=" + std::to_string(frame.channels) +
         "_sampleSize=" + std::to_string(frame.sampleSize) +
         "_samplingRate=" + std::to_string(frame.samplingRate) +
         "_timestamp=" + std::to_string(frame.timestamp) +
         "_frameDuration=" + std::to_string(frame.frameDuration);
}
