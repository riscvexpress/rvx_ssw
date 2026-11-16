#ifndef __ERVP_LOG_H__
#define __ERVP_LOG_H__

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RVX_LOG_ERROR 0
#define RVX_LOG_WARNING 1
#define RVX_LOG_INFO 2
#define RVX_LOG_PROFILE 3
#define RVX_LOG_DEBUG 4

void rvx_log(int level, const char* module, const char* format, ...);
void rvx_vlog(int level, const char* module, const char* format, va_list va);

#define rvx_log_error(module, ...) rvx_log(RVX_LOG_ERROR, module, __VA_ARGS__)
#define rvx_log_warning(module, ...) rvx_log(RVX_LOG_WARNING, module, __VA_ARGS__)
#define rvx_log_info(module, ...) rvx_log(RVX_LOG_INFO, module, __VA_ARGS__)
#define rvx_log_profile(module, ...) rvx_log(RVX_LOG_PROFILE, module, __VA_ARGS__)
#define rvx_log_debug(module, ...) rvx_log(RVX_LOG_DEBUG, module, __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif
