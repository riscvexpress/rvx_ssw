#ifndef __ERVP_PLATFORM_API_H__
#define __ERVP_PLATFORM_API_H__

#include <stdint.h>

#include "ervp_error_code.h"
#include "ervp_printf.h"

typedef uintptr_t linker_var_t;
#define GET_LINKER_VAR(x) ((linker_var_t)(&x))

extern int num_restart;

void print_platform_info();
void print_rvx_info();
int wait_or_initialize();
void _set_initialized();
void _clear_bss(int bss_start, int bss_end);
void _init_platform();
void _init_heap();
void _init_each_core();
void _reboot_nvm();
void _stop_cpu();

void worker_core_entry();
void exit_platform();

#define exit exit_rvx
__attribute__((noreturn)) void exit_rvx(unsigned int status);

void print_linker_var();

#endif
