#include "errors.h"

#include <assert.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

/* Print a error message on err as per printf-style fmt and
 * args given by ap.  If fmt ends with ':', follow with a space,
 * strerror(errno), followed by a newline.
 */
static void
verrorf(FILE *err, const char *fmt, va_list ap)
{
  vfprintf(err, fmt, ap);
  if (fmt[strlen(fmt) - 1] == ':') fprintf(err, " %s", strerror(errno));
  fprintf(err, "\n");
}

/** Print a error message on err as per printf-style fmt and optional
 *  arguments.  If fmt ends with ':', follow with a space,
 *  strerror(errno), followed by a newline.
 *
 *  Always return 1;
 */
int
errorf(FILE *err, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  verrorf(err, fmt, ap);
  va_end(ap);
  return 1;
}

/** Print a error message on stderr as per printf-style fmt and
 *  optional arguments.  If fmt ends with ':', follow with a space,
 *  strerror(errno), followed by a newline.
 *
 *  Always return 1;
 */
int
error(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  verrorf(stderr, fmt, ap);
  va_end(ap);
  return 1;
}

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
int
aerror(char **err, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  const char *prefix = (*err) ? *err : "";
  bool isSys = fmt[strlen(fmt) - 1] == ':';
  const char *sysErrorPrefix = (isSys) ? " " : "";
  const char *sysError = (isSys) ? strerror(errno) : "";
  int msgLen = vsnprintf(NULL, 0, fmt, ap);
  va_end(ap);
  size_t totalLen =
    strlen(prefix) + msgLen + strlen(sysErrorPrefix) + strlen(sysError) + 1;
  char *buf = malloc(totalLen + 1);
  if (!buf) {
    error("aerror() malloc() failure:");
    free(*err); //note that free(NULL) is ok
    *err = NULL;
  }
  else {
    va_start(ap, fmt);
    int i = 0;
    strcpy(&buf[i], prefix);
    i += strlen(prefix);
    free(*err); //note that free(NULL) is ok
    i += vsnprintf(&buf[i], totalLen + 1 - i, fmt, ap);
    va_end(ap);
    i += snprintf(&buf[i], totalLen + 1 - i, "%s%s\n",
                  sysErrorPrefix, sysError);
    assert(i == totalLen);
    *err = buf;
  }
  return 1;
}

/* Print a error message on err as per printf-style fmt and
 * arguments given by ap.  If fmt ends with ':', follow with a space,
 * strerror(errno), followed by a newline.  Terminate the program.
 */
static void
__attribute__ ((noreturn))
vfatalf(FILE *err, const char *fmt, va_list ap)
{
  vfprintf(err, fmt, ap);
  if (fmt[strlen(fmt) - 1] == ':') fprintf(err, " %s", strerror(errno));
  fprintf(err, "\n");
  _exit(1);  //terminate with extreme prejudice; (exit() not MT-safe)
}

/* Print a error message on stderr as per printf-style fmt and
 * optional arguments.  If fmt ends with ':', follow with a space,
 * strerror(errno), followed by a newline.  Terminate the program.
 */
void
fatal(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vfatalf(stderr, fmt, ap);
}

/* Print a error message on err as per printf-style fmt and
 * optional arguments.  If fmt ends with ':', follow with a space,
 * strerror(errno), followed by a newline.  Terminate the program.
 */
void
fatalf(FILE *err, const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vfatalf(err, fmt, ap);
}
