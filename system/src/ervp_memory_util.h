#ifndef __ERVP_MEMORY_UTIL_H__
#define __ERVP_MEMORY_UTIL_H__

#include <stddef.h>
#include <stdint.h>
#include <string.h>

void print_memory_byte(uint8_t* array, int num);
void print_memory_4byte(uint32_t* array, int num);
int memory_equal(const void* result, const void* ref, size_t num, int prints);

#endif
