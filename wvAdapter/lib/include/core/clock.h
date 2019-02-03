#ifndef CLOCK_H_
#define CLOCK_H_

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <unordered_map>

#include "common.h"

typedef std::unordered_map<std::thread::id,
                           std::chrono::steady_clock::time_point>
    ThreadIdToTimeMap_t;

class WV_ADAPTER_TEST_API Clock {
public:
  enum MEDIA_TYPE { AUDIO = 0, VIDEO };

  void SetStartTime(MEDIA_TYPE mediaType);
  /* frameDuration: how long a frame should be displayed (in sec) */
  void Sync(double frameDuration);

  // let the audio thread itself mange its time (not the clock)
  void UpdateAudioTime(double frameDuration);

  void Shutdown() {
    isShutdown_ = true;
    waitTillStarted_.notify_all();
    syncCond_.notify_all();
  }

private:
  int startedThreads_ = 0;

  std::mutex lock_;
  std::condition_variable waitTillStarted_;
  int startedThreadsToWait_ = 2;
  std::condition_variable syncCond_;

  ThreadIdToTimeMap_t threadIdStartTimeMapping_;
  std::unordered_map<std::thread::id, MEDIA_TYPE> threadIdMediaTypeMapping_;
  // times in second for audio and video starting at zero
  double audioTime_ = 0.0;
  double videoTime_ = 0.0;

  bool isShutdown_ = false;
};

#endif
