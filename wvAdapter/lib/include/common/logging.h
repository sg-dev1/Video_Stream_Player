#ifndef WVADAPTER_LOGGING_H_
#define WVADAPTER_LOGGING_H_

#include "common.h"

#include <thread>
#include <ostream>
#include <mutex>

#include <string.h>

namespace logging {
  extern WV_ADAPTER_TEST_API std::mutex lock_;
  extern WV_ADAPTER_TEST_API std::ostream nullStream_;

  enum class LogLevel {
    NONE = 0,
    TRACE,
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
  };

  WV_ADAPTER_TEST_API std::ostream& printLogMessage(const LogLevel &level, const char *file, int line,
                                const char *func, const std::thread::id tid);

  void addThreadIdNameMapping(const std::thread::id tid, const char *name);

  void setLogLevel(const LogLevel &level);
}

#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define LOGMSG_PRINT(level, msg)                                     \
   do {                                                              \
     std::unique_lock<std::mutex> lock(logging::lock_);              \
     logging::printLogMessage(level, __FILENAME__, __LINE__,             \
        __func__, std::this_thread::get_id()) << msg << std::endl;   \
   } while(0)

//
// Logging macros to be used by clients
//

#define TRACE_PRINT(msg) LOGMSG_PRINT(logging::LogLevel::TRACE, msg)
#define DEBUG_PRINT(msg) LOGMSG_PRINT(logging::LogLevel::DEBUG, msg)
#define INFO_PRINT(msg)  LOGMSG_PRINT(logging::LogLevel::INFO, msg)
#define WARN_PRINT(msg)  LOGMSG_PRINT(logging::LogLevel::WARNING, msg)
#define ERROR_PRINT(msg) LOGMSG_PRINT(logging::LogLevel::ERROR, msg)
#define FATAL_PRINT(msg) LOGMSG_PRINT(logging::LogLevel::FATAL, msg)


// old macros
// TODO replace

#define PRINT_BYTE_ARRAY(name, arr, len)                                       \
  do {                                                                         \
    printf("[INFO](%s:%d:%s) %s (%d): [ ", __FILE__, __LINE__, __func__, name, \
           len);                                                               \
    for (size_t k__ = 0; k__ < len; k__++) {                                   \
      printf("%02x ", (arr)[k__]);                                             \
    }                                                                          \
    printf("]\n");                                                             \
  } while (0);

/*
#define DEBUG_PRINT_ITERABLE(x)                                                \
  do {                                                                         \
    std::cout << "[DEBUG] (" << __FILE__ << ":" << __LINE__ << ":" << __func__ \
              << ":thread-id-" << std::this_thread::get_id() << ") " << #x     \
              << ": ";                                                         \
    for (auto const &value : x) {                                              \
      std::cout << value << ", ";                                              \
    }                                                                          \
    std::cout << std::endl;                                                    \
  } while (0);
*/

#define PRETTY_PRINT_BENTO4_FORMAT(msg, format)                                \
  do {                                                                         \
    printf("%s: %c%c%c%c\n", msg, (char)((format >> 24) & 0xFF),               \
           (char)((format >> 16) & 0xFF), (char)((format >> 8) & 0xFF),        \
           (char)((format)&0xFF));                                             \
  } while (0);

#endif // WVADAPTER_LOGGING_H_
