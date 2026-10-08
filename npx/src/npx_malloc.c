#include "npx_malloc.h"

void *npx_malloc_prefer_spm(size_t size)
{
  void *result;
  result = npx_spm_malloc(size);
  if (!result)
    result = npx_malloc(size);
  assert(result);
#if 0
  debug_printx((unsigned int)result);
#endif
  return result;
}