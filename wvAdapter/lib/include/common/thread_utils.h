#ifndef THREAD_UTILS_H_
#define THREAD_UTILS_H_

#include <condition_variable>
#include <mutex>
#include <stdint.h> // uint32_t

class ConditionalWaitHelper {
public:
  void Wait();
  void Signal();

  void SetUpperBound(uint32_t upperBound) { upperBound_ = upperBound; }

private:
  uint32_t upperBound_;
  uint32_t currentValue_ = 0;

  std::mutex lock_;
  std::condition_variable cond_;
};

#endif // THREAD_UTILS_H_
