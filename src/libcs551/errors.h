#ifndef ERRORS_H_
#define ERRORS_H_

#include <stdio.h>
/** Print a error message on stderr as per printf-style fmt and
 *  optional arguments.  If fmt ends with ':', follow with a space,
 *  strerror(errno), followed by a newline.
 *
 *  Always returns a 1.
 */
__attribute__ ((format(printf, 1, 2)))
int error(const char *fmt, ...);

/** Print a error message on err as per printf-style fmt and optional
 *  arguments.  If fmt ends with ':', follow with a space,
 *  strerror(errno), followed by a newline.
 *
 *  Always return 1;
 */
__attribute__ ((format(printf, 2, 3)))
int errorf(FILE *err, const char *fmt, ...);

/** Similar to GNU's asprintf().  Specifically, sets *err to a
 *  dynamically allocated string which is printf(fmt, ...) with the
 *  following additions:
 *
 *   a) If *err is not NULL, then *err is included as the prefix of
 *      the returned string.  The original *err is assumed to have
 *      been dynamically allocated and is free()'d before return.
 *      This allows accumulating errors.
 *   b) If fmt ends with a ':' then a space followed by strerror(errno)
 *      is appended.
 *   c) A newline is always appended.
 *
 *  If dynamic memory allocation fails, then ignores fmt, ..., prints
 *  a suitable message on stderr and sets *err to NULL afer free()'ing
 *  previous value if any.
 *
 *  Always return 1;
 */
int aerror(char **err, const char *fmt, ...);

/* Print a error message on stderr as per printf-style fmt and
 * optional arguments.  If fmt ends with ':', follow with a space,
 * strerror(errno), followed by a newline.  Terminate the program.
 */
__attribute__ ((noreturn,  format(printf, 1, 2)))
void fatal(const char *fmt, ...);

/* Print a error message on err as per printf-style fmt and
 * optional arguments.  If fmt ends with ':', follow with a space,
 * strerror(errno), followed by a newline.  Terminate the program.
 */
__attribute__ ((noreturn,  format(printf, 2, 3)))
void fatalf(FILE *err, const char *fmt, ...);

#endif /* #ifndef ERRORS_H_ */
