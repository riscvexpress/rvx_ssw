#include "ervp_misc_util.h"
#include "ervp_printf.h"
#include "ervp_malloc.h"
#include "ervp_assert.h"
#include "ervp_math.h"
#include "ervp_matrix_op_sw.h"
#include "ervp_special_matrix_op.h"
#include "ervp_smart_flush.h"

#include <string.h>
#include <stdint.h>
#include <limits.h>

#include "npx_malloc.h"
#include "npx_tensor.h"
#include "npx_layer.h"
#include "npx_network.h"
#include "npx_profiling.h"
#include "npx_spm.h"

char npx_layer_type_to_char(npx_layer_type_t type)
{
  char result = 0;
  switch (type)
  {
  case NPXL_CONV2D:
    result = 'c';
    break;
  case NPXL_LINEAR:
    result = 'f';
    break;
  case NPXL_MAXPOOL2D:
    result = 'm';
    break;
  case NPXL_AVGPOOL2D:
    result = 'a';
    break;
  case NPXL_SUMPOOL2D:
    result = 'u';
    break;
  case NPXL_LEAKY:
    result = 'n';
    break;
  case NPXL_FLATTEN:
    result = '-';
    break;
  case NPXL_SHORTCUT:
    result = 's';
    break;
  case NPXL_BLOCK:
    result = 'b';
    break;
  default:
    assert(0);
  }
  return result;
}

const char *npx_layer_type_to_str(npx_layer_type_t type)
{
  const char *result = NULL;
  switch (type)
  {
  case NPXL_CONV2D:
    result = "NPXL_CONV2D";
    break;
  case NPXL_LINEAR:
    result = "NPXL_LINEAR";
    break;
  case NPXL_MAXPOOL2D:
    result = "NPXL_MAXPOOL2D";
    break;
  case NPXL_AVGPOOL2D:
    result = "NPXL_AVGPOOL2D";
    break;
  case NPXL_SUMPOOL2D:
    result = "NPXL_SUMPOOL2D";
    break;
  case NPXL_LEAKY:
    result = "NPXL_LEAKY";
    break;
  case NPXL_FLATTEN:
    result = "NPXL_FLATTEN";
    break;
  case NPXL_SHORTCUT:
    result = "NPXL_SHORTCUT";
    break;
  case NPXL_IDENTITY:
    result = "NPXL_IDENTITY";
    break;
  case NPXL_BLOCK:
    result = "NPXL_BLOCK";
    break;
  default:
    assert(0);
  }
  return result;
}

int npx_layer_output_size(npx_layer_type_t layer_type, void *layer)
{
  assert(layer);
  int outputs = 0;
  if (layer_type == NPXL_BLOCK)
  {
    npx_layer_block_t *p = (npx_layer_block_t *)layer;
    assert(p->layer_compute_seq);
    npx_layer_compute_t *layer_compute = p->layer_compute_seq[p->num_layer - 1];
    assert(layer_compute);
    assert(layer_compute->layer_type != NPXL_BLOCK);
    outputs = npx_layer_output_size(layer_compute->layer_type, layer_compute->layer);
  }
  else
  {
    npx_layer2d_iodata_t *p = &(((npx_conv2d_layer_t *)layer)->iodata);
    outputs = p->out_size[0] * p->out_size[1] * p->out_channels;
  }
  return outputs;
}

int npx_layer_testvector_size(npx_layer_type_t layer_type, void *layer)
{
  assert(layer);
  int outputs = 0;
  if (layer_type == NPXL_BLOCK)
  {
    npx_layer_block_t *p = (npx_layer_block_t *)layer;
    assert(p->layer_compute_seq);
    for (int i = 0; i < p->num_layer; i++)
    {
      npx_layer_compute_t *layer_compute = p->layer_compute_seq[i];
      assert(layer_compute);
      assert(layer_compute->layer_type != NPXL_BLOCK);
      outputs += npx_layer_output_size(layer_compute->layer_type, layer_compute->layer);
    }
  }
  else
    outputs = npx_layer_output_size(layer_type, layer);
  return outputs;
}

npx_layerio_tsseq_t *npx_layerio_tsseq_alloc(int timesteps)
{
  npx_layerio_tsseq_t *result = malloc(sizeof(npx_layerio_tsseq_t));
  result->timesteps = timesteps;
  result->sequence = (NpxTensorInfo **)malloc(sizeof(NpxTensorInfo *) * timesteps);
  for (int i = 0; i < timesteps; i++)
    result->sequence[i] = NULL;
  result->is_boundary = NULL;
  result->scaled = 0;
  return result;
}

void npx_layerio_tsseq_free(npx_layerio_tsseq_t *p)
{
  assert(p);
  if (p->sequence)
  {
    for (int i = 0; i < p->timesteps; i++)
      if (p->sequence[i])
        npx_tensor_free(p->sequence[i]);
    free(p->sequence);
  }
  free(p);
}

