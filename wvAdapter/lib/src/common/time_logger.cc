#include ***REMOVED***time_logger.h***REMOVED***

#include ***REMOVED***logging.h***REMOVED***
#include ***REMOVED***constants.h***REMOVED***

#include <array>
#include <unordered_map>
#include <iomanip>

using dblArrSizeType_t = std::array<double, TIME_LOGGER_RING_BUFFER_SIZE>::size_type;

struct TimeLogEntry {
  TimeLogEntry(const std::chrono::steady_clock::time_point &point) :
      startTime(point), nextIndex(0), elementsInBuffer(0) {}

  std::chrono::steady_clock::time_point startTime;
  std::array<double, TIME_LOGGER_RING_BUFFER_SIZE> ringBuffer;
  dblArrSizeType_t nextIndex;
  dblArrSizeType_t elementsInBuffer;
};

class TimeLogger::Impl {
public:
  Impl() : isEnabled_(false) {}

  void Enable(bool enable) { isEnabled_ = enable; }

  void SetStart();
  void SetEnd();

  void PrintSummary();

private:
  bool isEnabled_;

  std::unordered_map<std::thread::id, std::unique_ptr<TimeLogEntry>> threadIdTimeLogEntryMap_;
};

void TimeLogger::Impl::SetStart() {
  if (!isEnabled_) {
    return;
  }
  auto iter = threadIdTimeLogEntryMap_.find(std::this_thread::get_id());
  if (iter != threadIdTimeLogEntryMap_.end()) {
    iter->second->startTime = std::chrono::steady_clock::now();
  } else {
    threadIdTimeLogEntryMap_.emplace(std::this_thread::get_id(),
                                     std::unique_ptr<TimeLogEntry>(new TimeLogEntry(std::chrono::steady_clock::now())));
  }
}

void TimeLogger::Impl::SetEnd() {
  if (!isEnabled_) {
    return;
  }

  auto iter = threadIdTimeLogEntryMap_.find(std::this_thread::get_id());
  if (iter != threadIdTimeLogEntryMap_.end()) {
    std::chrono::steady_clock::time_point currTime =
        std::chrono::steady_clock::now();
    std::chrono::duration<double> diff = currTime - iter->second->startTime;
    iter->second->ringBuffer[iter->second->nextIndex] = diff.count();

    iter->second->nextIndex = (iter->second->nextIndex + 1) % TIME_LOGGER_RING_BUFFER_SIZE;
    if (iter->second->elementsInBuffer < TIME_LOGGER_RING_BUFFER_SIZE) {
      iter->second->elementsInBuffer = iter->second->elementsInBuffer + 1;
    }
  } else {
    WARN_PRINT(***REMOVED***Could not find thread with id ***REMOVED*** << std::this_thread::get_id() <<
               ***REMOVED***. Have you forgotten to call SetStart() before?***REMOVED***);
  }
}

void TimeLogger::Impl::PrintSummary() {
  if (!isEnabled_) {
    WARN_PRINT(***REMOVED***TimeLogger is not enabled! Cannot print a summary.***REMOVED***);
    return;
  }

  INFO_PRINT(***REMOVED***Time usage per Thread summary:\n***REMOVED*** <<
             ***REMOVED***------------------------------\n***REMOVED***);
  for (auto iter = threadIdTimeLogEntryMap_.cbegin(); iter != threadIdTimeLogEntryMap_.cend(); iter++) {
    if (iter->second->elementsInBuffer == 0) {
      WARN_PRINT(***REMOVED***No time measurements were taken for thread with id ***REMOVED*** << iter->first << ***REMOVED***. Cannot print a summary.***REMOVED***);
    }

    dblArrSizeType_t start;
    if (iter->second->nextIndex == 0) {
      start =  TIME_LOGGER_RING_BUFFER_SIZE - 1;
    } else {
      start = iter->second->nextIndex - 1;
    }

    dblArrSizeType_t iMin = 0, iMax = 0;
    double min = std::numeric_limits<double>::max(),
        max = std::numeric_limits<double>::min(),
        avg = 0.0, sum = 0.0;
    for (dblArrSizeType_t i = 0; i < iter->second->elementsInBuffer; i++) {
      dblArrSizeType_t idx = (i + start) % TIME_LOGGER_RING_BUFFER_SIZE;
      double value = iter->second->ringBuffer[idx];
      sum += value;
      if (value < min) {
        iMin = i;
        min = value;
      }
      if (value > max) {
        iMax = i;
        max = value;
      }
    }
    avg = sum / iter->second->elementsInBuffer;

    INFO_PRINT(***REMOVED***min = ***REMOVED*** << std::setprecision(3) << min <<
               ***REMOVED***, max = ***REMOVED*** << max << ***REMOVED***, avg = ***REMOVED*** << avg << ***REMOVED*** seconds \n***REMOVED*** <<
               ***REMOVED***(last # values = ***REMOVED*** << iter->second->elementsInBuffer << ***REMOVED***, iMax = ***REMOVED*** << iMax << ***REMOVED***, iMin = ***REMOVED*** << iMin <<
               ***REMOVED***------------------------------\n***REMOVED***);
  }
}


// -----------------------------------------------------------------

TimeLogger::TimeLogger() : pImpl_(new TimeLogger::Impl()) {}

TimeLogger::~TimeLogger() = default;

void TimeLogger::Enable(bool enable) { pImpl_->Enable(enable); }

void TimeLogger::SetStart() { pImpl_->SetStart(); }

void TimeLogger::SetEnd() { pImpl_->SetEnd(); }

void TimeLogger::PrintSummary() { pImpl_->PrintSummary(); }