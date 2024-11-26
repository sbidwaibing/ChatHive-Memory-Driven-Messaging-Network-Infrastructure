#include "str-space.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Management of dynamically-allocated space for NUL-terminated strings */

/** routines which return int use the return value to indicate the
 *  error status.  0 if everything okay, non-zero for a memory
 *  allocation error.
 */

// clients responsible for allocation/deallocation of this structure.
// note that clients should regard the insides of this struct as
// private.
typedef struct {
  size_t size;   /** # of allocated bytes in str */
  size_t index;  /** index of next free location in str */
  char *buf;     /** dynamically allocated buffer, NUL-terminated if not empty */
} _StrSpace; //definition from header file repeated here for easy access

#ifdef TEST_STR_SPACE
  enum { INIT_STR_SPACE_SIZE = 2 };
#else
  enum { INIT_STR_SPACE_SIZE = 128 };
#endif

static int
ensure_space(StrSpace *strSpace, size_t needed) {
  size_t available = strSpace->size - strSpace->index;
  if (available >= needed) return 0;
  size_t newSize = strSpace->size == 0 ? INIT_STR_SPACE_SIZE : 2*strSpace->size;
  if (newSize - strSpace->index < needed) newSize = strSpace->index + needed;
  if ((strSpace->buf = realloc(strSpace->buf, newSize)) == NULL) return 1;
  strSpace->size = newSize;
  return 0;
}

/** initialize this string space.  Must be called before first
 *  use of this strSpace.
 *
 *  No error return.
 */
void
init_str_space(StrSpace *strSpace)
{
  strSpace->index = strSpace->size = 0;
  strSpace->buf = NULL;
}

/** clear this string space so that it does not store any strings
 *
 *  No error return.
 */
void
clear_str_space(StrSpace *strSpace)
{
  strSpace->index = 0;
}

/** free all dynamic memory used by this strSpace.  *MUST* be called
 *  when strSpace is no longer needed. Note that this routine does not
 *  free the strSpace structure itself, as its lifetime is assumed to
 *  be controlled by the client.
 */
void
free_str_space(StrSpace *strSpace)
{
  free(strSpace->buf);
  init_str_space(strSpace);
}

/** add NUL-terminated string str to strSpace.  Note that str can be
 *  "" to start a new string for append.
 */
int
add_str_space(StrSpace *strSpace, const char *str)
{
  size_t n = strlen(str);
  if (ensure_space(strSpace, n + 1) != 0) return 1;
  strcpy(&strSpace->buf[strSpace->index], str);
  strSpace->index += n + 1;
  assert(strSpace->index <= strSpace->size);
  return 0;
}

/** append NUL-terminated string str to last str in strSpace.  If called
 *  when strSpace is empty, then simply adds string to strSpace.
 */
int
append_str_space(StrSpace *strSpace, const char *str)
{
  const size_t n = strlen(str);
  int isEmpty = strSpace->index == 0;
  const size_t needed = (isEmpty) ? n + 1 : n;
  if (ensure_space(strSpace, needed) != 0) return 1;
  strcpy(&strSpace->buf[strSpace->index - !isEmpty], str);
  strSpace->index += n + isEmpty;
  assert(strSpace->index <= strSpace->size);
  return 0;
}

/** appends a string specified by a sprintf() string to strSpace.
 *  Like sprintf(&strSpaceFreeBuf, fmt, ...) where strSpaceFreeBuf
 *  points to the NUL of the last string in strSpace or the start
 *  of the strSpace buf if empty.
 */
__attribute__ ((format(printf, 2, 3)))
int
append_sprintf_str_space(StrSpace *strSpace, const char *fmt, ...)
{
  int isEmpty = strSpace->index == 0;
  size_t available = strSpace->size - (strSpace->index - !isEmpty);
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(&strSpace->buf[strSpace->index - !isEmpty],
                    available, fmt, ap);
  va_end(ap);
  va_start(ap, fmt);
  if (n >= available) {
    if (ensure_space(strSpace, n+1) != 0) return 1;
    vsnprintf(&strSpace->buf[strSpace->index - !isEmpty],
              n+1, fmt, ap);
  }
  va_end(ap);
  strSpace->index += n - !isEmpty + 1;
  return 0;
}

/** External iterator used to iterate through strings in strSpace
 *  using lastStr.  Specifically, if called with lastStr NULL, it a
 *  pointer to the first NUL-terminated string in strSpace; if called
 *  with non-NULL, then it returns a pointer to the next string in
 *  strSpace after lastStr, NULL if none.
 *
 *  To iterate over all strings in strSpace:
 *  for (const char *str = iter_str_space(strSpace, NULL);
 *       str != NULL;
 *       str = iter_str_space(strSpace, NULL)) {
 *    // do something with str
 *  }
 *
 *  If strSpace was used to construct a single string with multiple
 *  calls to append routines, then this routine can be used to
 *  access that string:
 *  const char *str = iter_str_space(strSpace, NULL);
 *
 *  The results are undefined if strSpace is modified during the iteration.
 *
 *  No error return.
 */
