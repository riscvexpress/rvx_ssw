#ifndef __ERVP_MATRIX_OP_H__
#define __ERVP_MATRIX_OP_H__

#include "ervp_printf.h"
#include "ervp_malloc.h"
#include "ervp_matrix.h"
#include "ervp_matrix_element.h"
#include "ervp_hwtask.h"

typedef uint8_t ervp_mop_option_value_t;

typedef union __attribute__((packed))
{
  ervp_mop_option_value_t value;
  struct __attribute__((packed))
  {
    unsigned int rshift : 6;
    unsigned int performs_cliping : 1;
    unsigned int acc : 1;
  } br;
} ervp_mop_option_t;

static const unsigned int PADMODE_NONE = 0;
static const unsigned int PADMODE_ZEROS = 1;
static const unsigned int PADMODE_REPLICATE = 2;

typedef uint16_t ervp_mpad_option_value_t;

typedef union
{
  ervp_mpad_option_value_t value;
  struct __attribute__((packed))
  {
    int num_rowu : 3;
    int num_rowd : 3;
    int num_colu : 3;
    int num_cold : 3;
    unsigned int mode : 4;
  } br;
} ervp_mpad_option_t;

typedef uint32_t ervp_mconv_option_value_t;

typedef union
{
  ervp_mconv_option_value_t value;
  struct __attribute__((packed))
  {
    ervp_mop_option_t mop_option;
    ervp_mpad_option_t pad_option;
    uint8_t stride_m1;
    // unsigned int boundary_to_mult : 1;
    // unsigned int pretty_to_mult : 8; // number of col in left matrix
  } br;
} ervp_mconv_option_t;

static const unsigned int DOWNSAMPLE_NONE = 0;
static const unsigned int DOWNSAMPLE_TOPLEFT = 1;
static const unsigned int DOWNSAMPLE_MAX = 2;
static const unsigned int DOWNSAMPLE_AVERAGE = 3;
static const unsigned int DOWNSAMPLE_SUM = 4;

typedef uint32_t ervp_mdownsample_option_value_t;

typedef union
{
  ervp_mdownsample_option_value_t value;
  struct __attribute__((packed))
  {
    uint8_t stride_m1;
    uint8_t downsample_mode;
    ervp_mpad_option_t pad_option;
  } br;
} ervp_mdownsample_option_t;

typedef struct _ervp_mop_mapping ervp_mop_mapping_t;

_Static_assert(sizeof(ervp_mop_option_t) == sizeof(ervp_mop_option_value_t), "ervp_mop_option_t must be 1 byte");
_Static_assert(sizeof(ervp_mpad_option_t) == sizeof(ervp_mpad_option_value_t), "ervp_mpad_option_t must be 2 bytes");
_Static_assert(sizeof(ervp_mconv_option_t) == sizeof(ervp_mconv_option_value_t), "ervp_mconv_option_t must be 4 bytes");
_Static_assert(sizeof(ervp_mdownsample_option_t) == sizeof(ervp_mdownsample_option_value_t), "ervp_mdownsample_option_t must be 4 bytes");

static const unsigned int COMPARE_LT = 1;
static const unsigned int COMPARE_EQ = 2;
static const unsigned int COMPARE_GT = 4;

static const unsigned int COMPARE_LE = COMPARE_LT | COMPARE_EQ;
static const unsigned int COMPARE_NE = COMPARE_LT | COMPARE_GT;
static const unsigned int COMPARE_GE = COMPARE_GT | COMPARE_EQ;

typedef ervp_hwtask_busy_fx_t (*ervp_matrix_copy_part_fx_t)(
    ervp_mop_mapping_t *mop_mapping,
    const ErvpMatrixInfo *a,
    ErvpMatrixInfo *c,
    int num_row,
    int num_col,
    unsigned int option_value);

typedef ervp_hwtask_busy_fx_t (*ervp_matrix_transpose_part_fx_t)(
    ervp_mop_mapping_t *mop_mapping,
    const ErvpMatrixInfo *a,
    ErvpMatrixInfo *c,
    int num_row,
    int num_col,
    unsigned int option_value);

typedef ervp_hwtask_busy_fx_t (*ervp_matrix_unary_fx_t)(
    ervp_mop_mapping_t *mop_mapping,
    const ErvpMatrixInfo *a,
    ErvpMatrixInfo *c,
    unsigned int option_value);

