#ifndef MEMALLOC_H_
#define MEMALLOC_H_

#include <stdlib.h>

//Error checking wrappers around memory allocation routines.  Will
//terminate program on error.

void *mallocChk(size_t size);

void *reallocChk(void *ptr, size_t size);

void *callocChk(size_t nmemb, size_t size);

#endif /* #ifndef MEMALLOC_H_ */
