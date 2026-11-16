#include "platform_info.h"

#include "utlist.h"
#include "uthash.h"

#include "ervp_printf.h"
#include "ervp_log.h"
#include "ervp_misc_util.h"
#include "ervp_malloc_system.h"
#include "ervp_malloc.h"
#include "ervp_assert.h"
#include "ervp_memory_allocator.h"
#include "ervp_variable_allocation.h"
#include "ervp_core_id.h"
#include "ervp_per_core.h"

#ifdef USE_REUSE_MEMORY_ALLOCATOR
static memory_allocator_t default_allocator PER_CORE_DATA;
#endif

__attribute__((weak)) void *malloc_rvx(size_t size)
{
  void *ptr = NULL;
  if (size > 0)
  {
#ifdef USE_REUSE_MEMORY_ALLOCATOR
    memory_block_info_t *block;
    size_t extended_size = ALIGN_UP_POW2(size + MEMORY_BLOCK_INFO_SIZE, DATA_ALIGN_SIZE);
    if (extended_size < size)
      return NULL;
    block = memory_allocator_pop(&PER_CORE(default_allocator), extended_size);
    if (block == NULL)
    {
      block = (memory_block_info_t *)_alloc_new_memory_space(extended_size);
      if (block == NULL)
        return NULL;
      block->size = extended_size;
    }
    ptr = (void *)(((unsigned int)block) + MEMORY_BLOCK_INFO_SIZE);
#else
    size_t extended_size = ALIGN_UP_POW2(size, DATA_ALIGN_SIZE);
    if (extended_size < size)
      return NULL;
    ptr = (memory_block_info_t *)_alloc_new_memory_space(extended_size);
#endif
  }

#if 0
      rvx_log_debug("MALLOC", "malloc %p %d", ptr, size);
#endif
  return ptr;
}

void *malloc_rvx_with_assert(size_t size, const char *file, unsigned int line, const char *func)
{
  void *ptr = malloc_rvx(size);
  if (!ptr)
  {
    assert_must_info(0, file, line, func, "malloc fails (size %u, sram 0x%08x~0x%08x, dram 0x%08x~0x%08x)", size, heap_sram_addr, heap_sram_lastaddr, heap_dram_addr, heap_dram_lastaddr);
  }
  return ptr;
}

void free_rvx(void *ptr)
{
  if (ptr == NULL)
    return;
#ifdef USE_REUSE_MEMORY_ALLOCATOR
  memory_block_info_t *block;
  block = (void *)(((unsigned int)ptr) - MEMORY_BLOCK_INFO_SIZE);
  memory_allocator_push(&PER_CORE(default_allocator), block);
#if 0
    rvx_log_debug("MALLOC", "free %p %d", ptr, block->size);
#endif
#endif
}

void *realloc_rvx(void *ptr, size_t new_size)
{
#ifdef USE_REUSE_MEMORY_ALLOCATOR
  memory_block_info_t *block;
  void *new_ptr;
  size_t old_size;
  if (ptr == NULL)
    return malloc_rvx(new_size);
  new_ptr = malloc_rvx(new_size);
  if (new_ptr == NULL)
    return NULL;
  block = (void *)(((unsigned int)ptr) - MEMORY_BLOCK_INFO_SIZE);
  old_size = block->size - MEMORY_BLOCK_INFO_SIZE;
  memcpy(new_ptr, ptr, (old_size < new_size) ? old_size : new_size);
  free_rvx(ptr);
  return new_ptr;
#else
  assert_must_msg(0, "realloc requires USE_REUSE_MEMORY_ALLOCATOR");
  return NULL;
#endif
}

void print_heap_status()
{
  printf_must("\nheap_sram_addr: 0x%x", heap_sram_addr);
  printf_must("\nheap_sram_lastaddr: 0x%x", heap_sram_lastaddr);
  printf_must("\nheap_dram_addr: 0x%x", heap_dram_addr);
  printf_must("\nheap_dram_lastaddr: 0x%x", heap_dram_lastaddr);
}

void *calloc_rvx(size_t elt_count, size_t elt_size)
{
  if ((elt_size != 0) && (elt_count > (SIZE_MAX / elt_size)))
    return NULL;
  size_t num = elt_count * elt_size;
  void *result = malloc_rvx(num);
  if (result != NULL)
    result = memset(result, 0, num);
  return result;
}

int has_memory_leak()
{
  int diff = 0;
#ifdef USE_REUSE_MEMORY_ALLOCATOR
#if defined(USE_LARGE_RAM)
  unsigned int current = heap_dram_addr;
#else
  unsigned int current = heap_sram_addr;
#endif
  static int init = 0;
  static unsigned int previous = 0;

  if (init < 2)
  {
    init++;
    previous = current;
  }
  else
  {
    diff = current - previous;
    if (diff == 0)
    {
      rvx_log_info("MALLOC", "No Memory Leak");
    }
    else
    {
      rvx_log_warning("MALLOC", "Memory Leak");
      debug_printx(previous);
      debug_printx(current);
      debug_printx(diff);
      previous = current;
    }
  }
#else
  rvx_log_warning("MALLOC", "Enables 'USE_REUSE_MEMORY_ALLOCATOR' for testing memory leak");
#endif
  return diff;
}