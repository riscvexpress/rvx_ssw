#include <limits.h>
#include "ervp_assert.h"
#include "ervp_matrix_op.h"
#include "ervp_smart_flush.h"

static inline void _matrix_downsample_float_each_sw(const ErvpMatrixInfo *input_info, ErvpMatrixInfo *output_info, int o_row_index, int o_col_index, ervp_mdownsample_option_t downsample_option)
{
  float result = 0;

  const int row_index_dd = _matrix_downsample_cal_start_row_index_of_input_matrix(o_row_index, downsample_option);
  const int col_index_dd = _matrix_downsample_cal_start_col_index_of_input_matrix(o_col_index, downsample_option);
  const int stride = downsample_option.br.stride_m1 + 1;
  const ervp_mpad_option_t pad_option = downsample_option.br.pad_option;

  switch(downsample_option.br.downsample_mode)
  {
    case DOWNSAMPLE_TOPLEFT:
      {
        result = matrix_read_float_element(input_info, row_index_dd, col_index_dd);
        matrix_write_float_element(output_info, o_row_index, o_col_index, result);
      }
      break;
    case DOWNSAMPLE_MAX:
      {
        result = INT32_MIN;
        for(int k_row_index = 0; k_row_index < stride; k_row_index++)
        {
          for(int k_col_index = 0; k_col_index < stride; k_col_index++)
          {
            int is_pad = 0;
            int i_row_index = k_row_index + row_index_dd;
            int i_col_index = k_col_index + col_index_dd;

            if(i_row_index < 0)
            {
              assert(pad_option.br.num_rowd);
              assert(i_row_index >= (0 - pad_option.br.num_rowd));
              is_pad = 1;
            }
            else if(i_row_index >= input_info->num_row)
            {
              assert(pad_option.br.num_rowu);
              assert(i_row_index < (input_info->num_row + pad_option.br.num_rowu));
              is_pad = 1;
            }

            if(i_col_index < 0)
            {
              assert(pad_option.br.num_cold);
              assert(i_col_index >= (0 - pad_option.br.num_cold));
              is_pad = 1;
            }
            else if(i_col_index >= input_info->num_col)
            {
              assert(pad_option.br.num_colu);
              assert(i_col_index < (input_info->num_col + pad_option.br.num_colu));
              is_pad = 1;
            }

            if(!is_pad)
            {
              float temp = matrix_read_float_element(input_info, i_row_index, i_col_index);
              if(temp > result)
                result = temp;
            }
          }
        }
        matrix_write_float_element(output_info, o_row_index, o_col_index, result);
      }
      break;
    case DOWNSAMPLE_AVERAGE:
    case DOWNSAMPLE_SUM:
      {
        for(int k_row_index = 0; k_row_index < stride; k_row_index++)
        {
          for(int k_col_index = 0; k_col_index < stride; k_col_index++)
          {
            int is_pad = 0;
            int i_row_index = k_row_index + row_index_dd;
            int i_col_index = k_col_index + col_index_dd;

            if(i_row_index < 0)
            {
              assert(pad_option.br.num_rowd);
              assert(i_row_index >= (0 - pad_option.br.num_rowd));
              i_row_index = 0;
              is_pad = 1;
            }
            else if(i_row_index >= input_info->num_row)
            {
              assert(pad_option.br.num_rowu);
              assert(i_row_index < (input_info->num_row + pad_option.br.num_rowu));
              i_row_index = input_info->num_row - 1;
              is_pad = 1;
            }

            if(i_col_index < 0)
            {
              assert(pad_option.br.num_cold);
              assert(i_col_index >= (0 - pad_option.br.num_cold));
              i_col_index = 0;
              is_pad = 1;
            }
            else if(i_col_index >= input_info->num_col)
            {
              assert(pad_option.br.num_colu);
              assert(i_col_index < (input_info->num_col + pad_option.br.num_colu));
              i_col_index = input_info->num_col - 1;
              is_pad = 1;
            }

            float i_value = matrix_read_float_element(input_info, i_row_index, i_col_index);
            result += i_value;
          }
        }
        if(downsample_option.br.downsample_mode==DOWNSAMPLE_AVERAGE)
          result = result / (stride*stride);
        matrix_write_float_element(output_info, o_row_index, o_col_index, result);
      }
      break;
    default:
      assert(0);
  }
}

ervp_hwtask_busy_fx_t _matrix_downsample_float_sw(ervp_mop_mapping_t *mop_mapping, const ErvpMatrixInfo *input_info, ErvpMatrixInfo *output_info, unsigned int downsample_option_value)
{
  // printf_function();
  ervp_mdownsample_option_t downsample_option;
  downsample_option.value = downsample_option_value;
  for(int o_row_index = 0; o_row_index < output_info->num_row; o_row_index++)
  {
    for(int o_col_index = 0; o_col_index < output_info->num_col; o_col_index++)
    {
      _matrix_downsample_float_each_sw(input_info, output_info, o_row_index, o_col_index, downsample_option);
    }
  }
  switch(downsample_option.br.downsample_mode)
  {
    case DOWNSAMPLE_TOPLEFT:
    case DOWNSAMPLE_MAX:
      output_info->is_binary = input_info->is_binary;
      break;
  }
  //
  trackedvar_add(input_info->addr, 0);
  trackedvar_add(output_info->addr, 1);
  return HWTASK_BUSY_FX_NULL;
}

