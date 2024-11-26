#include "errors.h"

#include <stdlib.h>

void *mallocChk(size_t size)
{
  void *p = malloc(size);
  if (!p) fatal("malloc failure for size %zu:", size);
  return p;
}

void *reallocChk(void *ptr, size_t size)
{
  void *p = realloc(ptr, size);
  if (!p) fatal("realloc failure for size %zu:", size);
  return p;
}

void *callocChk(size_t nmemb, size_t size)
{
  void *p = calloc(nmemb, size);
  if (!p) fatal("calloc failure for nmemb %zu of size %zu:", nmemb, size);
  return p;
}
