#include "vector.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** dynamically grown vector of arbitrary sized elements */

// clients responsible for allocation/deallocation of this structure.
// note that clients should regard the insides of this struct as
// private.
typedef struct {
  size_t len;
  size_t elementSize;
  size_t capacity;
  void **elements;      //dynamically managed by this module
} _Vector; //definition from header file repeated here for easy access


/** initialize vec to store elements of size elementSize.  No error return */
void
init_vector(Vector *vec, size_t elementSize)
{
  vec->len = vec->capacity = 0;
  vec->elementSize = elementSize;
  vec->elements = NULL;
}

/** clear out vec */
void
clear_vector(Vector *vec)
{
  vec->len = 0;
}

/** free all dynamic memory used by this vec.  *MUST* be called
 *  when vec is no longer needed. Note that this routine does not
 *  free the Vector structure itself, as its lifetime is assumed to
 *  be controlled by the client.
 *
 *  No error return.
 */
void
free_vector(Vector *vec)
{
  free(vec->elements);
  init_vector(vec, 0);
}

/** add a element and set it to a mem-copy of elementSize bytes from *entry.
 *  Returns 0 if ok, non-0 on alloc error
 */
int
add_vector(Vector *vec, void *entry)
{
  enum { INIT_VEC_SIZE = 16 };
  if (vec->len == vec->capacity) {
    size_t newSize = vec->capacity == 0 ? INIT_VEC_SIZE : 2*vec->capacity;
    void *p = realloc(vec->elements, newSize * vec->elementSize);
    if (!p) return 1;
    vec->capacity = newSize; vec->elements = p;
  }
  assert(vec->len < vec->capacity);
  memcpy(&vec->elements[vec->len], entry, vec->elementSize);
  vec->len++;
  return 0;
}

/** return # of elements currently stored in vec; no error return  */
size_t
n_elements_vector(const Vector *vec)
{
  return vec->len;
}

/** fill in *element with elementSize bytes of vec[index].
 *  Returns 0 if ok, non-0 if bad index
 */
int
get_element_vector(const Vector *vec, size_t index, void *element)
{
  if (index >= vec->len) return 1;
  memcpy(element, &vec->elements[index], vec->elementSize);
  return 0;
}

/** return base of vector[n_elements].  Valid only while no elements added */
void *
get_base_vector(const Vector *vec)
{
  return vec->elements;
}
