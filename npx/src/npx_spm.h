#ifndef __NPX_SPM_H__
#define __NPX_SPM_H__

#include <stddef.h>
#include "platform_info.h"
#include "ervp_matrix_op_sw.h"

#if defined(USE_NPX_SPM)

void npx_spm_create(uintptr_t baseaddr, size_t size);
void npx_spm_destroy();
void npx_spm_print_all();
void npx_spm_report();

void *npx_spm_malloc(size_t size);
int npx_spm_free(void *ptr);

#else

static inline void npx_spm_create(uintptr_t baseaddr, size_t size) {};
static inline void npx_spm_destroy() {};
static inline void npx_spm_print_all() {};
static inline void npx_spm_report() {};

static inline void *npx_spm_malloc(size_t size) { return 0; };
static inline int npx_spm_free(void *ptr) { return 0; };

#endif

#endif