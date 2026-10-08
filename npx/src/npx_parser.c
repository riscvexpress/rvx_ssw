#include "ervp_printf.h"
#include "ervp_malloc.h"
#include "ervp_assert.h"
#include "ervp_memory_util.h"
#include "ervp_matrix.h"
#include "ervp_round_int.h"
#include "ervp_stdlib.h"

#include <math.h>
#include <string.h>
// #include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include "npx_parser.h"
#include "npx_layer.h"
#include "npx_network.h"
#include "npx_tensor.h"

static void parse_global_cfg(texpar_list_t *option_list, npx_network_t *net);
static int is_global(const texpar_section_t *s);
static void parse_conv2d(npx_layer_compute_t *layer_compute, texpar_list_t *option_list, const char *operator);
static void parse_avgpool2d(npx_layer_compute_t *layer_compute, texpar_list_t *option_list, const char *operator);
static void parse_leaky(npx_layer_compute_t *layer_compute, texpar_list_t *option_list, const char *operator);
static void parse_flatten(npx_layer_compute_t *layer_compute, texpar_list_t *option_list, const char *operator);
static void parse_linear(npx_layer_compute_t *layer_compute, texpar_list_t *option_list, const char *operator);
static void parse_maxpool2d(npx_layer_compute_t *layer_compute, texpar_list_t *option_list, const char *operator);
static void parse_shortcut(npx_layer_compute_t *layer_compute, texpar_list_t *option_list, const char *operator);
static void parse_pool2d(npx_layer_compute_t *layer_compute, texpar_list_t *option_list, const char *operator, npx_layer_type_t layer_type);

static npx_network_t *npx_network_malloc(int num_layer);

static const int NUM_NON_LAYER = 1;

npx_network_t *npx_parse_network_cfg(const char *net_fname, const char *opt_fname)
{
  texpar_list_t *section_list = texpar_read_file_with_section(net_fname);
  texpar_list_t *opt_section_list = texpar_read_file_with_section(opt_fname);
  npx_network_t *net = npx_network_malloc(section_list->size - NUM_NON_LAYER);

  /* network info parsing */
  assert(section_list->front);
  texpar_section_t *global_section = (texpar_section_t *)section_list->front->val;
  assert(is_global(global_section));
  parse_global_cfg(global_section->option_list, net);
  // texpar_unused(train_option_list);

  /* layers info parsing */
  texpar_iter_t *net_iter = section_list->front;
  texpar_iter_t *opt_iter = opt_section_list->front;
  for (int i = 0; i < NUM_NON_LAYER; i++)
    net_iter = net_iter->next;

  int layer_index = 0;
  // texpar_free_section(net_section);
  while (net_iter)
  {
    const texpar_section_t *net_section = (texpar_section_t *)net_iter->val;
    texpar_list_t *option_list = net_section->option_list;
    const texpar_section_t *opt_section = (texpar_section_t *)opt_iter->val;
    assert(strcmp(net_section->type, opt_section->type) == 0);

    char *operator = texpar_find_str(opt_section->option_list, "operator", "cpu");
    // printf("\nlayer %02d: %s --> %s", layer_index, net_section->type, operator);

    if (strcmp(net_section->type, "[Conv2d]") == 0)
    {
      parse_conv2d(net->layer_compute_seq[layer_index], option_list, operator);
    }
    else if (strcmp(net_section->type, "[Linear]") == 0)
    {
      parse_linear(net->layer_compute_seq[layer_index], option_list, operator);
    }
    else if (strcmp(net_section->type, "[Leaky]") == 0)
    {
      parse_leaky(net->layer_compute_seq[layer_index], option_list, operator);
    }
    else if (strcmp(net_section->type, "[MaxPool2d]") == 0)
    {
      parse_maxpool2d(net->layer_compute_seq[layer_index], option_list, operator);
    }
    else if (strcmp(net_section->type, "[AvgPool2d]") == 0)
    {
      parse_avgpool2d(net->layer_compute_seq[layer_index], option_list, operator);
    }
    else if (strcmp(net_section->type, "[Flatten]") == 0)
    {
      parse_flatten(net->layer_compute_seq[layer_index], option_list, operator);
    }
    else if (strcmp(net_section->type, "[Shortcut]") == 0)
    {
      parse_shortcut(net->layer_compute_seq[layer_index], option_list, operator);
    }
    else
      assert(0);

    // texpar_free_section(net_section);
    // texpar_free_section(opt_section);
    net_iter = net_iter->next;
    opt_iter = opt_iter->next;
    layer_index++;
  }
  // free_list(section_list);
  // free_list(opt_section_list);

  for (int i = 0; i < net->num_layer; i++)
  {
    if (net->layer_compute_seq[i]->layer_type != NPXL_SHORTCUT)
      continue;
    npx_shortcut_layer_t *shortcut_layer = (npx_shortcut_layer_t *)(net->layer_compute_seq[i]->layer);
    const int source_index = i + shortcut_layer->skip_from;
    assert(source_index >= 0);
    shortcut_layer->shortcut_input = &(net->layer_compute_seq[source_index]->output_tsseq);
  }

  printf("\nnumber of layers: %d\n", net->num_layer);

  return net;
}

