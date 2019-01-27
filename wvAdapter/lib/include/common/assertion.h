#ifndef ASSERTION_H_
#define ASSERTION_H_

#include ***REMOVED***constants.h***REMOVED***

#include <stdexcept>

#define ASSERT_MSG(x, msg)                                                     \
  do {                                                                         \
    if (!(x)) {                                                                \
      ERROR_PRINT(***REMOVED***ASSERTION ERROR: following condition violated: ***REMOVED*** << #x      \
                  << ***REMOVED***. ***REMOVED*** << msg);                                             \
      throw std::runtime_error(***REMOVED***assertion error***REMOVED***);                             \
    }                                                                          \
  } while (false);
#define ASSERT(x) ASSERT_MSG(x, ***REMOVED******REMOVED***)

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
#define DEBUG_ASSERT_MSG(x, msg) ASSERT_MSG(x, msg)
#define DEBUG_ASSERT(x)          ASSERT(x)
#else
#define DEBUG_ASSERT_MSG(x, msg)
#define DEBUG_ASSERT(x)
#endif

#endif // ASSERTION_H_
