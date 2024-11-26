#ifndef VECTOR_H_
#define VECTOR_H_

#include <stddef.h>

/** dynamically grown vector of arbitrary sized elements */

// clients responsible for allocation/deallocation of this structure.
// note that clients should regard the insides of this struct as
// private.
typedef struct {
  size_t len;
  size_t elementSize;
  size_t capacity;
  void **elements;      //dynamically managed by this module
} Vector;


/** initialize vec to store elements of size elementSize.  No error return */
void init_vector(Vector *vec, size_t elementSize);

/** clear out vec */
void clear_vector(Vector *vec);

/** free all dynamic memory used by this vec.  *MUST* be called
 *  when vec is no longer needed. Note that this routine does not
 *  free the Vector structure itself, as its lifetime is assumed to
 *  be controlled by the client.
 *
 *  No error return.
 */
void free_vector(Vector *vec);

/** add a element and set it to a mem-copy of elementSize bytes from *entry.
 *  Returns 0 if ok, non-0 on alloc error
 */
int add_vector(Vector *vec, void *entry);

/** return # of elements currently stored in vec; no error return  */
size_t n_elements_vector(const Vector *vec);

/** fill in *element with elementSize bytes of vec[index].
 *  Returns 0 if ok, non-0 if bad index
 */
int get_element_vector(const Vector *vec, size_t index, void *element);

/** return base of vector[n_elements].  Valid only while no elements added */
void *get_base_vector(const Vector *vec);

#endif //#ifndef VECTOR_H_
