#include "gtest/gtest.h"
#include <chrono>
#include <string>
#include <thread>

#include <interface/mmal/mmal_logging.h>
#include <interface/vcos/vcos_logging.h>

#include "logging.h"
#include "util.h"

// comment this to disable audio and video rendering in separate threads
// for the MmalAudioVideoRenderingTest test case
#define _MMAL_TEST_RENDER_IN_OWN_THREAD

#include "mmal_audio_renderer.h"
#define AUDIO_TEST_DATA_PATH "data/renderer/priv/audio/"

class MmalAudioRendererTest : public ::testing::Test {
protected:
  MmalAudioRendererTest() {
    audioDir_ = __DIR__ + std::string(AUDIO_TEST_DATA_PATH);
  }

  MmalAudioRenderer renderer_;
  std::string audioDir_;
};

TEST_F(MmalAudioRendererTest, DISABLED_Render1) {
  std::vector<std::string> audioFiles;
  bool ret = GetFilesInDir(audioDir_, audioFiles);
  ASSERT_TRUE(ret);
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

//////////////////////////////////////////////////////

#include "mmal_video_renderer.h"
#define VIDEO_TEST_DATA_PATH "data/renderer/priv/video/"

class MmalVideoRendererTest : public ::testing::Test {
protected:
  MmalVideoRendererTest() {
    videoDir_ = __DIR__ + std::string(VIDEO_TEST_DATA_PATH);
  }

  MmalVideoRenderer renderer_;
  std::string videoDir_;
};

TEST_F(MmalVideoRendererTest, DISABLED_Render1) {
  std::vector<std::string> videoFiles;
  bool ret = GetFilesInDir(videoDir_, videoFiles);
  ASSERT_TRUE(ret);
  ASSERT_GT(videoFiles.size(), 0) << "No video test files to render found. It "
                                     "seems that you have first to generated "
                                     "some.";

  std::sort(videoFiles.begin(), videoFiles.end());

  std::string format;
  uint32_t width, height, y_plane_offset, u_plane_offset, v_plane_offset,
      y_plane_stride, u_plane_stride, v_plane_stride;
  double frameDuration;
  ret = ParseVideoFilename(videoFiles[0], format, width, height, y_plane_offset,
                           u_plane_offset, v_plane_offset, y_plane_stride,
                           u_plane_stride, v_plane_stride, frameDuration);
  ASSERT_TRUE(ret);

  VIDEO_FORMAT_ENUM encoding;
  if (format == "I420") {
    encoding = VIDEO_FORMAT_ENUM::kI420;
  } else { // format == "Yv12"
    encoding = VIDEO_FORMAT_ENUM::kYv12;
  }

  VideoRendererConfig videoCfg;
  videoCfg.width = width;
  videoCfg.height = height;
  videoCfg.encoding = encoding;
  ret = renderer_.Open(&videoCfg);
  ASSERT_TRUE(ret);

  for (int i = 0; i < videoFiles.size(); i++) {
    std::chrono::steady_clock::time_point startTime =
        std::chrono::steady_clock::now();
    std::string currFilePath = videoDir_ + videoFiles[i];
    // DEBUG_PRINT("Rendering " << currFilePath);

    std::vector<uint8_t> data;
    int r = ReadFileData(currFilePath, data);
    ASSERT_GT(r, 0);

    // DEBUG_PRINT("# " << r << " bytes read from " << currFilePath);

    VIDEO_FRAME frame;

    frame.encoding = encoding;
    frame.width = width;
    frame.height = height;
    frame.y_plane_offset = y_plane_offset;
    frame.u_plane_offset = u_plane_offset;
    frame.v_plane_offset = v_plane_offset;
    frame.y_plane_stride = y_plane_stride;
    frame.u_plane_stride = u_plane_stride;
    frame.v_plane_stride = v_plane_stride;
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

//////////////////////////////////////////////////////

#include "clock.h"

class MmalAudioVideoRenderingTest : public ::testing::Test {
protected:
  MmalAudioVideoRenderingTest() {
    audioDir_ = __DIR__ + std::string(AUDIO_TEST_DATA_PATH);
    videoDir_ = __DIR__ + std::string(VIDEO_TEST_DATA_PATH);
  }

  MmalAudioRenderer audioRenderer_;
  MmalVideoRenderer videoRenderer_;
  std::string audioDir_;
  std::string videoDir_;
  Clock clock_;
};

// Test thread for audio rendering
class AudioTestRenderThread {
public:
  void Start(std::vector<std::string> &audioFiles, std::string &audioDir,
             Renderer *audioRenderer, Clock *clock, AUDIO_FRAME *dummyFrame) {
    audioFiles_ = audioFiles;
    audioRenderer_ = audioRenderer;
    audioDir_ = audioDir;
    clock_ = clock;
    frame_ = dummyFrame;

    thread_ = std::thread(&AudioTestRenderThread::Run, this);
  }

  void join() { thread_.join(); }

private:
  void Run() {
    DEBUG_PRINT("Starting test render audio thread...");

    for (int i = 0; i < audioFiles_.size(); i++) {
      clock_->SetStartTime(Clock::MEDIA_TYPE::AUDIO);

      std::string currFilePath = audioDir_ + audioFiles_[i];
      // DEBUG_PRINT("Rendering " << currFilePath);

      std::vector<uint8_t> data;
      int r = ReadFileData(currFilePath, data);
      ASSERT_GT(r, 0);

      frame_->buffer = data.data();
      frame_->bufferSize = r;

      audioRenderer_->Render(frame_);

      clock_->Sync(frame_->frameDuration);

      data.clear();
    }

    DEBUG_PRINT("Finishing test render audio thread");
  }

  std::thread thread_;
  std::vector<std::string> audioFiles_;
  std::string audioDir_;

  Renderer *audioRenderer_;
  Clock *clock_;
  AUDIO_FRAME *frame_;
};

// Test thread for video rendering
class VideoTestRenderThread {
public:
  void Start(std::vector<std::string> &videoFiles, std::string &videoDir,
             Renderer *videoRenderer, Clock *clock, VIDEO_FRAME *dummyFrame) {
    videoFiles_ = videoFiles;
    videoDir_ = videoDir;
    videoRenderer_ = videoRenderer;
    clock_ = clock;
    frame_ = dummyFrame;

    thread_ = std::thread(&VideoTestRenderThread::Run, this);
  }
  void join() { thread_.join(); }

private:
  void Run() {
    DEBUG_PRINT("Starting test render video thread...");

    for (int i = 0; i < videoFiles_.size(); i++) {
      clock_->SetStartTime(Clock::MEDIA_TYPE::VIDEO);

      std::string currFilePath = videoDir_ + videoFiles_[i];
      // DEBUG_PRINT("Rendering " << currFilePath);

      std::vector<uint8_t> data;
      int r = ReadFileData(currFilePath, data);
      ASSERT_GT(r, 0);

      frame_->buffer = data.data();
      frame_->bufferSize = r;

      videoRenderer_->Render(frame_);

      clock_->Sync(frame_->frameDuration);

      data.clear();
    }

    DEBUG_PRINT("Finishing test render video thread");
  }

  std::thread thread_;
  std::vector<std::string> videoFiles_;
  std::string videoDir_;

  VIDEO_FRAME *frame_;
  Renderer *videoRenderer_;
  Clock *clock_;
};

TEST_F(MmalAudioVideoRenderingTest, Render1) {
  vcos_log_set_level(VCOS_LOG_CATEGORY, VCOS_LOG_TRACE);

  // Load audio and video files
  std::vector<std::string> audioFiles;
  bool ret = GetFilesInDir(audioDir_, audioFiles);
  ASSERT_TRUE(ret);
  ASSERT_GT(audioFiles.size(), 0) << "No audio test files to render found. It "
                                     "seems that you have first to generated "
                                     "some.";

  std::vector<std::string> videoFiles;
  ret = GetFilesInDir(videoDir_, videoFiles);
  ASSERT_TRUE(ret);
  ASSERT_GT(videoFiles.size(), 0) << "No video test files to render found. It "
                                     "seems that you have first to generated "
                                     "some.";

  std::sort(audioFiles.begin(), audioFiles.end());
  std::sort(videoFiles.begin(), videoFiles.end());

  // Parse audio and video files
  uint32_t channels, sampleSize;
  uint64_t samplingRate;
  double audioFrameDuration;
  ret = ParseAudioFilename(audioFiles[0], channels, samplingRate, sampleSize,
                           audioFrameDuration);
  ASSERT_TRUE(ret);

  AudioRendererConfig cfg;
  cfg.channels = channels;
  cfg.sampleSize = sampleSize;
  cfg.samplingRate = samplingRate;

  ret = audioRenderer_.Open(&cfg);
  ASSERT_TRUE(ret);

  std::string format;
  uint32_t width, height, y_plane_offset, u_plane_offset, v_plane_offset,
      y_plane_stride, u_plane_stride, v_plane_stride;
  double videoFrameDuration;
  ret = ParseVideoFilename(videoFiles[0], format, width, height, y_plane_offset,
                           u_plane_offset, v_plane_offset, y_plane_stride,
                           u_plane_stride, v_plane_stride, videoFrameDuration);
  ASSERT_TRUE(ret);

  VIDEO_FORMAT_ENUM encoding;
  if (format == "I420") {
    encoding = VIDEO_FORMAT_ENUM::kI420;
  } else { // format == "Yv12"
    encoding = VIDEO_FORMAT_ENUM::kYv12;
  }

  VideoRendererConfig videoCfg;
  videoCfg.width = width;
  videoCfg.height = height;
  videoCfg.encoding = encoding;
  ret = videoRenderer_.Open(&videoCfg);
  ASSERT_TRUE(ret);

  // rendering
  AUDIO_FRAME audioDummyFrame;
  audioDummyFrame.channels = channels;
  audioDummyFrame.sampleSize = sampleSize;
  audioDummyFrame.samplingRate = samplingRate;
  audioDummyFrame.frameDuration = audioFrameDuration;

  VIDEO_FRAME videoDummyFrame;
  videoDummyFrame.encoding = encoding;
  videoDummyFrame.width = width;
  videoDummyFrame.height = height;
  videoDummyFrame.y_plane_offset = y_plane_offset;
  videoDummyFrame.u_plane_offset = u_plane_offset;
  videoDummyFrame.v_plane_offset = v_plane_offset;
  videoDummyFrame.y_plane_stride = y_plane_stride;
  videoDummyFrame.u_plane_stride = u_plane_stride;
  videoDummyFrame.v_plane_stride = v_plane_stride;
  videoDummyFrame.frameDuration = videoFrameDuration;

  // Different strategies:
  //   A) both render in own thread
  //   B) rendering in same thread

#ifdef _MMAL_TEST_RENDER_IN_OWN_THREAD
  // A
  AudioTestRenderThread audioThread;
  VideoTestRenderThread videoThread;

  audioThread.Start(audioFiles, audioDir_, &audioRenderer_, &clock_,
                    &audioDummyFrame);
  videoThread.Start(videoFiles, videoDir_, &videoRenderer_, &clock_,
                    &videoDummyFrame);

  audioThread.join();
  videoThread.join();
#else
  // B
  int numIterations = audioFiles.size() > videoFiles.size() ? audioFiles.size()
                                                            : videoFiles.size();
  for (int i = 0; i < numIterations; i++) {
    std::vector<uint8_t> audioData;
    if (i < audioFiles.size()) {
      std::string currAudioFilePath = audioDir_ + audioFiles[i];
      int r = ReadFileData(currAudioFilePath, audioData);
      ASSERT_GT(r, 0);
      audioDummyFrame.buffer = audioData.data();
      audioDummyFrame.bufferSize = r;
    }

    std::vector<uint8_t> videoData;
    if (i < videoFiles.size()) {
      std::string currVideoFilePath = videoDir_ + videoFiles[i];
      int r = ReadFileData(currVideoFilePath, videoData);
      ASSERT_GT(r, 0);
      videoDummyFrame.buffer = videoData.data();
      videoDummyFrame.bufferSize = r;
    }

    std::chrono::steady_clock::time_point startTime =
        std::chrono::steady_clock::now();
    if (i < audioFiles.size()) {
      audioRenderer_.Render(&audioDummyFrame);
    }
    if (i < videoFiles.size()) {
      videoRenderer_.Render(&videoDummyFrame);
    }

    double frameDuration = audioFrameDuration < videoFrameDuration
                               ? audioFrameDuration
                               : videoFrameDuration;
    std::chrono::steady_clock::time_point currTime =
        std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = currTime - startTime;
    double timeToSleepDbl = frameDuration - diff.count();
    int timeToSleepMicros = timeToSleepDbl * 1000000;
    INFO_PRINT("Sleeping for " << timeToSleepMicros << " microseconds.");
    std::this_thread::sleep_for(std::chrono::microseconds(timeToSleepMicros));
  }
#endif

  audioRenderer_.Close();
  videoRenderer_.Close();
}