static npx_network_t *npx_network_malloc(int num_layer)
{
  npx_network_t *net = (npx_network_t *)calloc(1, sizeof(npx_network_t));
  net->num_layer = num_layer;
  net->layer_compute_seq = npx_layer_compute_seq_alloc(num_layer);
  for (int i = 0; i < net->num_layer; i++)
    net->layer_compute_seq[i] = npx_layer_compute_alloc();
  return net;
}

// atoi stops at the first non-digit, so the option string is read without being modified
static void set_array_from_str(const char *size_str, int *size_array, int num)
{
  if (size_str[0] == '(')
    size_str++;
  for (int i = 0; i < num; i++)
  {
    assert(*size_str);
    size_array[num - 1 - i] = atoi(size_str);
    while (*size_str && (*size_str != ','))
      size_str++;
    if (*size_str == ',')
      size_str++;
  }
}

static void parse_global_cfg(texpar_list_t *option_list, npx_network_t *net)
{
  char *input_size_str = texpar_find_str(option_list, "input_size", "28,28");
  int input_channels = texpar_find_int_quiet(option_list, "input_channels", 1);
  int classes = texpar_find_int_quiet(option_list, "output_classes", 1);

  int size[2];
  set_array_from_str(input_size_str, size, 2);
  net->h = size[0];
  net->w = size[1];
  net->c = input_channels;
  net->classes = classes;

  printf("\nh,w,c : %d,%d,%d", net->h, net->w, net->c);
  printf("\noutput_classes: %d", net->classes);
}

static int is_global(const texpar_section_t *section)
{
  return (strcmp(section->type, "[global]") == 0);
}

static void parse_iodata2d(npx_layer2d_iodata_t *iodata, texpar_list_t *option_list)
{
  iodata->in_channels = texpar_find_int_quiet(option_list, "in_channels", 0);
  iodata->out_channels = texpar_find_int_quiet(option_list, "out_channels", 0);
  char *input_size_str = texpar_find_str(option_list, "in_size", "");
  char *output_size_str = texpar_find_str(option_list, "out_size", "");
  set_array_from_str(input_size_str, iodata->in_size, 2);
  set_array_from_str(output_size_str, iodata->out_size, 2);
  iodata->in_is_quantized = texpar_find_int_quiet(option_list, "in_is_quantized", -1);
  iodata->out_is_quantized = texpar_find_int_quiet(option_list, "out_is_quantized", -1);
  iodata->in_is_binary = texpar_find_int_quiet(option_list, "in_is_binary", -1);
  iodata->out_is_binary = texpar_find_int_quiet(option_list, "out_is_binary", -1);
  iodata->out_maxvalue = texpar_find_int_quiet(option_list, "out_maxvalue", -1);

  assert(iodata->in_channels > 0);
  assert(iodata->out_channels > 0);
  assert(iodata->in_size[0] > 0);
  assert(iodata->in_size[1] > 0);
  assert(iodata->out_size[0] > 0);
  assert(iodata->out_size[1] > 0);
  assert(iodata->in_is_quantized >= 0);
  assert(iodata->out_is_quantized >= 0);
  assert(iodata->out_maxvalue >= 0);

  if (iodata->out_is_quantized)
  {
    if (iodata->out_maxvalue <= INT8_MAX)
      iodata->out_datatype = MATRIX_DATATYPE_SINT08;
    else if (iodata->out_maxvalue <= SHRT_MAX)
      iodata->out_datatype = MATRIX_DATATYPE_SINT16;
    else
      iodata->out_datatype = MATRIX_DATATYPE_SINT32;
  }
  else
    iodata->out_datatype = MATRIX_DATATYPE_FLOAT32;
}

static int get_weight_datatype(texpar_list_t *option_list)
{
  const char *neuron_type = texpar_find_str(option_list, "neuron_type", "");
  assert_msg(neuron_type[0] == 'w', "%s", neuron_type);
  if (neuron_type[1] == 'f')
    return MATRIX_DATATYPE_FLOAT32;
  int weight_is_signed = (neuron_type[1] == 's');
  assert_msg(weight_is_signed || (neuron_type[1] == 'u'), "%s", neuron_type);
  int weight_bitwidth = atoi(neuron_type + 2);
  assert_msg(weight_bitwidth > 0, "%s", neuron_type);
  if (weight_bitwidth <= 8)
    return weight_is_signed ? MATRIX_DATATYPE_SINT08 : MATRIX_DATATYPE_UINT08;
  assert_msg(weight_is_signed, "%s", neuron_type);
  if (weight_bitwidth <= 16)
    return MATRIX_DATATYPE_SINT16;
  assert_msg(weight_bitwidth <= 32, "%s", neuron_type);
  return MATRIX_DATATYPE_SINT32;
}

static void _transpose_matrix_info(ErvpMatrixInfo **src_info_list, ErvpMatrixInfo **dst_info_list, int row, int col)
{
  for (int i = 0; i < row; i++)
    for (int j = 0; j < col; j++)
      dst_info_list[j * row + i] = src_info_list[i * col + j];
}

