#include "ervp_log.h"
#include "ervp_printf.h"

static const char* const level_name[] = {"ERROR", "WARNING", "INFO", "PROFILE", "DEBUG"};

void rvx_vlog(int level, const char* module, const char* format, va_list va)
{
#ifdef SILENT_FOR_PROFILING
  if (level == RVX_LOG_DEBUG)
    return;
#endif
  printf_rvx("\n[%s/%s] ", level_name[level], module);
  vprintf_rvx(format, va);
}

void rvx_log(int level, const char* module, const char* format, ...)
{
  va_list va;
  va_start(va, format);
  rvx_vlog(level, module, format, va);
  va_end(va);
}