typedef ervp_hwtask_busy_fx_t (*ervp_matrix_binarypp_fx_t)(
    ervp_mop_mapping_t *mop_mapping,
    const ErvpMatrixInfo *a,
    const ErvpMatrixInfo *b,
    ErvpMatrixInfo *c,
    unsigned int option_value);

typedef ervp_hwtask_busy_fx_t (*ervp_matrix_conv_fx_t)(
    ervp_mop_mapping_t *mop_mapping,
    const ErvpMatrixInfo *a,
    const ErvpMatrixInfo *b,
    ErvpMatrixInfo *c,
    unsigned int conv_option_value);

typedef ervp_hwtask_busy_fx_t (*ervp_matrix_conv_sharedinput_fx_t)(
    ervp_mop_mapping_t *mop_mapping,
    int num_output,
    const ErvpMatrixInfo *input_info,
    const ErvpMatrixInfo **kernel_info_list,
    ErvpMatrixInfo **output_info_list,
    unsigned int conv_option_value);

typedef ervp_hwtask_busy_fx_t (*ervp_matrix_conv_sharedoutput_fx_t)(
    ervp_mop_mapping_t *mop_mapping,
    int num_input,
    const ErvpMatrixInfo **input_info_list,
    const ErvpMatrixInfo **kernel_info_list,
    ErvpMatrixInfo *output_info,
    unsigned int conv_option_value);

typedef ervp_hwtask_busy_fx_t (*ervp_matrix_downsample_fx_t)(
    ervp_mop_mapping_t *mop_mapping,
    const ErvpMatrixInfo *input_info,
    ErvpMatrixInfo *output_info,
    unsigned int downsample_option_value);

typedef ervp_hwtask_busy_fx_t (*ervp_matrix_shift_fixed_fx_t)(
    ervp_mop_mapping_t *mop_mapping,
    const ErvpMatrixInfo *a,
    int shamount,
    ErvpMatrixInfo *c,
    unsigned int option_value);

typedef ervp_hwtask_busy_fx_t (*ervp_matrix_pad_fx_t)(
    ervp_mop_mapping_t *mop_mapping,
    const ErvpMatrixInfo *a,
    ErvpMatrixInfo *c,
    unsigned int pad_option_value);

typedef ervp_hwtask_busy_fx_t (*ervp_matrix_fill_fixed_fx_t)(
    ervp_mop_mapping_t *mop_mapping,
    ErvpMatrixInfo *result,
    int32_t value);

typedef ervp_hwtask_busy_fx_t (*ervp_matrix_fill_float_fx_t)(
    ervp_mop_mapping_t *mop_mapping,
    ErvpMatrixInfo *result,
    float value);

typedef ervp_hwtask_busy_fx_t (*ervp_matrix_fill_special_fx_t)(
    ervp_mop_mapping_t *mop_mapping,
    ErvpMatrixInfo *result);

typedef ervp_hwtask_busy_fx_t (*ervp_matrix_binary_fx_t)(
    ervp_mop_mapping_t *mop_mapping,
    const ErvpMatrixInfo *a,
    const ErvpMatrixInfo *b,
    ErvpMatrixInfo *c);

typedef ervp_hwtask_busy_fx_t (*ervp_matrix_compare_fx_t)(
    ervp_mop_mapping_t *mop_mapping,
    const ErvpMatrixInfo *a,
    const ErvpMatrixInfo *b,
    ErvpMatrixInfo *c,
    unsigned int compare_mode);

typedef struct _ervp_mop_mapping
{
  ervp_matrix_copy_part_fx_t matrix_copy_part;
  ervp_matrix_transpose_part_fx_t matrix_transpose_part;
  ervp_matrix_unary_fx_t matrix_reshape;

  ervp_matrix_binarypp_fx_t matrix_add;
  ervp_matrix_binarypp_fx_t matrix_sub;
  ervp_matrix_binarypp_fx_t matrix_ewmult;
  ervp_matrix_binarypp_fx_t matrix_mult;
  ervp_matrix_conv_fx_t matrix_conv;
  ervp_matrix_conv_sharedinput_fx_t matrix_conv_sharedinput;
  ervp_matrix_conv_sharedoutput_fx_t matrix_conv_sharedoutput;
  ervp_matrix_downsample_fx_t matrix_downsample;

  ervp_matrix_shift_fixed_fx_t matrix_shift_fixed;
  ervp_matrix_pad_fx_t matrix_pad;

  ervp_matrix_fill_fixed_fx_t matrix_fill_fixed;
  ervp_matrix_fill_float_fx_t matrix_fill_float;
  ervp_matrix_fill_special_fx_t matrix_identity;

  ervp_matrix_unary_fx_t matrix_copy;
  ervp_matrix_unary_fx_t matrix_transpose;
  ervp_matrix_fill_special_fx_t matrix_zero;
  ervp_matrix_fill_special_fx_t matrix_one;

  ervp_matrix_binary_fx_t matrix_max;
  ervp_matrix_binary_fx_t matrix_min;
  ervp_matrix_binary_fx_t matrix_asl;
  ervp_matrix_binary_fx_t matrix_asr;

  ervp_matrix_compare_fx_t matrix_compare;

} ervp_mop_mapping_t;

