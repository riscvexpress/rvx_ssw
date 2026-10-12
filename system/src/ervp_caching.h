#ifndef __ERVP_CACHING_H__
#define __ERVP_CACHING_H__

#include <stdint.h>
#include "platform_info.h"
#include "core_dependent.h"

extern uintptr_t _cacheable_start;
extern uintptr_t _cacheable_last;

void register_cacheable_region(int index, uintptr_t cacheable_start, uintptr_t cacheable_last);
void print_cacheable_region();

#if defined(CACHING_NONE)

static inline int is_cacheable_region(void* ptr)
{
  return 0;
}

#elif defined(CACHING_SAFE)

static inline int is_cacheable_region(void* ptr)
{
  return (((uintptr_t)ptr) >= _cacheable_start) && (((uintptr_t)ptr) <= _cacheable_last);
}

#elif defined(CACHING_MOST) || defined(CACHING_ALL)

static inline int is_cacheable_region(void* ptr)
{
  return (((uintptr_t)ptr) >= FIXED_CACHEABLE_START) && (((uintptr_t)ptr) <= FIXED_CACHEABLE_LAST);
}

#endif

#endif
