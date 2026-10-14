#ifndef DATAFRAMELIB_H
#define DATAFRAMELIB_H

// Include Arrow utilities
#include <arrow/api.h>
#include <arrow/util/logging.h>

// Include all library headers
#include "dataframelib/expression.h"
#include "dataframelib/apachearrow.h"
#include "dataframelib/graph.h"
#include "dataframelib/eagerdataframe.h"
#include "dataframelib/lazydataframe.h"
#include "dataframelib/queryoptimiser.h"

// Define ARROW_THROW_NOT_OK if not already defined
#ifndef ARROW_THROW_NOT_OK
#define ARROW_THROW_NOT_OK(expr)                                    \
  do {                                                              \
    ::arrow::Status __s = (expr);                                   \
    if (!__s.ok()) {                                                \
      throw std::runtime_error(__s.ToString());                     \
    }                                                               \
  } while (0)
#endif

#endif