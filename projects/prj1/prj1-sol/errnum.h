#ifndef ERRNUM_H_
#define ERRNUM_H_

typedef enum {
  NO_ERR,
  MEM_ERR,
  IO_ERR,
} ErrNum;

/** return a static string explaining err */
const char *errnum_to_string(ErrNum err);

#endif // #ifndef ERRNUM_H_
