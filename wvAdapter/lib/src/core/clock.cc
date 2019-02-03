#include "clock.h"
#include "assertion.h"
#include "logging.h"

#define MAX_DELTA 0.5

void Clock::SetStartTime(Clock::MEDIA_TYPE mediaType) {
  std::unique_lock<std::mutex> lock(lock_);

  ThreadIdToTimeMap_t::const_iterator got =
      threadIdStartTimeMapping_.find(std::this_thread::get_id());
  if (got == threadIdStartTimeMapping_.end()) {
    startedThreads_++;
    threadIdMediaTypeMapping_[std::this_thread::get_id()] = mediaType;
    if (mediaType == Clock::MEDIA_TYPE::AUDIO) {
      INFO_PRINT("Starting audio clock ...");
    } else if (mediaType == Clock::MEDIA_TYPE::VIDEO) {
      INFO_PRINT("Starting video clock ...");
    }

#if !defined(DISABLE_CLOCK_SYNC) && !defined(DISABLE_AUDIO) &&                 \
    !defined(DISABLE_VIDEO)
    waitTillStarted_.wait(lock, [=] {
      return startedThreads_ == startedThreadsToWait_ || isShutdown_;
    });
    waitTillStarted_.notify_one();
    if (mediaType == Clock::MEDIA_TYPE::AUDIO) {
      INFO_PRINT("Audio clock started!");
    } else if (mediaType == Clock::MEDIA_TYPE::VIDEO) {
      INFO_PRINT("Video clock started!");
    }
#endif
  }

  threadIdStartTimeMapping_[std::this_thread::get_id()] =
      std::chrono::steady_clock::now();
}

void Clock::Sync(double frameDuration) {

  int timeToSleepMicros = 0;
  {
    std::unique_lock<std::mutex> lock(lock_);

    std::chrono::steady_clock::time_point startTime =
        threadIdStartTimeMapping_[std::this_thread::get_id()];
    Clock::MEDIA_TYPE mediaType =
        threadIdMediaTypeMapping_[std::this_thread::get_id()];

#if !defined(DISABLE_CLOCK_SYNC) && !defined(DISABLE_AUDIO) &&                 \
    !defined(DISABLE_VIDEO)
    // sync audio and video
    syncCond_.wait(lock, [=] {
      double delta;
      if (mediaType == Clock::MEDIA_TYPE::AUDIO) {
        delta = audioTime_ - videoTime_;
      } else if (mediaType == Clock::MEDIA_TYPE::VIDEO) {
        delta = videoTime_ - audioTime_;
      } else {
        ASSERT(false);
      }
      return delta < MAX_DELTA || isShutdown_;
    });
#endif

    std::chrono::steady_clock::time_point currTime =
        std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = currTime - startTime;
    double timeToSleepDbl = frameDuration - diff.count();
    timeToSleepMicros = timeToSleepDbl * 1000000;

    if (mediaType == Clock::MEDIA_TYPE::AUDIO) {
      audioTime_ += frameDuration;
      DEBUG_PRINT("New audioTime: " << audioTime_);
    } else if (mediaType == Clock::MEDIA_TYPE::VIDEO) {
      videoTime_ += frameDuration;
      DEBUG_PRINT("New videoTime: " << videoTime_);
    } else {
      ASSERT(false);
    }
  }
  syncCond_.notify_one();
  if (timeToSleepMicros > 0) {
    DEBUG_PRINT("Sleeping for " << timeToSleepMicros << " microseconds.");
    std::this_thread::sleep_for(std::chrono::microseconds(timeToSleepMicros));
  }
}

void Clock::UpdateAudioTime(double frameDuration) {
  {
    std::unique_lock<std::mutex> lock(lock_);

    audioTime_ += frameDuration;
    syncCond_.notify_one();
    DEBUG_PRINT("New audioTime: " << audioTime_);
  }
}
