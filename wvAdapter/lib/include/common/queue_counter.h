#ifndef WV_ADAPTER_LIB_QUEUE_COUNTER_H
#define WV_ADAPTER_LIB_QUEUE_COUNTER_H

#include <string>
#include <sstream>
#include <memory>
#include <iomanip>

#include "logging.h"

struct QueueCounterSummary{
  uint32_t counter;
  float percentFilled;
  uint64_t memoryUsage;

  std::string toString() {
    std::stringstream ss;
    ss << "counter = " <<  counter << ", filled = " << std::setprecision(3) <<
         percentFilled * 100 << " %, " << " memoryUsage = " << memoryUsage / 1000000 << " MB (" << memoryUsage << " bytes).";
    return ss.str();
  }
};

class QueueCounter {
public:
  QueueCounter();
  ~QueueCounter();

  void RegisterQueue(const std::string &name, uint32_t upperBound);
  void IncQueue(const std::string &name, uint32_t memoryUsage);
  void DecQueue(const std::string &name, uint32_t memoryUsage);

  void GetSummary(const std::string &name, QueueCounterSummary &summary);

private:
  class Impl;
  std::unique_ptr<Impl> pImpl_;
};

#endif //WV_ADAPTER_LIB_QUEUE_COUNTER_H