npx_layerio_tsseq_t *npx_output_tsseq_alloc(int timesteps, const int *is_boundary, float scaled, ervp_matrix_datatype_t datatype, int num_dim, npx_tensor_dim_size_t *size_array)
{
  assert(is_boundary);
  assert(size_array);

  npx_layerio_tsseq_t *output_tsseq = npx_layerio_tsseq_alloc(timesteps);
  output_tsseq->scaled = scaled;
  output_tsseq->is_boundary = is_boundary;

  NpxTensorInfo *all_output_tensor = npx_tensor_alloc_wo_data(num_dim + 1);
  assert(all_output_tensor);
  npx_tensor_set_size_array(all_output_tensor, size_array);
  npx_tensor_set_size(all_output_tensor, num_dim, timesteps);
  ErvpMatrixDatatype adjusted_datatype;
  adjusted_datatype.value = datatype;
#ifdef USE_SUBBYTE_NPX
  while (1)
  {
    if (((adjusted_datatype.br.num_bits * size_array[0]) & 7) == 0)
      break;
    adjusted_datatype.br.num_bits <<= 1;
    adjusted_datatype.br.addr_lsa++;
  }
#endif
  npx_tensor_set_datatype(all_output_tensor, adjusted_datatype.value);
  npx_tensor_set_contiguous_layout(all_output_tensor);
  // const int tensor_size = ALIGN_UP_POW2(npx_tensor_sizes(all_output_tensor), CACHE_LINE_SIZE);
  all_output_tensor->num_dim = num_dim;
  const int tensor_size = npx_tensor_sizes(all_output_tensor);
  all_output_tensor->addr = npx_malloc_prefer_spm(timesteps * tensor_size);
  all_output_tensor->array_needs_free = 1;

  for (int i = 0; i < timesteps; i++)
  {
    NpxTensorInfo *output_tensor = npx_tensor_generate_subtensor_info(all_output_tensor);
    npx_tensor_set_contiguous_layout(output_tensor);
    assert(output_tensor);
    output_tensor->addr = all_output_tensor->addr + (i * tensor_size);
    output_tsseq->sequence[i] = output_tensor;
  }
  all_output_tensor->num_dim = num_dim + 1;
  npx_tensor_free(all_output_tensor);
  return output_tsseq;
}

__attribute__((weak)) void npx_forward_flatten_layer_default(void *layer_ptr, ervp_mop_mapping_t *mop_mapping, npx_layerio_state_t *state)
{
  printf_function();
  NPX_PROFILING_START();
  assert(state->output_tsseq == NULL);

  npx_flatten_layer_t *layer = (npx_flatten_layer_t *)layer_ptr;
  const int timesteps = state->input_tsseq->timesteps;
  const NpxTensorInfo *const input_tensor = state->input_tsseq->sequence[0];

  int no_copy = 0;
  if (matrix_datatype_is_subbyte(input_tensor->datatype))
    no_copy = 0;
  else if (!npx_tensor_has_contiguous_layout(npx_tensor_get_original_tensor(input_tensor)))
    no_copy = 0;
  else
    no_copy = 1;

  if (no_copy)
  {
    state->output_tsseq = npx_layerio_tsseq_alloc(timesteps);
    state->output_tsseq->scaled = state->input_tsseq->scaled;
    state->output_tsseq->is_boundary = state->input_tsseq->is_boundary;

    int num_elements = npx_tensor_elements(input_tensor);
    for (int i = 0; i < timesteps; i++)
    {
      NpxTensorInfo *input_tensor = state->input_tsseq->sequence[i];
      assert(input_tensor);
      NpxTensorInfo *output_tensor = npx_tensor_generate_subtensor_info(input_tensor);
      assert(output_tensor);
      output_tensor->num_dim = 2;
      npx_tensor_set_size(output_tensor, 0, 1);
      npx_tensor_set_size(output_tensor, 1, num_elements);
      npx_tensor_set_contiguous_layout(output_tensor);
      output_tensor->addr = input_tensor->addr;
      state->output_tsseq->sequence[i] = output_tensor;
    }
  }
  else
  {
    npx_tensor_dim_size_t size_array[2];
    size_array[0] = 1;
    size_array[1] = npx_tensor_elements(input_tensor);
    state->output_tsseq = npx_output_tsseq_alloc(timesteps, state->input_tsseq->is_boundary, state->input_tsseq->scaled, input_tensor->datatype, 2, size_array);

    for (int i = 0; i < timesteps; i++)
    {
      NpxTensorInfo *input_tensor = state->input_tsseq->sequence[i];
      NpxTensorInfo *output_tensor = state->output_tsseq->sequence[i];
      assert(input_tensor);
      assert(output_tensor);
      npx_tensor_reshape(mop_mapping, input_tensor, output_tensor);
    }
  }
  NPX_PROFILING_END();
}

__attribute__((weak)) void npx_forward_maxpool2d_layer_default(void *layer_ptr, ervp_mop_mapping_t *mop_mapping, npx_layerio_state_t *state)
{
  printf_function();
  NPX_PROFILING_START();

  ervp_matrix_downsample_fx_t matrix_downsample_fx;
  if (mop_mapping == NULL)
    matrix_downsample_fx = _matrix_downsample_sw;
  else
    matrix_downsample_fx = mop_mapping->matrix_downsample;

  npx_maxpool2d_layer_t *layer = (npx_maxpool2d_layer_t *)layer_ptr;
  assert_pointer(2, layer, state);
  assert(matrix_downsample_fx != NULL);
  assert(state->output_tsseq == NULL);
  assert(layer->pad_options.br.mode == PADMODE_NONE);

  const int timesteps = state->input_tsseq->timesteps;
  const NpxTensorInfo *const input_tensor = state->input_tsseq->sequence[0];
  assert(layer->iodata.in_size[0] == npx_tensor_get_size(input_tensor, 0));
  assert(layer->iodata.in_size[1] == npx_tensor_get_size(input_tensor, 1));
  assert(layer->iodata.out_size[0] == (((npx_tensor_get_size(input_tensor, 0) + 2 * layer->pad_options.br.num_rowd - layer->kernel_size) / layer->stride) + 1));
  assert(layer->iodata.out_size[1] == (((npx_tensor_get_size(input_tensor, 1) + 2 * layer->pad_options.br.num_rowd - layer->kernel_size) / layer->stride) + 1));

  npx_tensor_dim_size_t size_array[3];
  size_array[0] = layer->iodata.out_size[0];
  size_array[1] = layer->iodata.out_size[1];
  size_array[2] = npx_tensor_get_size(input_tensor, 2);
  state->output_tsseq = npx_output_tsseq_alloc(timesteps, state->input_tsseq->is_boundary, state->input_tsseq->scaled, input_tensor->datatype, 3, size_array);

  ervp_mdownsample_option_t downsample_option;
  downsample_option.value = 0;
  downsample_option.br.stride_m1 = layer->stride - 1;
  downsample_option.br.downsample_mode = DOWNSAMPLE_MAX;

  ervp_hwtask_busy_fx_t hwtask_busy_fx = HWTASK_BUSY_FX_NULL;
  for (int i = 0; i < timesteps; i++)
  {
    NpxTensorInfo *input_tensor3d = state->input_tsseq->sequence[i];
    NpxTensorInfo *output_tensor3d = state->output_tsseq->sequence[i];
    assert(input_tensor3d);
    assert(output_tensor3d);

    if ((i == 0) || (input_tensor3d != state->input_tsseq->sequence[0]))
    {
      ErvpMatrixInfo *input_matrix = NULL;
      ErvpMatrixInfo *output_matrix = NULL;

      for (int j = 0; j < layer->iodata.out_channels; j++)
      {
        input_matrix = npx_tensor_iterate_using_matrix_info(input_tensor3d, 1, input_matrix);
        output_matrix = npx_tensor_iterate_using_matrix_info(output_tensor3d, 1, output_matrix);
        hwtask_busy_fx = matrix_downsample_fx(mop_mapping, input_matrix, output_matrix, downsample_option.value);
      }
      hwtask_wait_complete(hwtask_busy_fx);
      hwtask_busy_fx = HWTASK_BUSY_FX_NULL;
      matrix_free(input_matrix);
      matrix_free(output_matrix);
    }
    else
    {
      npx_tensor_free(output_tensor3d);
      output_tensor3d = npx_tensor_generate_subtensor_info(state->output_tsseq->sequence[0]);
      npx_tensor_set_contiguous_layout(output_tensor3d);
      output_tensor3d->addr = state->output_tsseq->sequence[0]->addr;
      state->output_tsseq->sequence[i] = output_tensor3d;
    }
  }
  hwtask_wait_complete(hwtask_busy_fx);
  NPX_PROFILING_END();
}

__attribute__((weak)) void npx_forward_sumpool2d_layer_default(void *layer_ptr, ervp_mop_mapping_t *mop_mapping, npx_layerio_state_t *state)
{
  printf_function();
  NPX_PROFILING_START();

  ervp_matrix_downsample_fx_t matrix_downsample_fx;
  if (mop_mapping == NULL)
    matrix_downsample_fx = _matrix_downsample_sw;
  else
    matrix_downsample_fx = mop_mapping->matrix_downsample;

  npx_sumpool2d_layer_t *layer = (npx_sumpool2d_layer_t *)layer_ptr;
  assert_pointer(2, layer, state);
  assert(matrix_downsample_fx != NULL);
  assert(state->output_tsseq == NULL);
  assert(layer->pad_options.br.mode == PADMODE_NONE);

  const int timesteps = state->input_tsseq->timesteps;
  const NpxTensorInfo *const input_tensor = state->input_tsseq->sequence[0];
  assert(layer->iodata.in_size[0] == npx_tensor_get_size(input_tensor, 0));
  assert(layer->iodata.in_size[1] == npx_tensor_get_size(input_tensor, 1));
  assert(layer->iodata.out_size[0] == (((npx_tensor_get_size(input_tensor, 0) + 2 * layer->pad_options.br.num_rowd - layer->kernel_size) / layer->stride) + 1));
  assert(layer->iodata.out_size[1] == (((npx_tensor_get_size(input_tensor, 1) + 2 * layer->pad_options.br.num_rowd - layer->kernel_size) / layer->stride) + 1));

  npx_tensor_dim_size_t size_array[3];
  size_array[0] = ((npx_tensor_get_size(input_tensor, 0) + 2 * layer->pad_options.br.num_rowd - layer->kernel_size) / layer->stride) + 1;
  size_array[1] = ((npx_tensor_get_size(input_tensor, 1) + 2 * layer->pad_options.br.num_rowd - layer->kernel_size) / layer->stride) + 1;
  size_array[2] = npx_tensor_get_size(input_tensor, 2);
  state->output_tsseq = npx_output_tsseq_alloc(timesteps, state->input_tsseq->is_boundary, state->input_tsseq->scaled, input_tensor->datatype, 3, size_array);
  state->output_tsseq->scaled *= (layer->kernel_size * layer->kernel_size);

  ervp_mdownsample_option_t downsample_option;
  downsample_option.value = 0;
  downsample_option.br.stride_m1 = layer->stride - 1;
  downsample_option.br.downsample_mode = DOWNSAMPLE_SUM;

  ervp_hwtask_busy_fx_t hwtask_busy_fx = HWTASK_BUSY_FX_NULL;
  for (int i = 0; i < timesteps; i++)
  {
    NpxTensorInfo *input_tensor3d = state->input_tsseq->sequence[i];
    NpxTensorInfo *output_tensor3d = state->output_tsseq->sequence[i];
    assert(input_tensor3d);
    assert(output_tensor3d);

    if ((i == 0) || (input_tensor3d != state->input_tsseq->sequence[0]))
    {
      ErvpMatrixInfo *input_matrix = NULL;
      ErvpMatrixInfo *output_matrix = NULL;

      for (int j = 0; j < layer->iodata.out_channels; j++)
      {
        input_matrix = npx_tensor_iterate_using_matrix_info(input_tensor3d, 1, input_matrix);
        output_matrix = npx_tensor_iterate_using_matrix_info(output_tensor3d, 1, output_matrix);
        hwtask_busy_fx = matrix_downsample_fx(mop_mapping, input_matrix, output_matrix, downsample_option.value);
      }
      hwtask_wait_complete(hwtask_busy_fx);
      hwtask_busy_fx = HWTASK_BUSY_FX_NULL;
      matrix_free(input_matrix);
      matrix_free(output_matrix);
    }
    else
    {
      npx_tensor_free(output_tensor3d);
      output_tensor3d = npx_tensor_generate_subtensor_info(state->output_tsseq->sequence[0]);
      npx_tensor_set_contiguous_layout(output_tensor3d);
      output_tensor3d->addr = state->output_tsseq->sequence[0]->addr;
      state->output_tsseq->sequence[i] = output_tensor3d;
    }
  }
  NPX_PROFILING_END();
}