static void parse_conv2d(npx_layer_compute_t *layer_compute, texpar_list_t *option_list, const char *operator)
{
  npx_conv2d_layer_t *conv2d_layer = (npx_conv2d_layer_t *)calloc(1, sizeof(npx_conv2d_layer_t));
  layer_compute->layer_type = NPXL_CONV2D;
  layer_compute->layer = conv2d_layer;
  conv2d_layer->kernel_size = texpar_find_int_quiet(option_list, "kernel_size", 0);
  conv2d_layer->stride = texpar_find_int_quiet(option_list, "stride", 1);

  int pad_amount = texpar_find_int_quiet(option_list, "padding", 0);
  conv2d_layer->pad_options.value = 0;
  conv2d_layer->pad_options.br.num_rowu = pad_amount;
  conv2d_layer->pad_options.br.num_rowd = pad_amount;
  conv2d_layer->pad_options.br.num_colu = pad_amount;
  conv2d_layer->pad_options.br.num_cold = pad_amount;
  if (pad_amount == 0)
    conv2d_layer->pad_options.br.mode = PADMODE_NONE;
  else
    conv2d_layer->pad_options.br.mode = PADMODE_ZEROS;

  conv2d_layer->groups = texpar_find_int_quiet(option_list, "groups", 1);

  assert(conv2d_layer->kernel_size > 0);
  assert(conv2d_layer->stride > 0);
  assert(conv2d_layer->stride < 16);
  assert(conv2d_layer->groups > 0);

  parse_iodata2d(&(conv2d_layer->iodata), option_list);

  assert((conv2d_layer->iodata.in_channels % conv2d_layer->groups) == 0);
  assert((conv2d_layer->iodata.out_channels % conv2d_layer->groups) == 0);
  conv2d_layer->in_channels_per_group = conv2d_layer->iodata.in_channels / conv2d_layer->groups;
  conv2d_layer->out_channels_per_group = conv2d_layer->iodata.out_channels / conv2d_layer->groups;

  const int num_weight_matrix = conv2d_layer->iodata.out_channels * conv2d_layer->in_channels_per_group;

  // weight
  conv2d_layer->weight_tensor = npx_tensor_alloc_wo_data(4);
  npx_tensor_set_size(conv2d_layer->weight_tensor, 0, conv2d_layer->kernel_size);
  npx_tensor_set_size(conv2d_layer->weight_tensor, 1, conv2d_layer->kernel_size);
  npx_tensor_set_size(conv2d_layer->weight_tensor, 2, conv2d_layer->in_channels_per_group);
  npx_tensor_set_size(conv2d_layer->weight_tensor, 3, conv2d_layer->iodata.out_channels);
  npx_tensor_set_datatype(conv2d_layer->weight_tensor, get_weight_datatype(option_list));
  npx_tensor_alloc_data(conv2d_layer->weight_tensor);

  conv2d_layer->weight_matrix_info_list_for_output_reuse = npx_tensor_generate_matrix_info_list(conv2d_layer->weight_tensor, 1, num_weight_matrix);
  conv2d_layer->weight_matrix_info_list_for_input_reuse = npx_tensor_generate_matrix_info_list(conv2d_layer->weight_tensor, 1, num_weight_matrix);
  // memory leak
  for (int g = 0; g < conv2d_layer->groups; g++)
  {
    const int offset = g * conv2d_layer->out_channels_per_group * conv2d_layer->in_channels_per_group;
    _transpose_matrix_info(&(conv2d_layer->weight_matrix_info_list_for_output_reuse[offset]), &(conv2d_layer->weight_matrix_info_list_for_input_reuse[offset]), conv2d_layer->out_channels_per_group, conv2d_layer->in_channels_per_group);
  }

  // operator
  layer_compute->operator = operator;
  if (strcmp(layer_compute->operator, "dca") == 0)
    layer_compute->forward = npx_forward_conv2d_layer_reuse;
  else
    layer_compute->forward = npx_forward_conv2d_layer_default;
}

static void parse_shortcut(npx_layer_compute_t *layer_compute, texpar_list_t *option_list, const char *operator)
{
  npx_shortcut_layer_t *shortcut_layer = (npx_shortcut_layer_t *)calloc(1, sizeof(npx_shortcut_layer_t));
  layer_compute->layer_type = NPXL_SHORTCUT;
  layer_compute->layer = shortcut_layer;

  shortcut_layer->skip_from = texpar_find_int_quiet(option_list, "skip_from", 0);
  assert(shortcut_layer->skip_from < 0);

  const char *shortcut_type = texpar_find_str(option_list, "type", "Identity");
  if (strcmp(shortcut_type, "Identity") == 0)
  {
    shortcut_layer->shortcut_type = NPXL_IDENTITY;
    shortcut_layer->layer = NULL;
    parse_iodata2d(&(shortcut_layer->iodata), option_list);
  }
  else
  {
    npx_layer_compute_t inner_layer_compute;
    memset(&inner_layer_compute, 0, sizeof(npx_layer_compute_t));
    if (strcmp(shortcut_type, "Conv2d") == 0)
      parse_conv2d(&inner_layer_compute, option_list, operator);
    else if (strcmp(shortcut_type, "MaxPool2d") == 0)
      parse_pool2d(&inner_layer_compute, option_list, operator, NPXL_MAXPOOL2D);
    else if (strcmp(shortcut_type, "AvgPool2d") == 0)
      parse_pool2d(&inner_layer_compute, option_list, operator, NPXL_AVGPOOL2D);
    else
      assert(0);
    shortcut_layer->shortcut_type = inner_layer_compute.layer_type;
    shortcut_layer->layer = inner_layer_compute.layer;
    assert(shortcut_layer->layer);
    switch (shortcut_layer->shortcut_type)
    {
    case NPXL_CONV2D:
      shortcut_layer->iodata = ((npx_conv2d_layer_t *)(shortcut_layer->layer))->iodata;
      break;
    case NPXL_MAXPOOL2D:
    case NPXL_AVGPOOL2D:
      shortcut_layer->iodata = ((npx_pool2d_layer_t *)(shortcut_layer->layer))->iodata;
      break;
    default:
      assert(0);
    }
  }

  layer_compute->operator = operator;
  layer_compute->forward = npx_forward_shortcut_layer_default;
}

