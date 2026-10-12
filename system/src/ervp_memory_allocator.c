#include "platform_info.h"

#include "ervp_printf.h"
#include "ervp_memory_allocator.h"
#include "ervp_multicore_synch.h"
#include "ervp_assert.h"
#include "ervp_variable_allocation.h"

memory_block_info_t *memory_allocator_pop(memory_allocator_t *allocator, size_t size)
{
  memory_block_head_info_t *head;
  memory_block_info_t *block;
  assert(allocator != NULL);
  HASH_FIND(hh, *allocator, &size, sizeof(size_t), head);
  block = NULL;
  if (head != NULL)
  {
    block = head->next;
    if (block != NULL) // if not first dummy element
      LL_DELETE(head, block);
  }
  return block;
}

void memory_allocator_push(memory_allocator_t *allocator, memory_block_info_t *block)
{
#if 0
  void *ptr = (void *)(((unsigned int)block) + MEMORY_BLOCK_INFO_SIZE);
  debug_printx((unsigned int)ptr);
#endif
  memory_block_head_info_t *head;
  assert(allocator != NULL);
  assert(block);
  HASH_FIND(hh, *allocator, &(block->size), sizeof(size_t), head);
  if (head == NULL)
  {
    // make first element dummy
    // head = _alloc_new_memory_space(sizeof(memory_block_head_info_t));
    head = ppalloc(sizeof(memory_block_head_info_t));
    head->size = block->size;
    head->next = NULL;
    HASH_ADD(hh, *allocator, size, sizeof(size_t), head);
  }
  LL_APPEND_ELEM(head, head, block);
}
