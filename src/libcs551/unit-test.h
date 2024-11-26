#ifndef UNIT_TEST_H_
#define UNIT_TEST_H_

/** fprint(stderr, "%s", msg) on stderr if not cond, preceded by
 *  file-name and line #.
 */
#define CHK(cond, msg) do { \
  if (!(cond)) fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, msg); \
} while(0)

/** fprint(stderr, fmt, ...) on stderr if not cond, preceded by
 *  file-name and line #.
 */
#define CHKF(cond, fmt, ...) do { \
  if (!(cond)) { \
    fprintf(stderr, "%s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, fmt "\n", __VA_ARGS__); \
  } \
} while(0)

#endif //#ifndef UNIT_TEST_H_
