#include "gtest/gtest.h"

#include "alsa_audio_renderer.h"
#include "audio_renderer.h"
#include "logging.h"
#include "util.h"

#define AUDIO_TEST_DATA_PATH "data/renderer/priv/audio/"

// TODO merge with mmal_renderer_test::MmalAudioRendererTest
//  (duplicated code)
class AlsaAudioRendererTest : public ::testing::Test {
protected:
  AlsaAudioRendererTest() {
    audioDir_ = __DIR__ + std::string(AUDIO_TEST_DATA_PATH);
  }

  AlsaAudioRenderer renderer_;
  std::string audioDir_;
};

TEST_F(AlsaAudioRendererTest, Render1) {
  std::vector<std::string> audioFiles;
  bool ret = GetFilesInDir(audioDir_, audioFiles);
  ASSERT_TRUE(ret) << "Could not load audio files. Seems that audioDir " << audioDir_ << " does not exist.";
  ASSERT_GT(audioFiles.size(), 0) << "No audio test files to render found. It "
                                     "seems that you have first to generated "
                                     "some.";

  std::sort(audioFiles.begin(), audioFiles.end());

  uint32_t channels, sampleSize;
  uint64_t samplingRate;
  double frameDuration;
  ret = ParseAudioFilename(audioFiles[0], channels, samplingRate, sampleSize,
                           frameDuration);
  ASSERT_TRUE(ret);

  AudioRendererConfig cfg;
  cfg.channels = channels;
  cfg.sampleSize = sampleSize;
  cfg.samplingRate = samplingRate;

  // rendering
  ret = renderer_.Open(&cfg);
  ASSERT_TRUE(ret);
  for (int i = 0; i < audioFiles.size(); i++) {
    std::chrono::steady_clock::time_point startTime =
        std::chrono::steady_clock::now();
    std::string currFilePath = audioDir_ + audioFiles[i];
    // DEBUG_PRINT("Rendering " << currFilePath);

    std::vector<uint8_t> data;
    int r = ReadFileData(currFilePath, data);
    ASSERT_GT(r, 0);

    // DEBUG_PRINT("# " << r << " bytes read from " << currFilePath);

    AUDIO_FRAME frame;
    frame.channels = cfg.channels;
    frame.sampleSize = cfg.sampleSize;
    frame.samplingRate = cfg.samplingRate;
    frame.buffer = data.data();
    frame.bufferSize = r; // data.size();
    frame.frameDuration = frameDuration;

    // DEBUG_PRINT("Going to render frame with " << frame.bufferSize << "
    // bytes.");

    renderer_.Render(&frame);

    std::chrono::steady_clock::time_point currTime =
        std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = currTime - startTime;
    double timeToSleepDbl = frameDuration - diff.count();
    int timeToSleepMicros = timeToSleepDbl * 1000000;
    // INFO_PRINT("Sleeping for " << timeToSleepMicros << " microseconds.");
    std::this_thread::sleep_for(std::chrono::microseconds(timeToSleepMicros));
  }
  renderer_.Close();
}