const char *
iter_str_space(const StrSpace *strSpace, const char *lastStr)
{
  if (lastStr == NULL) {
    return (strSpace->index > 0) ? strSpace->buf : NULL;
  }
  else {
    const char *nextStr = lastStr + strlen(lastStr) + 1;
    return (nextStr < &strSpace->buf[strSpace->index]) ? nextStr : NULL;
  }
}


/**************************** Unit Tests *******************************/

//whitebox testing

#ifdef TEST_STR_SPACE

#include "unit-test.h"

static void
test_ensure_space(void)
{
  StrSpace s;

  init_str_space(&s);
  assert(INIT_STR_SPACE_SIZE > 1);
  ensure_space(&s, 1);
  CHKF(s.size == INIT_STR_SPACE_SIZE,
       "ALLOC_1: %zu != %d", s.size, INIT_STR_SPACE_SIZE);
  free_str_space(&s);

  init_str_space(&s);
  ensure_space(&s, INIT_STR_SPACE_SIZE);
  CHKF(s.size == INIT_STR_SPACE_SIZE,
       "ALLOC_INIT_STR_SPACE_SIZE: %zu != %d", s.size, INIT_STR_SPACE_SIZE);
  free_str_space(&s);

  init_str_space(&s);
  ensure_space(&s, INIT_STR_SPACE_SIZE + 1);
  CHKF(s.size == INIT_STR_SPACE_SIZE + 1,
       "ALLOC_EXCESS: %zu != %d", s.size, INIT_STR_SPACE_SIZE + 1);
  free_str_space(&s);

}

static void
fill(char buf[], size_t bufSize, char c) {
  memset(buf, c, bufSize - 1); buf[bufSize - 1] = '\0';
}

static void
test_add_str_space(void)
{
  StrSpace s;
  const size_t initSize2 = 2 * INIT_STR_SPACE_SIZE;
  char str[initSize2 + 1];

  init_str_space(&s);
  fill(str, initSize2 + 1, 'x');
  add_str_space(&s, str);
  CHKF(s.size == initSize2 + 1,
       "ADD_INIT2: %zu != %zu", s.size, initSize2);
  CHKF(strcmp(s.buf, str) == 0,
       "ADD_INIT2_STR: %s != %s", s.buf, str);

  clear_str_space(&s);

  fill(str, initSize2 + 1, 'x');
  add_str_space(&s, str);
  CHKF(s.size == initSize2 + 1,
       "ADD_INIT2: %zu != %zu", s.size, initSize2 + 1);
  CHKF(strcmp(s.buf, str) == 0,
       "ADD_INIT2_1_STR: %s != %s", s.buf, str);
  free_str_space(&s);

}

static void
test_append_str_space(void)
{
  // assuming INIT_STR_SPACE_SIZE is S, we will append 3 strings:
  // a) a string s1 with strlen(s1) == S - 1; expect index == size == S
  // b) a string s2 with strlen(s2) == S; expect index == size == 2*S
  // c) a string s3 with strlen(s3) == S; expect index == 3*S; size == 4*S
  StrSpace s;
  init_str_space(&s);

  const size_t S = INIT_STR_SPACE_SIZE;
  char str[S*3 + 1];

  fill(str, S, '1');
  assert(strlen(str) == 1*S - 1);
  append_str_space(&s, str);
  CHKF(s.index == S, "APPEND1_INDEX: %zu != %zu", s.index, S);
  CHKF(s.size == S, "APPEND1_SIZE: %zu != %zu", s.size, S);
  CHKF(strcmp(s.buf, str) == 0, "APPEND_1_STR: %s != %s", s.buf, str);

  fill(&str[S-1], S+1, '2');
  assert(strlen(str) == 2*S - 1);
  append_str_space(&s, &str[S-1]);
  CHKF(s.index == 2*S, "APPEND2_INDEX: %zu != %zu", s.index, 2*S);
  CHKF(s.size == 2*S, "APPEND2_SIZE: %zu != %zu", s.size, 2*S);
  CHKF(strcmp(s.buf, str) == 0, "APPEND2_STR: %s != %s", s.buf, str);

  fill(&str[2*S-1], S + 1, '3');
  assert(strlen(str) == 3*S - 1);
  append_str_space(&s, &str[2*S-1]);
  CHKF(s.index == 3*S, "APPEND3_INDEX: %zu != %zu", s.index, 3*S);
  CHKF(s.size == 4*S, "APPEND3_SIZE: %zu != %zu", s.size, 4*S);
  CHKF(strcmp(s.buf, str) == 0, "APPEND3_STR: %s != %s", s.buf, str);

  free_str_space(&s);

}