__attribute__((weak)) void npx_forward_avgpool2d_layer_default(void *layer_ptr, ervp_mop_mapping_t *mop_mapping, npx_layerio_state_t *state)
{
  printf_function();
  NPX_PROFILING_START();

  ervp_matrix_downsample_fx_t matrix_downsample_fx;
  if (mop_mapping == NULL)
    matrix_downsample_fx = _matrix_downsample_sw;
  else
    matrix_downsample_fx = mop_mapping->matrix_downsample;

  npx_avgpool2d_layer_t *layer = (npx_avgpool2d_layer_t *)layer_ptr;
  assert_pointer(2, layer, state);
  assert(matrix_downsample_fx != NULL);
  assert(state->output_tsseq == NULL);
  assert(layer->pad_options.br.mode == PADMODE_NONE);

  const int timesteps = state->input_tsseq->timesteps;
  const NpxTensorInfo *const input_tensor = state->input_tsseq->sequence[0];
  assert(layer->iodata.in_size[0] == npx_tensor_get_size(input_tensor, 0));
  assert(layer->iodata.in_size[1] == npx_tensor_get_size(input_tensor, 1));
  assert(layer->iodata.out_size[0] == (((npx_tensor_get_size(input_tensor, 0) + 2 * layer->pad_options.br.num_rowd - layer->kernel_size) / layer->stride) + 1));
  assert(layer->iodata.out_size[1] == (((npx_tensor_get_size(input_tensor, 1) + 2 * layer->pad_options.br.num_rowd - layer->kernel_size) / layer->stride) + 1));

  npx_tensor_dim_size_t size_array[3];
  size_array[0] = ((npx_tensor_get_size(input_tensor, 0) + 2 * layer->pad_options.br.num_rowd - layer->kernel_size) / layer->stride) + 1;
  size_array[1] = ((npx_tensor_get_size(input_tensor, 1) + 2 * layer->pad_options.br.num_rowd - layer->kernel_size) / layer->stride) + 1;
  size_array[2] = npx_tensor_get_size(input_tensor, 2);
  state->output_tsseq = npx_output_tsseq_alloc(timesteps, state->input_tsseq->is_boundary, state->input_tsseq->scaled, input_tensor->datatype, 3, size_array);

  ervp_mdownsample_option_t downsample_option;
  downsample_option.value = 0;
  downsample_option.br.stride_m1 = layer->stride - 1;
  downsample_option.br.downsample_mode = DOWNSAMPLE_AVERAGE;

  ervp_hwtask_busy_fx_t hwtask_busy_fx = HWTASK_BUSY_FX_NULL;
  for (int i = 0; i < timesteps; i++)
  {
    NpxTensorInfo *input_tensor3d = state->input_tsseq->sequence[i];
    NpxTensorInfo *output_tensor3d = state->output_tsseq->sequence[i];
    assert(input_tensor3d);
    assert(output_tensor3d);

    if ((i == 0) || (input_tensor3d != state->input_tsseq->sequence[0]))
    {
      ErvpMatrixInfo *input_matrix = NULL;
      ErvpMatrixInfo *output_matrix = NULL;

      for (int j = 0; j < layer->iodata.out_channels; j++)
      {
        input_matrix = npx_tensor_iterate_using_matrix_info(input_tensor3d, 1, input_matrix);
        output_matrix = npx_tensor_iterate_using_matrix_info(output_tensor3d, 1, output_matrix);
        hwtask_busy_fx = matrix_downsample_fx(mop_mapping, input_matrix, output_matrix, downsample_option.value);
      }
      hwtask_wait_complete(hwtask_busy_fx);
      hwtask_busy_fx = HWTASK_BUSY_FX_NULL;
      matrix_free(input_matrix);
      matrix_free(output_matrix);
    }
    else
    {
      npx_tensor_free(output_tensor3d);
      output_tensor3d = npx_tensor_generate_subtensor_info(state->output_tsseq->sequence[0]);
      npx_tensor_set_contiguous_layout(output_tensor3d);
      output_tensor3d->addr = state->output_tsseq->sequence[0]->addr;
      state->output_tsseq->sequence[i] = output_tensor3d;
    }
  }
  NPX_PROFILING_END();
}

