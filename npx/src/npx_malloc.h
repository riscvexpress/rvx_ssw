#ifndef __NPX_MALLOC_H__
#define __NPX_MALLOC_H__

#include <stddef.h>

#include "ervp_assert.h"
#include "ervp_smart_flush.h"
#include "npx_spm.h"

#define npx_malloc trackedvar_malloc
#define npx_free trackedvar_free

void *npx_malloc_prefer_spm(size_t size);

#endif