// ervp_mop_option_t
int _melement_perform_rshift_and_clip(int value, int rshift, int performs_cliping, ervp_matrix_datatype_t datatype);

static inline ervp_mop_option_t mop_option_set(ervp_mop_option_value_t value)
{
  ervp_mop_option_t mop_option;
  mop_option.value = value;
  return mop_option;
}

static inline int _mop_option_check(const ErvpMatrixInfo *minfo, ervp_mop_option_t mop_option)
{
  int option_is_okay = 1;
  if (matrix_datatype_is_float(minfo->datatype))
  {
    if (mop_option.br.rshift != 0)
      option_is_okay = 0;
    else if (mop_option.br.performs_cliping != 0)
      option_is_okay = 0;
  }
  return option_is_okay;
}

static inline int mop_option_has_postprocess(ervp_mop_option_t mop_option)
{
  return (mop_option.br.performs_cliping | mop_option.br.rshift) != 0;
}

static inline int mop_option_is_acc(ervp_mop_option_t mop_option)
{
  return mop_option.br.acc;
}

static inline ervp_mop_option_t mop_option_acc_only()
{
  ervp_mop_option_t mop_option;
  mop_option.value = 0;
  mop_option.br.acc = 1;
  return mop_option;
}

// ervp_mpad_option_t
static inline ervp_mpad_option_t mpad_option_set(int pad_amount, unsigned int pad_mode)
{
  ervp_mpad_option_t pad_option;
  pad_option.value = 0;
  // num_* are 3-bit signed fields
  assert((pad_amount >= 0) && (pad_amount <= 3));
  if (pad_mode == PADMODE_NONE)
    pad_amount = 0;
  pad_option.br.num_rowd = pad_amount;
  pad_option.br.num_rowu = pad_amount;
  pad_option.br.num_cold = pad_amount;
  pad_option.br.num_colu = pad_amount;
  pad_option.br.mode = pad_mode;
  return pad_option;
}

static inline int mpad_option_has_pad(ervp_mpad_option_t pad_option)
{
  int result = (pad_option.br.mode != PADMODE_NONE);
  if (result == 0)
  {
    assert((!pad_option.br.num_rowd && !pad_option.br.num_rowu && !pad_option.br.num_cold && !pad_option.br.num_colu));
  }
  return result;
}

// ervp_mconv_option_t
static inline ervp_mconv_option_t mconv_option_set(ervp_mconv_option_value_t value)
{
  ervp_mconv_option_t conv_option;
  conv_option.value = value;
  return conv_option;
}

static inline ervp_mconv_option_t matrix_conv_set_pad(ervp_mconv_option_t conv_option, int pad_amount, int pad_mode)
{
  ervp_mconv_option_t conv_option2;
  conv_option2.value = conv_option.value;
  conv_option2.br.pad_option = mpad_option_set(pad_amount, pad_mode);
  return conv_option2;
}

static inline ervp_mpad_option_t mconv_option_get_pad_option(ervp_mconv_option_t conv_option)
{
  return conv_option.br.pad_option;
}

static inline int mconv_option_has_postprocess(ervp_mconv_option_t conv_option)
{
  return mop_option_has_postprocess(conv_option.br.mop_option) || (conv_option.br.stride_m1 > 0);
}

static inline int matrix_conv_has_pad(ervp_mconv_option_t conv_option)
{
  return mpad_option_has_pad(conv_option.br.pad_option);
}

