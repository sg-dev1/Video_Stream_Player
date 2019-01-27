#ifndef WV_ADAPTER_LIB_QUEUE_COUNTER_H
#define WV_ADAPTER_LIB_QUEUE_COUNTER_H

#include <string>
#include <sstream>
#include <memory>
#include <iomanip>

#include ***REMOVED***logging.h***REMOVED***

struct QueueCounterSummary{
  uint32_t counter;
  float percentFilled;
  uint64_t memoryUsage;

  std::string toString() {
    std::stringstream ss;
    ss << ***REMOVED***counter = ***REMOVED*** <<  counter << ***REMOVED***, filled = ***REMOVED*** << std::setprecision(3) <<
         percentFilled * 100 << ***REMOVED*** %, ***REMOVED*** << ***REMOVED*** memoryUsage = ***REMOVED*** << memoryUsage / 1000000 << ***REMOVED*** MB (***REMOVED*** << memoryUsage << ***REMOVED*** bytes).***REMOVED***;
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