__attribute__((weak)) void npx_forward_conv2d_layer_default(void *layer_ptr, ervp_mop_mapping_t *mop_mapping, npx_layerio_state_t *state)
{
  printf_function();
  NPX_PROFILING_START();
  // NOT supported
  npx_conv2d_layer_t *layer = (npx_conv2d_layer_t *)layer_ptr;
  if (layer->pad_options.br.num_rowd > 0)
    assert(layer->pad_options.br.mode == PADMODE_ZEROS);
  assert_pointer(2, layer, state);
  assert(state->output_tsseq == NULL);

  const int timesteps = state->input_tsseq->timesteps;
  const NpxTensorInfo *const input_tensor = state->input_tsseq->sequence[0];
  assert(layer->iodata.in_size[0] == npx_tensor_get_size(input_tensor, 0));
  assert(layer->iodata.in_size[1] == npx_tensor_get_size(input_tensor, 1));
  assert(layer->iodata.out_size[0] == (((npx_tensor_get_size(input_tensor, 0) + 2 * layer->pad_options.br.num_rowd - layer->kernel_size) / layer->stride) + 1));
  assert(layer->iodata.out_size[1] == (((npx_tensor_get_size(input_tensor, 1) + 2 * layer->pad_options.br.num_rowd - layer->kernel_size) / layer->stride) + 1));

  npx_tensor_dim_size_t size_array[3];
  size_array[0] = layer->iodata.out_size[0];
  size_array[1] = layer->iodata.out_size[1];
  size_array[2] = layer->iodata.out_channels;
  // ervp_matrix_datatype_t datatype = layer->iodata.out_is_quantized ? MATRIX_DATATYPE_SINT32 : MATRIX_DATATYPE_FLOAT32;
  ervp_matrix_datatype_t datatype = layer->iodata.out_datatype;
  state->output_tsseq = npx_output_tsseq_alloc(timesteps, state->input_tsseq->is_boundary, state->input_tsseq->scaled, datatype, 3, size_array);

  ervp_mconv_option_t conv_option;
  conv_option.value = 0;
  conv_option.br.mop_option.br.acc = 1;
  conv_option = matrix_conv_set_pad(conv_option, layer->pad_options.br.num_rowd, layer->pad_options.br.mode);
  conv_option.br.stride_m1 = layer->stride - 1;

  for (int i = 0; i < timesteps; i++)
  {
    NpxTensorInfo *input_tensor3d = state->input_tsseq->sequence[i];
    NpxTensorInfo *output_tensor3d = state->output_tsseq->sequence[i];
    assert(input_tensor3d);
    assert(output_tensor3d);
    ErvpMatrixInfo *weight_matrix = NULL;
    ErvpMatrixInfo *output_matrix = NULL;
    ErvpMatrixInfo **input_matrix_info_list = npx_tensor_generate_matrix_info_list(input_tensor3d, 1, layer->iodata.in_channels);

    for (int j = 0; j < layer->iodata.out_channels; j++)
    {
      const int group_index = j / layer->out_channels_per_group;
      output_matrix = npx_tensor_iterate_using_matrix_info(output_tensor3d, 1, output_matrix);
      matrix_zero_sw(output_matrix);
      for (int k = 0; k < layer->in_channels_per_group; k++)
      {
        weight_matrix = npx_tensor_iterate_using_matrix_info(layer->weight_tensor, 1, weight_matrix);
        matrix_conv_sw(input_matrix_info_list[(group_index * layer->in_channels_per_group) + k], weight_matrix, output_matrix, conv_option.value);
      }
    }
    matrix_list_free(input_matrix_info_list, layer->iodata.in_channels);
    matrix_free(weight_matrix);
    matrix_free(output_matrix);
  }
  NPX_PROFILING_END();
}

static void npx_forward_linear_layer_old(npx_linear_layer_t *layer, ervp_mop_mapping_t *mop_mapping, npx_layerio_state_t *state)
{
  printf_function();
  NPX_PROFILING_START();

  ervp_matrix_binarypp_fx_t matrix_mult_fx;
  if (mop_mapping == NULL)
    matrix_mult_fx = _matrix_mult_sw;
  else
    matrix_mult_fx = mop_mapping->matrix_mult;

  assert_pointer(2, layer, state);
  assert(matrix_mult_fx != NULL);
  assert(state->input_tsseq != NULL);
  assert(state->output_tsseq == NULL);
  assert(layer->iodata.in_channels == layer->iodata.out_channels);

  const int timesteps = state->input_tsseq->timesteps;
  const NpxTensorInfo *const input_tensor = state->input_tsseq->sequence[0];
  npx_tensor_dim_size_t size_array[3];
  size_array[0] = 1;
  size_array[1] = layer->out_features;
  size_array[2] = layer->iodata.out_channels;
  // ervp_matrix_datatype_t datatype = matrix_datatype_is_float(input_tensor->datatype) ? MATRIX_DATATYPE_FLOAT32 : MATRIX_DATATYPE_SINT32;
  ervp_matrix_datatype_t datatype = layer->iodata.out_datatype;
  int num_dim = input_tensor->num_dim;
  state->output_tsseq = npx_output_tsseq_alloc(timesteps, state->input_tsseq->is_boundary, state->input_tsseq->scaled, datatype, num_dim, size_array);

  ErvpMatrixInfo weight_matrix;
  npx_tensor_to_matrix_info(layer->weight_tensor, &weight_matrix);
  ervp_hwtask_busy_fx_t hwtask_busy_fx = HWTASK_BUSY_FX_NULL;

  for (int i = 0; i < timesteps; i++)
  {
    NpxTensorInfo *input_tensor3d = state->input_tsseq->sequence[i];
    NpxTensorInfo *output_tensor3d = state->output_tsseq->sequence[i];
    assert(input_tensor3d);
    assert(output_tensor3d);

    ErvpMatrixInfo *input_vector = NULL;
    ErvpMatrixInfo *output_vector = NULL;

    for (int j = 0; j < layer->iodata.out_channels; j++)
    {
      input_vector = npx_tensor_iterate_using_matrix_info(input_tensor3d, 1, input_vector);
      output_vector = npx_tensor_iterate_using_matrix_info(output_tensor3d, 1, output_vector);
      hwtask_busy_fx = matrix_mult_fx(mop_mapping, &weight_matrix, input_vector, output_vector, 0);
    }
    hwtask_wait_complete(hwtask_busy_fx);
    hwtask_busy_fx = HWTASK_BUSY_FX_NULL;
    matrix_free(input_vector);
    matrix_free(output_vector);
  }
  NPX_PROFILING_END();
}

