#include "ervp_caching.h"
#include "ervp_printf.h"

uintptr_t _cacheable_start = 0;
uintptr_t _cacheable_last = 0;

void register_cacheable_region(int index, uintptr_t cacheable_start, uintptr_t cacheable_last)
{
  _cacheable_start = cacheable_start;
  _cacheable_last = cacheable_last;
}

void print_cacheable_region()
{
  debug_printx(_cacheable_start);
  debug_printx(_cacheable_last);
}