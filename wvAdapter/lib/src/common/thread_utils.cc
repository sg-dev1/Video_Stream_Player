#include "thread_utils.h"
#include "logging.h"

void ConditionalWaitHelper::Wait() {
  std::unique_lock<std::mutex> lock(lock_);
  currentValue_++;
  cond_.wait(lock, [=] { return currentValue_ < upperBound_; });
}

void ConditionalWaitHelper::Signal() {
  {
    std::unique_lock<std::mutex> lock(lock_);
    currentValue_--;
  }
  cond_.notify_one();
}