static void parse_pool2d(npx_layer_compute_t *layer_compute, texpar_list_t *option_list, const char *operator, npx_layer_type_t layer_type)
{
  npx_pool2d_layer_t *pool2d_layer = (npx_pool2d_layer_t *)calloc(1, sizeof(npx_pool2d_layer_t));
  layer_compute->layer_type = layer_type;
  layer_compute->layer = pool2d_layer;
  pool2d_layer->kernel_size = texpar_find_int_quiet(option_list, "kernel_size", 0);
  pool2d_layer->stride = texpar_find_int_quiet(option_list, "stride", 1);

  const int pad_amount = texpar_find_int_quiet(option_list, "padding", 0);
  pool2d_layer->pad_options.value = 0;
  pool2d_layer->pad_options.br.num_rowu = pad_amount;
  pool2d_layer->pad_options.br.num_rowd = pad_amount;
  pool2d_layer->pad_options.br.num_colu = pad_amount;
  pool2d_layer->pad_options.br.num_cold = pad_amount;
  if (pad_amount == 0)
    pool2d_layer->pad_options.br.mode = PADMODE_NONE;
  else
    pool2d_layer->pad_options.br.mode = PADMODE_ZEROS;

  assert(pool2d_layer->kernel_size > 0);
  assert(pool2d_layer->stride > 0);
  assert(pool2d_layer->stride < 16);
  assert(pool2d_layer->kernel_size == pool2d_layer->stride);

  parse_iodata2d(&(pool2d_layer->iodata), option_list);

  layer_compute->operator = operator;
  switch (layer_type)
  {
  case NPXL_MAXPOOL2D:
    layer_compute->forward = npx_forward_maxpool2d_layer_default;
    break;
  case NPXL_AVGPOOL2D:
    layer_compute->forward = npx_forward_avgpool2d_layer_default;
    break;
  case NPXL_SUMPOOL2D:
    layer_compute->forward = npx_forward_sumpool2d_layer_default;
    break;
  default:
    assert(0);
  }
}

static void parse_avgpool2d(npx_layer_compute_t *layer_compute, texpar_list_t *option_list, const char *operator)
{
  parse_pool2d(layer_compute, option_list, operator, NPXL_SUMPOOL2D);
}

