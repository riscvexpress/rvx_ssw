#ifndef __ERVP_BLOCKED_MATRIX_OP_H__
#define __ERVP_BLOCKED_MATRIX_OP_H__

#include "ervp_malloc.h"
#include "ervp_matrix_op.h"
#include "ervp_special_matrix_op.h"

typedef struct
{
  int block_size;
  ervp_mop_mapping_t *subop_mapping;
} ervp_blocked_matrix_info_t;

static inline void blocked_matrix_info_init(ervp_blocked_matrix_info_t *info)
{
  assert(info);
  info->block_size = 0;
  info->subop_mapping = NULL;
}

static inline ervp_blocked_matrix_info_t *blocked_matrix_info_alloc()
{
  ervp_blocked_matrix_info_t *info = malloc(sizeof(ervp_blocked_matrix_info_t));
  blocked_matrix_info_init(info);
  return info;
}

static inline void blocked_matrix_info_free(ervp_blocked_matrix_info_t *blocked_info)
{
  free(blocked_info);
}

static const int SUBBLOCK_BY_SW = 0;
static const int SUBBLOCK_BY_HW = 1;
static const int SUBBLOCK_BY_HW_ZEROPAD = 2;

// option_value is passed to subblock_binary_fx as is, so it does not have to be a mop option
ervp_hwtask_busy_fx_t blocked_matrix_op_nopost(int block_size, void *subblock_binary_fx, ervp_mop_mapping_t *subop_mapping, const ErvpMatrixInfo *a, const ErvpMatrixInfo *b, ErvpMatrixInfo *c, unsigned int option_value, int subblock_mode);
ervp_hwtask_busy_fx_t blocked_matrix_op_mode(int block_size, void *subblock_binary_fx, ervp_mop_mapping_t *subop_mapping, const ErvpMatrixInfo *a, const ErvpMatrixInfo *b, ErvpMatrixInfo *c, unsigned int option_value, int subblock_mode);

static inline ervp_hwtask_busy_fx_t blocked_matrix_op(int block_size, void *subblock_binary_fx, ervp_mop_mapping_t *subop_mapping, const ErvpMatrixInfo *a, const ErvpMatrixInfo *b, ErvpMatrixInfo *c, unsigned int option_value)
{
  return blocked_matrix_op_mode(block_size, subblock_binary_fx, subop_mapping, a, b, c, option_value, SUBBLOCK_BY_HW);
}

ervp_hwtask_busy_fx_t blocked_matrix_mult_mode(ervp_blocked_matrix_info_t *blocked_info, const ErvpMatrixInfo *a, const ErvpMatrixInfo *b, ErvpMatrixInfo *c, unsigned int option_value, int subblock_mode);

static inline ervp_hwtask_busy_fx_t blocked_matrix_mult(ervp_blocked_matrix_info_t *blocked_info, const ErvpMatrixInfo *a, const ErvpMatrixInfo *b, ErvpMatrixInfo *c, unsigned int option_value)
{
  return blocked_matrix_mult_mode(blocked_info, a, b, c, option_value, SUBBLOCK_BY_HW);
}

ervp_hwtask_busy_fx_t blocked_matrix_conv_mode(ervp_blocked_matrix_info_t *blocked_info, const ErvpMatrixInfo *input_info, const ErvpMatrixInfo *kernel_info, ErvpMatrixInfo *output_info, unsigned int conv_option_value, int subblock_mode);

static inline ervp_hwtask_busy_fx_t blocked_matrix_conv_prepad(ervp_blocked_matrix_info_t *blocked_info, const ErvpMatrixInfo *input_info, const ErvpMatrixInfo *kernel_info, ErvpMatrixInfo *output_info, unsigned int conv_option_value)
{
  return blocked_matrix_conv_mode(blocked_info, input_info, kernel_info, output_info, conv_option_value, SUBBLOCK_BY_HW);
}

// the zero pad is sent to the subop with each block, and the other pads are applied as blocked_matrix_conv_prepad
static inline ervp_hwtask_busy_fx_t blocked_matrix_conv_hwzeropad(ervp_blocked_matrix_info_t *blocked_info, const ErvpMatrixInfo *input_info, const ErvpMatrixInfo *kernel_info, ErvpMatrixInfo *output_info, unsigned int conv_option_value)
{
  return blocked_matrix_conv_mode(blocked_info, input_info, kernel_info, output_info, conv_option_value, SUBBLOCK_BY_HW_ZEROPAD);
}

