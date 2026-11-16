#ifndef __ERVP_ASSERT_H__
#define __ERVP_ASSERT_H__

#include "ervp_platform_api.h"
#include "ervp_printf.h"

#ifdef NDEBUG
  #define assert(ignore) ((void)0)
  #define assert_msg(...) ((void)0)
#else
  #define assert(expr) assert_must(expr)
  #define assert_msg(expr, ...) assert_must_msg(expr, __VA_ARGS__)
#endif

#define assert_must(expr) assert_must_msg(expr, "%s", #expr)
#define assert_must_msg(expr, ...) assert_must_info(expr, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define assert_must_info(expr, file, line, func, ...) do {if(!(expr)) _assert_fail_rvx(file, line, func, __VA_ARGS__);} while(0)

__attribute__((noreturn)) void _assert_fail_rvx(const char* file, unsigned int line, const char* func, const char* format, ...);

#define assert_pointer_align(p, n) assert((((uintptr_t)(p))&(n-1))==0)

int check_if_data_pointer_is_valid(void* p);

#define assert_not_implemented() { printf_must("\n"); printf_must("Not Implemented"); assert_must(0);}

#endif