static void parse_leaky(npx_layer_compute_t *layer_compute, texpar_list_t *option_list, const char *operator)
{
  npx_leaky_layer_t *leaky_layer = (npx_leaky_layer_t *)calloc(1, sizeof(npx_leaky_layer_t));
  layer_compute->layer_type = NPXL_LEAKY;
  layer_compute->layer = leaky_layer;

  leaky_layer->reset_mechanism = texpar_find_str(option_list, "reset_mechanism", "");
  leaky_layer->reset_delay = texpar_find_str(option_list, "reset_delay", "");

  //
  leaky_layer->is_hard_reset = (strcmp(leaky_layer->reset_mechanism, "zero") == 0);
  leaky_layer->is_soft_reset = (strcmp(leaky_layer->reset_mechanism, "subtract") == 0);
  assert(leaky_layer->is_hard_reset || leaky_layer->is_soft_reset);
  leaky_layer->has_reset_delay = (strcmp(leaky_layer->reset_delay, "True") == 0);

  parse_iodata2d(&(leaky_layer->iodata), option_list);

  int num_channel = leaky_layer->iodata.in_channels;
  ervp_matrix_datatype_t datatype = leaky_layer->iodata.in_is_quantized ? MATRIX_DATATYPE_SINT32 : MATRIX_DATATYPE_FLOAT32;
  leaky_layer->membrane_potential = (ErvpMatrixInfo **)calloc(num_channel, sizeof(ErvpMatrixInfo *));
#if 1
  leaky_layer->membrane_potential_total = matrix_alloc(datatype, num_channel * leaky_layer->iodata.in_size[1], leaky_layer->iodata.in_size[0]);
  for (int i = 0; i < num_channel; i++)
  {
    leaky_layer->membrane_potential[i] = matrix_generate_submatrix_info(leaky_layer->membrane_potential_total);
    leaky_layer->membrane_potential[i]->addr = matrix_get_row_addr(leaky_layer->membrane_potential_total, i * leaky_layer->iodata.in_size[1]);
    leaky_layer->membrane_potential[i]->num_row = leaky_layer->iodata.in_size[1];
  }
#else
  for (int i = 0; i < num_channel; i++)
    leaky_layer->membrane_potential[i] = matrix_alloc(datatype, leaky_layer->iodata.in_size[1], leaky_layer->iodata.in_size[0]);
#endif
  leaky_layer->membrane_potential_scaled = 1;

  layer_compute->operator = operator;
  layer_compute->forward = npx_forward_leaky_layer_default;
}

static void parse_flatten(npx_layer_compute_t *layer_compute, texpar_list_t *option_list, const char *operator)
{
  npx_flatten_layer_t *flatten_layer = (npx_flatten_layer_t *)calloc(1, sizeof(npx_flatten_layer_t));
  layer_compute->layer_type = NPXL_FLATTEN;
  layer_compute->layer = flatten_layer;

  parse_iodata2d(&(flatten_layer->iodata), option_list);

  layer_compute->operator = operator;
  layer_compute->forward = npx_forward_flatten_layer_default;
}

static void parse_linear(npx_layer_compute_t *layer_compute, texpar_list_t *option_list, const char *operator)
{
  npx_linear_layer_t *linear_layer = (npx_linear_layer_t *)calloc(1, sizeof(npx_linear_layer_t));
  layer_compute->layer_type = NPXL_LINEAR;
  layer_compute->layer = linear_layer;
  linear_layer->in_features = texpar_find_int_quiet(option_list, "in_features", 0);
  linear_layer->out_features = texpar_find_int_quiet(option_list, "out_features", 0);

  assert(linear_layer->in_features >= 0);
  assert(linear_layer->out_features >= 0);

  parse_iodata2d(&(linear_layer->iodata), option_list);

  // weight
  linear_layer->weight_tensor = npx_tensor_alloc_wo_data(2);
  npx_tensor_set_size(linear_layer->weight_tensor, 0, linear_layer->in_features);
  npx_tensor_set_size(linear_layer->weight_tensor, 1, linear_layer->out_features);
  npx_tensor_set_datatype(linear_layer->weight_tensor, get_weight_datatype(option_list));
  npx_tensor_alloc_data(linear_layer->weight_tensor);

  linear_layer->transposed_weight_matrix = matrix_alloc(linear_layer->weight_tensor->datatype, linear_layer->in_features, linear_layer->out_features);

  layer_compute->operator = operator;
  layer_compute->forward = npx_forward_linear_layer_default;
}

static void parse_maxpool2d(npx_layer_compute_t *layer_compute, texpar_list_t *option_list, const char *operator)
{
  parse_pool2d(layer_compute, option_list, operator, NPXL_MAXPOOL2D);
}