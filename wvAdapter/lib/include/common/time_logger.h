#ifndef WV_ADAPTER_LIB_TIME_LOGGER_H
#define WV_ADAPTER_LIB_TIME_LOGGER_H

#include <memory>

class TimeLogger {
public:
  TimeLogger();
  ~TimeLogger();

  void Enable(bool enable);

  void SetStart();
  void SetEnd();

  void PrintSummary();

private:
  class Impl;
  std::unique_ptr<Impl> pImpl_;
};

#endif //WV_ADAPTER_LIB_TIME_LOGGER_H
