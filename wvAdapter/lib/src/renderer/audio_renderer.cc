#include ***REMOVED***audio_renderer.h***REMOVED***

std::string GetAudioFrameString(const AUDIO_FRAME &frame) {
  return ***REMOVED***channels=***REMOVED*** + std::to_string(frame.channels) +
         ***REMOVED***_sampleSize=***REMOVED*** + std::to_string(frame.sampleSize) +
         ***REMOVED***_samplingRate=***REMOVED*** + std::to_string(frame.samplingRate) +
         ***REMOVED***_timestamp=***REMOVED*** + std::to_string(frame.timestamp) +
         ***REMOVED***_frameDuration=***REMOVED*** + std::to_string(frame.frameDuration);
}
