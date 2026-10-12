#ifndef __ERVP_DMA_H__
#define __ERVP_DMA_H__

#include "ervp_mmiox1.h"
#include "ervp_matrix.h"

typedef ervp_mmiox1_hwinfo_t ervp_dma_hwinfo_t;

typedef struct
{
  unsigned int src_addr;
  unsigned int src_stride;
  unsigned int dst_addr;
  unsigned int dst_stride;
  unsigned int line_size;
  unsigned int num_lines;
  unsigned int period_us : 31;
  unsigned int periodic : 1;
} dma_inst_t;

static inline void dma_inst_init(dma_inst_t *inst)
{
  int *p = (int *)inst;
  int size = (sizeof(dma_inst_t) + sizeof(int) - 1) / sizeof(int);
  for (int i = 0; i < size; i++)
    p[i] = 0;
}

static inline void dma_inst_set_matrix(dma_inst_t *inst, const ErvpMatrixInfo *src, const ErvpMatrixInfo *dst)
{
  dma_inst_init(inst);
  inst->src_addr = (unsigned int)src->addr;
  inst->src_stride = matrix_get_stride(src);
  inst->dst_addr = (unsigned int)dst->addr;
  inst->dst_stride = matrix_get_stride(dst);
  inst->line_size = (src->num_row) << matrix_datatype_get_addr_lsa(src->datatype);
  inst->num_lines = src->num_col;
}

static inline void dma_inst_push(const ervp_mmiox1_hwinfo_t *const hwinfo, const dma_inst_t *inst, int enable_itr)
{
  mmiox1_inst_push(hwinfo, inst, 1, enable_itr);
}

#define dma_status_read mmiox1_core_status_read
#define dma_inst_num_vacant mmiox1_inst_num_vacant
#define dma_inst_num_busy mmiox1_inst_num_busy
#define dma_is_busy mmiox1_inst_is_busy
#define dma_wait_busy mmiox1_inst_wait_busy
#define dma_clear_request mmiox1_core_clear_request
#define dma_is_clear_finished mmiox1_core_is_clear_finished
#define dma_clear mmiox1_core_clear

#endif
