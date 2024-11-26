#include "errnum.h"

#include <assert.h>

// must be in same order as ErrNum enum
// (this order dependency can be removed using macros)
static const char *errMsgs[] = {
  "no error",
  "memory allocation error",
  "I/O error",
};

/** return a static string explaining err */
const char *
errnum_to_string(ErrNum err)
{
  const int nErrNums = sizeof(errMsgs)/sizeof(errMsgs[0]);
  assert(err < nErrNums);
  return errMsgs[err];
}
