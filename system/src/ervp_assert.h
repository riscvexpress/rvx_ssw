#ifndef __ERVP_ASSERT_H__
#define __ERVP_ASSERT_H__

#include "ervp_platform_api.h"
#include "ervp_printf.h"

#ifdef NDEBUG
  #define assert(ignore) ((void)0)
  #define assert_msg(...) ((void)0)
#else
  #define assert(expr) assert_must(expr)
  #define assert_msg(expr, ...) do {if(!(expr)) { assert_must_msg(expr, __VA_ARGS__); }} while(0)
#endif

#define assert_must(expr) assert_must_msg(expr, #expr)
#define assert_must_msg(expr, ...) assert_must_info(expr, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define assert_must_info(expr, file, line, func, ...) do {if(!(((unsigned int)(expr)))) _assert_fail_rvx(file, line, func, __VA_ARGS__);} while(0)

__attribute__((noreturn)) void _assert_fail_rvx(const char* file, unsigned int line, const char* func, const char* format, ...);

#define POINTEROR1(a1)                        ((uintptr_t)a1)
#define POINTEROR2(a1, a2)                    (((uintptr_t)a1) | ((uintptr_t)a2))
#define POINTEROR3(a1, a2, a3)                (((uintptr_t)a1) | ((uintptr_t)a2) | ((uintptr_t)a3))
#define POINTEROR4(a1, a2, a3, a4)            (((uintptr_t)a1) | ((uintptr_t)a2) | ((uintptr_t)a3) | ((uintptr_t)a4))
#define POINTEROR5(a1, a2, a3, a4, a5)        (((uintptr_t)a1) | ((uintptr_t)a2) | ((uintptr_t)a3) | ((uintptr_t)a4) | ((uintptr_t)a5))

#define assert_pointer(n, ...) assert(POINTEROR##n(__VA_ARGS__))
#define assert_pointer_align(p, n) assert((((uintptr_t)(p))&(n-1))==0)

int check_if_data_porinter_is_valid(void* p);

#define assert_invalid_data_pointer(p) assert_msg(check_if_data_porinter_is_valid(p), "invalid data pointer: 0x%x", p)

#define assert_not_implemented() { printf_must("\n"); printf_must("Not Implemented"); assert_must(0);}

#endif
