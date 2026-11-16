#include <stdarg.h>

#include "ervp_assert.h"
#include "ervp_log.h"
#include "ervp_platform_api.h"
#include "ervp_variable_allocation.h"

void _assert_fail_rvx(const char* file, unsigned int line, const char* func, const char* format, ...)
{
  const char *file_name = debug_get_file_name_only(file);

  va_list va;
  va_start(va, format);
  rvx_vlog(RVX_LOG_ERROR, "ASSERT", format, va);
  va_end(va);

  printf_must("\n[%s @%d] %s", file_name, line, func);

  exit(ERROR_ASSERT);
}

__attribute__ ((weak))
int check_if_data_pointer_is_valid(void *p)
{
  int valid = 0;
  if (0)
    ;
#ifdef INCLUDE_SMALL_RAM
  else if ((p >= SMALL_RAM_BASEADDR) && (p <= SMALL_RAM_LASTADDR))
    valid = 1;
#endif
#ifdef INCLUDE_LARGE_RAM
#ifndef USE_LARGE_RAM_MANUALLY
  else if ((p >= LARGE_RAM_BASEADDR) && (p <= LARGE_RAM_LASTADDR))
    valid = 1;
#endif
#endif
  return valid;
}