__attribute__((weak)) void npx_forward_linear_layer_default(void *layer_ptr, ervp_mop_mapping_t *mop_mapping, npx_layerio_state_t *state)
{
  printf_function();
  NPX_PROFILING_START();

  ervp_matrix_binarypp_fx_t matrix_mult_fx;
  if (mop_mapping == NULL)
    matrix_mult_fx = _matrix_mult_sw;
  else
    matrix_mult_fx = mop_mapping->matrix_mult;

  npx_linear_layer_t *layer = (npx_linear_layer_t *)layer_ptr;
  assert_pointer(2, layer, state);
  assert(matrix_mult_fx != NULL);
  assert(state->input_tsseq != NULL);
  assert(state->output_tsseq == NULL);
  assert(layer->iodata.in_channels == layer->iodata.out_channels);

  const int timesteps = state->input_tsseq->timesteps;
  const NpxTensorInfo *const input_tensor = state->input_tsseq->sequence[0];
  npx_tensor_dim_size_t size_array[3];
  size_array[0] = 1;
  size_array[1] = layer->out_features;
  size_array[2] = layer->iodata.out_channels;
  // ervp_matrix_datatype_t datatype = matrix_datatype_is_float(input_tensor->datatype) ? MATRIX_DATATYPE_FLOAT32 : MATRIX_DATATYPE_SINT32;
  ervp_matrix_datatype_t datatype = layer->iodata.out_datatype;
  int num_dim = input_tensor->num_dim;
  state->output_tsseq = npx_output_tsseq_alloc(timesteps, state->input_tsseq->is_boundary, state->input_tsseq->scaled, datatype, num_dim, size_array);
  assert(npx_tensor_has_contiguous_layout(npx_tensor_get_original_tensor(state->input_tsseq->sequence[0])));
  assert(npx_tensor_has_contiguous_layout(npx_tensor_get_original_tensor(state->output_tsseq->sequence[0])));

  ErvpMatrixInfo input_matrix;
  ErvpMatrixInfo output_matrix;
  ervp_hwtask_busy_fx_t hwtask_busy_fx;

  matrix_init_info(input_tensor->datatype, layer->iodata.out_channels * timesteps, layer->in_features, state->input_tsseq->sequence[0]->addr, &input_matrix);
  matrix_init_info(layer->iodata.out_datatype, layer->iodata.out_channels * timesteps, layer->out_features, state->output_tsseq->sequence[0]->addr, &output_matrix);
  hwtask_busy_fx = matrix_mult_fx(mop_mapping, &input_matrix, layer->transposed_weight_matrix, &output_matrix, 0);
  hwtask_wait_complete(hwtask_busy_fx);
  NPX_PROFILING_END();
}

static ervp_hwtask_busy_fx_t matrix_compare_reset_decay_sw(npx_leaky_layer_t *layer, int mp_scale, ErvpMatrixInfo *mp_matrix, ErvpMatrixInfo *output_matrix)
{
  // NOT supported
  assert(!matrix_datatype_is_float(mp_matrix->datatype));

  int threshold_scaled = layer->threshold * mp_scale;
  for (int i = 0; i < mp_matrix->num_row; i++)
    for (int j = 0; j < mp_matrix->num_col; j++)
    {
      int mp = matrix_read_fixed_element(mp_matrix, i, j);
      int spike = (mp > threshold_scaled); // NOT >=
      matrix_write_fixed_element(output_matrix, i, j, spike);
      int value = 0;
      if (layer->is_hard_reset)
      {
        if (spike)
          value = 0;
        else if (layer->does_decay) // mp * layer->beta
          value = math_div_by_shift(mp * layer->beta_numerator, layer->beta_denominator_rsa);
      }
      else
      {
        value = mp;
        if (spike)
          value -= threshold_scaled;
        if (layer->does_decay) // value * layer->beta
          value = math_div_by_shift(value * layer->beta_numerator, layer->beta_denominator_rsa);
      }
      if (spike || layer->does_decay)
        matrix_write_fixed_element(mp_matrix, i, j, value);
    }
  trackedvar_add(mp_matrix->addr, 1);
  trackedvar_add(output_matrix->addr, 1);
  return HWTASK_BUSY_FX_NULL;
}

typedef ervp_hwtask_busy_fx_t (*matrix_compare_reset_decay_fx_t)(npx_leaky_layer_t *layer, int mp_scale, ErvpMatrixInfo *mp_matrix, ErvpMatrixInfo *output_matrix);
static matrix_compare_reset_decay_fx_t matrix_compare_reset_decay_hw = matrix_compare_reset_decay_sw;

