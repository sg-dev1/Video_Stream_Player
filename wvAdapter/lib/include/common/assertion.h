#ifndef ASSERTION_H_
#define ASSERTION_H_

#include "constants.h"

#include <stdexcept>

#define ASSERT_MSG(x, msg)                                                     \
  do {                                                                         \
    if (!(x)) {                                                                \
      ERROR_PRINT("ASSERTION ERROR: following condition violated: " << #x      \
                  << ". " << msg);                                             \
      throw std::runtime_error("assertion error");                             \
    }                                                                          \
  } while (false);
#define ASSERT(x) ASSERT_MSG(x, "")

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
#define DEBUG_ASSERT_MSG(x, msg) ASSERT_MSG(x, msg)
#define DEBUG_ASSERT(x)          ASSERT(x)
#else
#define DEBUG_ASSERT_MSG(x, msg)
#define DEBUG_ASSERT(x)
#endif

#endif // ASSERTION_H_