static void
test_append_sprintf_str_space(void)
{
  StrSpace s;
  init_str_space(&s);

  append_sprintf_str_space(&s, "hello %s to ", "world");
  #define STR1 "hello world to "
  CHKF(s.index == strlen(STR1) + 1,
       "SPRINTF1_INDEX: %zu != %zu", s.index, strlen(STR1) + 1);
  CHKF(strcmp(s.buf, STR1) == 0, "SPRINTF1: \"%s\" != \"%s\"", s.buf, STR1);

  append_sprintf_str_space(&s, "%d girls and ", 5);
  #define STR2 STR1 "5 girls and "
  CHKF(s.index == strlen(STR2) + 1,
       "SPRINTF2_INDEX: %zu != %zu", s.index, strlen(STR2) + 1);
  CHKF(strcmp(s.buf, STR2) == 0, "SPRINTF1: \"%s\" != \"%s\"", s.buf, STR2);

  append_sprintf_str_space(&s, "%d boys.", 3);
  #define STR3 STR2 "3 boys."
  CHKF(s.index == strlen(STR3) + 1,
       "SPRINTF3_INDEX: %zu != %zu", s.index, strlen(STR3) + 1);
  CHKF(strcmp(s.buf, STR3) == 0, "SPRINTF1: \"%s\" != \"%s\"", s.buf, STR3);

  free_str_space(&s);
  #undef STR1
  #undef STR2
  #undef STR3
}

static void
test_iter_str_space(void)
{
  StrSpace s;
  init_str_space(&s);

  const char *iter;

  #define STR1 "hello"
  add_str_space(&s, STR1);
  iter = iter_str_space(&s, NULL);
  CHKF(strcmp(iter, STR1) == 0, "ITER1: \"%s\" != \"%s\"", iter, STR1);
  iter = iter_str_space(&s, iter);
  CHKF(iter == NULL, "ITER1_DONE: \"%s\" != NULL", iter);

  #define STR2 "world"
  add_str_space(&s, STR2);
  iter = iter_str_space(&s, NULL);
  CHKF(strcmp(iter, STR1) == 0, "ITER2_0: \"%s\" != \"%s\"", iter, STR1);
  iter = iter_str_space(&s, iter);
  CHKF(strcmp(iter, STR2) == 0, "ITER2_1: \"%s\" != \"%s\"", iter, STR2);
  iter = iter_str_space(&s, iter);
  CHKF(iter == NULL, "ITER2_DONE: \"%s\" != NULL", iter);

  #define STR3 "goodbye"
  add_str_space(&s, STR3);
  append_str_space(&s, STR2);
  iter = iter_str_space(&s, NULL);
  CHKF(strcmp(iter, STR1) == 0, "ITER3_0: \"%s\" != \"%s\"", iter, STR1);
  iter = iter_str_space(&s, iter);
  CHKF(strcmp(iter, STR2) == 0, "ITER3_1: \"%s\" != \"%s\"", iter, STR2);
  iter = iter_str_space(&s, iter);
  CHKF(strcmp(iter, STR3 STR2) == 0,
       "ITER3_2: \"%s\" != \"%s\"", iter, STR3 STR2);
  iter = iter_str_space(&s, iter);
  CHKF(iter == NULL, "ITER3_DONE: \"%s\" != NULL", iter);

  #define STR4 "sayonara"
  add_str_space(&s, "");
  append_sprintf_str_space(&s, "%s", STR4);
  append_sprintf_str_space(&s, "%s", STR2);
  iter = iter_str_space(&s, NULL);
  CHKF(strcmp(iter, STR1) == 0, "ITER4_0: \"%s\" != \"%s\"", iter, STR1);
  iter = iter_str_space(&s, iter);
  CHKF(strcmp(iter, STR2) == 0, "ITER4_1: \"%s\" != \"%s\"", iter, STR2);
  iter = iter_str_space(&s, iter);
  CHKF(strcmp(iter, STR3 STR2) == 0,
       "ITER4_2: \"%s\" != \"%s\"", iter, STR3 STR2);
  iter = iter_str_space(&s, iter);
  CHKF(strcmp(iter, STR4 STR2) == 0,
       "ITER4_3: \"%s\" != \"%s\"", iter, STR4 STR2);
  iter = iter_str_space(&s, iter);
  CHKF(iter == NULL, "ITER4_DONE: \"%s\" != NULL", iter);


  free_str_space(&s);
}

int
main()
{
  test_ensure_space();
  test_add_str_space();
  test_append_str_space();
  test_append_sprintf_str_space();
  test_iter_str_space();
}

#endif //#ifdef TEST_STR_SPACE