static inline int matrix_conv_output_rows(int input_row, int kernel_row, ervp_mconv_option_t conv_option)
{
  ervp_mpad_option_t pad_option = mconv_option_get_pad_option(conv_option);
  int output_rows = ((input_row + pad_option.br.num_rowd + pad_option.br.num_rowu - kernel_row) / (conv_option.br.stride_m1 + 1)) + 1;
  assert(output_rows > 0);
  return output_rows;
}

static inline int matrix_conv_output_cols(int input_col, int kernel_col, ervp_mconv_option_t conv_option)
{
  ervp_mpad_option_t pad_option = mconv_option_get_pad_option(conv_option);
  int output_cols = ((input_col + pad_option.br.num_cold + pad_option.br.num_colu - kernel_col) / (conv_option.br.stride_m1 + 1)) + 1;
  assert(output_cols > 0);
  return output_cols;
}

/*
static inline int matrix_conv_input_rows(int output_row, int kernel_row, unsigned int conv_option_value)
{
  ervp_mconv_option_t conv_option;
  conv_option.value = conv_option_value;
  return ((output_row - 1) * (conv_option.br.stride_m1 + 1)) + kernel_row;
}

static inline int matrix_conv_input_cols(int output_col, int kernel_col, unsigned int conv_option_value)
{
  ervp_mconv_option_t conv_option;
  conv_option.value = conv_option_value;
  return ((output_col - 1) * (conv_option.br.stride_m1 + 1)) + kernel_col;
}
*/

ErvpMatrixInfo *matrix_conv_alloc_output(const ErvpMatrixInfo *input_info, const ErvpMatrixInfo *kernel_info, unsigned int conv_option_value);
int matrix_conv_check_size(const ErvpMatrixInfo *input_info, const ErvpMatrixInfo *kernel_info, const ErvpMatrixInfo *output_info, unsigned int conv_option_value);

static inline int _matrix_conv_cal_start_row_index_of_input_matrix(int kernel_size, int output_row_index, ervp_mconv_option_t conv_option)
{
  int input_index;
  input_index = output_row_index * (conv_option.br.stride_m1 + 1);
  input_index -= mconv_option_get_pad_option(conv_option).br.num_rowd;
  return input_index;
}

static inline int _matrix_conv_cal_start_col_index_of_input_matrix(int kernel_size, int output_col_index, ervp_mconv_option_t conv_option)
{
  int input_index;
  input_index = output_col_index * (conv_option.br.stride_m1 + 1);
  input_index -= mconv_option_get_pad_option(conv_option).br.num_cold;
  return input_index;
}

// ervp_mpad_option_t
unsigned int matrix_pad_gen_option(unsigned int conv_option_value);

static inline int matrix_pad_output_rows(int input_row, unsigned int pad_option_value)
{
  ervp_mpad_option_t pad_option;
  pad_option.value = pad_option_value;
  return input_row + pad_option.br.num_rowd + pad_option.br.num_rowu;
}

static inline int matrix_pad_output_cols(int input_col, unsigned int pad_option_value)
{
  ervp_mpad_option_t pad_option;
  pad_option.value = pad_option_value;
  return input_col + pad_option.br.num_cold + pad_option.br.num_colu;
}

ErvpMatrixInfo *matrix_pad_alloc_output(const ErvpMatrixInfo *input_info, unsigned int pad_option_value);
int matrix_pad_check_size(const ErvpMatrixInfo *input_info, const ErvpMatrixInfo *output_info, unsigned int pad_option_value);

// ervp_mop_mapping_t
ervp_mop_mapping_t *matrix_op_mapping_alloc();
static inline void matrix_op_mapping_free(ervp_mop_mapping_t *mapping)
{
  free(mapping);
}

int matrix_downsample_check_size(const ErvpMatrixInfo *input_info, const ErvpMatrixInfo *output_info, unsigned int downsample_option_value);

static inline int _matrix_downsample_cal_start_row_index_of_input_matrix(int output_row_index, ervp_mdownsample_option_t downsample_option)
{
  int input_index;
  input_index = output_row_index * (downsample_option.br.stride_m1 + 1);
  input_index -= downsample_option.br.pad_option.br.num_rowd;
  return input_index;
}

static inline int _matrix_downsample_cal_start_col_index_of_input_matrix(int output_col_index, ervp_mdownsample_option_t downsample_option)
{
  int input_index;
  input_index = output_col_index * (downsample_option.br.stride_m1 + 1);
  input_index -= downsample_option.br.pad_option.br.num_cold;
  return input_index;
}

#endif
