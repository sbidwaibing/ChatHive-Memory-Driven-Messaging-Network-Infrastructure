#ifndef STR_SPACE_H_
#define STR_SPACE_H_

#include <stddef.h>

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
} StrSpace;


/** initialize this string space.  Must be called before first
 *  use of this strSpace.
 *
 *  No error return.
 */
void init_str_space(StrSpace *strSpace);

/** free all dynamic memory used by this strSpace.  *MUST* be called
 *  when strSpace is no longer needed. Note that this routine does not
 *  free the strSpace structure itself, as its lifetime is assumed to
 *  be controlled by the client.
 *
 *  No error return.
 */
void free_str_space(StrSpace *strSpace);

/** clear this string space so that it does not store any strings
 *
 *  No error return.
 */
void clear_str_space(StrSpace *strSpace);

/** add NUL-terminated string str to strSpace.  Note that str can be
 *  "" to start a new string for append_str_space() or sprintf_str_space().
 */
int add_str_space(StrSpace *strSpace, const char *str);

/** append NUL-terminated string str to last str in strSpace.  If called
 *  when strSpace is empty, then simply adds string to strSpace.
 */
int append_str_space(StrSpace *strSpace, const char *str);

/** appends a string specified by a sprintf() string to strSpace.
 *  Like sprintf(&strSpaceFreeBuf, fmt, ...) where strSpaceFreeBuf
 *  points to the NUL of the last string in strSpace-buf or the start
 *  of the strSpace buf if empty.
 */
__attribute__ ((format(printf, 2, 3)))
int append_sprintf_str_space(StrSpace *strSpace, const char *fmt, ...);

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
const char *iter_str_space(const StrSpace *strSpace, const char *lastStr);

#endif //#ifndef STR_SPACE_H_