__attribute__((weak)) void npx_forward_leaky_layer_default(void *layer_ptr, ervp_mop_mapping_t *mop_mapping, npx_layerio_state_t *state)
{
  printf_function();
  NPX_PROFILING_START();

  npx_leaky_layer_t *layer = (npx_leaky_layer_t *)layer_ptr;
  const int total_tensor_size = npx_tensor_sizes(npx_tensor_get_original_tensor(state->input_tsseq->sequence[0]));
  const int SMALL_TENSOR_SIZE_THRESHOLD = CACHE_LINE_SIZE * 4;

  ervp_matrix_binarypp_fx_t matrix_ewmult_fx;
  ervp_matrix_binarypp_fx_t matrix_add_fx;
  ervp_matrix_unary_fx_t matrix_copy_fx;
  matrix_compare_reset_decay_fx_t matrix_compare_reset_decay_fx;

  if ((mop_mapping == NULL) || (total_tensor_size <= SMALL_TENSOR_SIZE_THRESHOLD))
  {
    matrix_ewmult_fx = _matrix_ewmult_sw;
    matrix_add_fx = _matrix_add_sw;
    matrix_copy_fx = _matrix_copy_sw;
    matrix_compare_reset_decay_fx = matrix_compare_reset_decay_sw;
  }
  else
  {
    matrix_ewmult_fx = mop_mapping->matrix_ewmult;
    matrix_add_fx = mop_mapping->matrix_add;
    matrix_copy_fx = mop_mapping->matrix_copy;
    matrix_compare_reset_decay_fx = matrix_compare_reset_decay_hw;
  }

  assert_pointer(2, layer, state);
  assert(matrix_ewmult_fx != NULL);
  assert(matrix_add_fx != NULL);
  const int timesteps = state->input_tsseq->timesteps;
  assert(state->input_tsseq);
  assert(state->input_tsseq->is_boundary);
  assert(state->output_tsseq == NULL);
  const NpxTensorInfo *const input_tensor = state->input_tsseq->sequence[0];
#ifdef USE_SUBBYTE_NPX
  state->output_tsseq = npx_output_tsseq_alloc(timesteps, state->input_tsseq->is_boundary, 1, MATRIX_DATATYPE_UINT01, input_tensor->num_dim, npx_tensor_get_size_array(input_tensor));
#else
  state->output_tsseq = npx_output_tsseq_alloc(timesteps, state->input_tsseq->is_boundary, 1, MATRIX_DATATYPE_SINT08, input_tensor->num_dim, npx_tensor_get_size_array(input_tensor));
#endif
  assert(npx_tensor_has_contiguous_layout(npx_tensor_get_original_tensor(state->input_tsseq->sequence[0])));
  assert(npx_tensor_has_contiguous_layout(npx_tensor_get_original_tensor(state->output_tsseq->sequence[0])));

  npx_layerio_tsseq_t *scaled_input_tsseq = NULL;
  ervp_hwtask_busy_fx_t hwtask_busy_fx = HWTASK_BUSY_FX_NULL;
  void *npx_buffer = npx_spm_malloc(matrix_num_bytes(layer->membrane_potential_total));
  ErvpMatrixInfo membrane_potential_total_obj;
  ErvpMatrixInfo *membrane_potential_total;
  int needs_copy = 1;

  if (npx_buffer)
  {
    membrane_potential_total = &membrane_potential_total_obj;
    *membrane_potential_total = *(layer->membrane_potential_total);
    membrane_potential_total->addr = npx_buffer;
  }
  else
  {
    membrane_potential_total = layer->membrane_potential_total;
    needs_copy = 0;
  }

  if (state->input_tsseq->scaled != layer->membrane_potential_scaled)
  {
    assert(state->input_tsseq->scaled >= 1);
    int lcm = math_lcm(state->input_tsseq->scaled, layer->membrane_potential_scaled);
    if (lcm != layer->membrane_potential_scaled)
    {
      ErvpMatrixInfo mp_scalar_factor_info;
      int mp_scalar_factor = lcm / layer->membrane_potential_scaled;
      matrix_init_scalar_info(MATRIX_DATATYPE_SINT32, &mp_scalar_factor, &mp_scalar_factor_info);
      hwtask_busy_fx = matrix_ewmult_fx(mop_mapping, layer->membrane_potential_total, &mp_scalar_factor_info, membrane_potential_total, 0);
      needs_copy = 0;
      layer->membrane_potential_scaled = lcm;
    }
    if (lcm != state->input_tsseq->scaled)
    {
      ErvpMatrixInfo input_scalar_factor_info;
      int input_scalar_factor = lcm / state->input_tsseq->scaled;
      matrix_init_scalar_info(MATRIX_DATATYPE_SINT32, &input_scalar_factor, &input_scalar_factor_info);
      assert((state->input_tsseq->scaled * input_scalar_factor) == lcm);
      scaled_input_tsseq = npx_output_tsseq_alloc(timesteps, state->input_tsseq->is_boundary, 1, MATRIX_DATATYPE_SINT32, input_tensor->num_dim, npx_tensor_get_size_array(input_tensor));
      ErvpMatrixInfo total_input_flatten_matrix;
      npx_tensor_to_flattened_matrix_info(npx_tensor_get_original_tensor(input_tensor), &total_input_flatten_matrix);
      ErvpMatrixInfo total_scaled_input_flatten_matrix;
      npx_tensor_to_flattened_matrix_info(npx_tensor_get_original_tensor(scaled_input_tsseq->sequence[0]), &total_scaled_input_flatten_matrix);
      hwtask_busy_fx = matrix_ewmult_fx(mop_mapping, &total_input_flatten_matrix, &input_scalar_factor_info, &total_scaled_input_flatten_matrix, 0);
    }
  }

  if (needs_copy)
    hwtask_busy_fx = matrix_copy_fx(mop_mapping, layer->membrane_potential_total, membrane_potential_total, 0);

  // npx_tensor_zero(mop_mapping, npx_tensor_get_original_tensor(state->output_tsseq->sequence[0]));

  ErvpMatrixInfo membrane_potential_ch_obj;
  ErvpMatrixInfo *membrane_potential_ch = &membrane_potential_ch_obj;
  *membrane_potential_ch = *membrane_potential_total;
  membrane_potential_ch->num_row = layer->iodata.in_size[1];
  membrane_potential_ch->array_needs_free = 0;
  membrane_potential_ch->is_sub = 1;

  for (int i = 0; i < timesteps; i++)
  {
    NpxTensorInfo *input_tensor3d;
    ErvpMatrixInfo input_flatten_matrix;
    if (scaled_input_tsseq == NULL)
      input_tensor3d = state->input_tsseq->sequence[i];
    else
      input_tensor3d = scaled_input_tsseq->sequence[i];
    assert(input_tensor3d);
    npx_tensor_to_flattened_matrix_info(input_tensor3d, &input_flatten_matrix);

    hwtask_wait_complete(hwtask_busy_fx);
    hwtask_busy_fx = matrix_add_fx(mop_mapping, &input_flatten_matrix, membrane_potential_total, membrane_potential_total, 0);

    if (state->input_tsseq->is_boundary[i])
    {
      NpxTensorInfo *output_tensor3d = state->output_tsseq->sequence[i];
      assert(output_tensor3d);
      output_tensor3d->is_binary = 1;
      ErvpMatrixInfo *output_matrix = NULL;

      for (int j = 0; j < layer->iodata.out_channels; j++)
      {
        output_matrix = npx_tensor_iterate_using_matrix_info(output_tensor3d, 1, output_matrix);
        membrane_potential_ch->addr = matrix_get_row_addr(membrane_potential_total, j * membrane_potential_ch->num_row);
        hwtask_busy_fx = matrix_compare_reset_decay_fx(layer, layer->membrane_potential_scaled, membrane_potential_ch, output_matrix);
      }
      matrix_free(output_matrix);
    }
  }
  hwtask_wait_complete(hwtask_busy_fx);
  if (scaled_input_tsseq)
    npx_layerio_tsseq_free(scaled_input_tsseq);
  if (npx_buffer)
  {
    hwtask_busy_fx = matrix_copy_fx(mop_mapping, membrane_potential_total, layer->membrane_potential_total, 0);
    hwtask_wait_complete(hwtask_busy_fx);
    npx_spm_free(npx_buffer);
  }
  NPX_PROFILING_END();
}