// legacy
ervp_hwtask_busy_fx_t blocked_matrix_conv_sharedinput_mode(ervp_blocked_matrix_info_t *blocked_info, int num_output, const ErvpMatrixInfo *input_info, const ErvpMatrixInfo **kernel_info_list, ErvpMatrixInfo **output_info_list, unsigned int conv_option_value, int subblock_mode);

static inline ervp_hwtask_busy_fx_t blocked_matrix_conv_sharedinput_prepad(ervp_blocked_matrix_info_t *blocked_info, int num_output, const ErvpMatrixInfo *input_info, const ErvpMatrixInfo **kernel_info_list, ErvpMatrixInfo **output_info_list, unsigned int conv_option_value)
{
  return blocked_matrix_conv_sharedinput_mode(blocked_info, num_output, input_info, kernel_info_list, output_info_list, conv_option_value, SUBBLOCK_BY_HW);
}

static inline ervp_hwtask_busy_fx_t blocked_matrix_conv_sharedinput_hwzeropad(ervp_blocked_matrix_info_t *blocked_info, int num_output, const ErvpMatrixInfo *input_info, const ErvpMatrixInfo **kernel_info_list, ErvpMatrixInfo **output_info_list, unsigned int conv_option_value)
{
  return blocked_matrix_conv_sharedinput_mode(blocked_info, num_output, input_info, kernel_info_list, output_info_list, conv_option_value, SUBBLOCK_BY_HW_ZEROPAD);
}

ervp_hwtask_busy_fx_t blocked_matrix_conv_sharedoutput_mode(ervp_blocked_matrix_info_t *blocked_info, int num_input, const ErvpMatrixInfo **input_info_list, const ErvpMatrixInfo **kernel_info_list, ErvpMatrixInfo *output_info, unsigned int conv_option_value, int subblock_mode);

static inline ervp_hwtask_busy_fx_t blocked_matrix_conv_sharedoutput_prepad(ervp_blocked_matrix_info_t *blocked_info, int num_input, const ErvpMatrixInfo **input_info_list, const ErvpMatrixInfo **kernel_info_list, ErvpMatrixInfo *output_info, unsigned int conv_option_value)
{
  return blocked_matrix_conv_sharedoutput_mode(blocked_info, num_input, input_info_list, kernel_info_list, output_info, conv_option_value, SUBBLOCK_BY_HW);
}

static inline ervp_hwtask_busy_fx_t blocked_matrix_conv_sharedoutput_hwzeropad(ervp_blocked_matrix_info_t *blocked_info, int num_input, const ErvpMatrixInfo **input_info_list, const ErvpMatrixInfo **kernel_info_list, ErvpMatrixInfo *output_info, unsigned int conv_option_value)
{
  return blocked_matrix_conv_sharedoutput_mode(blocked_info, num_input, input_info_list, kernel_info_list, output_info, conv_option_value, SUBBLOCK_BY_HW_ZEROPAD);
}

ervp_hwtask_busy_fx_t blocked_matrix_conv(ervp_blocked_matrix_info_t *blocked_info, const ErvpMatrixInfo *input_info, const ErvpMatrixInfo *kernel_info, ErvpMatrixInfo *output_info, unsigned int conv_option_value);
ervp_hwtask_busy_fx_t blocked_matrix_conv_sharedinput(ervp_blocked_matrix_info_t *blocked_info, int num_output, const ErvpMatrixInfo *input_info, const ErvpMatrixInfo **kernel_info_list, ErvpMatrixInfo **output_info_list, unsigned int conv_option_value);
ervp_hwtask_busy_fx_t blocked_matrix_conv_sharedoutput(ervp_blocked_matrix_info_t *blocked_info, int num_input, const ErvpMatrixInfo **input_info_list, const ErvpMatrixInfo **kernel_info_list, ErvpMatrixInfo *output_info, unsigned int conv_option_value);

#endif