__attribute__((weak)) void npx_forward_shortcut_layer_default(void *layer_ptr, ervp_mop_mapping_t *mop_mapping, npx_layerio_state_t *state)
{
  printf_function();
  NPX_PROFILING_START();
  npx_shortcut_layer_t *layer = (npx_shortcut_layer_t *)layer_ptr;
  assert_pointer(2, layer, state);
  assert(state->output_tsseq == NULL);
  assert_pointer(1, layer->shortcut_input);

  npx_layerio_tsseq_t *skip_tsseq = *(layer->shortcut_input);
  assert_pointer(1, skip_tsseq);
  const int timesteps = state->input_tsseq->timesteps;
  assert(skip_tsseq->timesteps == timesteps);

  npx_layerio_tsseq_t *branch_tsseq = skip_tsseq;
  if (layer->shortcut_type == NPXL_IDENTITY)
    assert(layer->layer == NULL);
  else
  {
    assert(layer->layer);
    npx_layerio_state_t branch_state;
    branch_state.input_tsseq = skip_tsseq;
    branch_state.output_tsseq = NULL;
    switch (layer->shortcut_type)
    {
    case NPXL_CONV2D:
      npx_forward_conv2d_layer_reuse(layer->layer, mop_mapping, &branch_state);
      break;
    case NPXL_MAXPOOL2D:
      npx_forward_maxpool2d_layer_default(layer->layer, mop_mapping, &branch_state);
      break;
    case NPXL_AVGPOOL2D:
      npx_forward_avgpool2d_layer_default(layer->layer, mop_mapping, &branch_state);
      break;
    default:
      assert(0);
    }
    branch_tsseq = branch_state.output_tsseq;
    assert_pointer(1, branch_tsseq);
  }
  assert(branch_tsseq->scaled == skip_tsseq->scaled);

  ervp_matrix_binarypp_fx_t matrix_add_fx;
  if (mop_mapping == NULL)
    matrix_add_fx = _matrix_add_sw;
  else
    matrix_add_fx = mop_mapping->matrix_add;
  assert(matrix_add_fx != NULL);

  npx_tensor_dim_size_t size_array[3];
  size_array[0] = layer->iodata.out_size[0];
  size_array[1] = layer->iodata.out_size[1];
  size_array[2] = layer->iodata.out_channels;
  state->output_tsseq = npx_output_tsseq_alloc(timesteps, state->input_tsseq->is_boundary, state->input_tsseq->scaled, layer->iodata.out_datatype, 3, size_array);

  ervp_hwtask_busy_fx_t hwtask_busy_fx = HWTASK_BUSY_FX_NULL;
  for (int i = 0; i < timesteps; i++)
  {
    NpxTensorInfo *branch_tensor3d = branch_tsseq->sequence[i];
    NpxTensorInfo *input_tensor3d = state->input_tsseq->sequence[i];
    NpxTensorInfo *output_tensor3d = state->output_tsseq->sequence[i];
    assert_pointer(3, branch_tensor3d, input_tensor3d, output_tensor3d);

    ErvpMatrixInfo *branch_matrix = NULL;
    ErvpMatrixInfo *input_matrix = NULL;
    ErvpMatrixInfo *output_matrix = NULL;
    for (int j = 0; j < layer->iodata.out_channels; j++)
    {
      branch_matrix = npx_tensor_iterate_using_matrix_info(branch_tensor3d, 1, branch_matrix);
      input_matrix = npx_tensor_iterate_using_matrix_info(input_tensor3d, 1, input_matrix);
      output_matrix = npx_tensor_iterate_using_matrix_info(output_tensor3d, 1, output_matrix);
      hwtask_wait_complete(hwtask_busy_fx);
      hwtask_busy_fx = matrix_add_fx(mop_mapping, input_matrix, branch_matrix, output_matrix, 0);
    }
    hwtask_wait_complete(hwtask_busy_fx);
    hwtask_busy_fx = HWTASK_BUSY_FX_NULL;
    matrix_free(branch_matrix);
    matrix_free(input_matrix);
    matrix_free(output_matrix);
  }

  if (branch_tsseq != skip_tsseq)
    npx_layerio_tsseq_free(branch_tsseq);
  NPX_PROFILING_END();
}

__attribute__((weak)) void npx_forward_layer_block_default(void *layer_ptr, ervp_mop_mapping_t *mop_mapping, npx_layerio_state_t *state)
{
  npx_layer_block_t *layer = (npx_layer_block_t *)layer_ptr;
  state->output_tsseq = _npx_foward_layer_sequence(layer->layer_compute_seq, state->input_tsseq, 0, layer->num_layer, 0);
}