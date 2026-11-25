#include "ervp_printf.h"
#include "ervp_log.h"
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

static void parse_global_cfg(const cJSON *node, npx_network_t *net);
static void parse_conv2d(npx_layer_compute_t *layer_compute, const cJSON *node, const char *operator);
static void parse_avgpool2d(npx_layer_compute_t *layer_compute, const cJSON *node, const char *operator);
static void parse_leaky(npx_layer_compute_t *layer_compute, const cJSON *node, const char *operator);
static void parse_flatten(npx_layer_compute_t *layer_compute, const cJSON *node, const char *operator);
static void parse_linear(npx_layer_compute_t *layer_compute, const cJSON *node, const char *operator);
static void parse_maxpool2d(npx_layer_compute_t *layer_compute, const cJSON *node, const char *operator);
static void parse_shortcut(npx_layer_compute_t *layer_compute, const cJSON *node, const char *operator);
static void parse_pool2d(npx_layer_compute_t *layer_compute, const cJSON *node, const char *operator, npx_layer_type_t layer_type);

static npx_network_t *npx_network_malloc(int num_layer);

static int find_node_index(const cJSON *node_list, const char *name)
{
  int index = 0;
  const cJSON *node;
  cJSON_ArrayForEach(node, node_list)
  {
    const char *node_name = npx_json_find_str(node, "name", NULL);
    if (node_name && (strcmp(node_name, name) == 0))
      return index;
    index++;
  }
  assert_msg(0, "%s", name);
  return -1;
}

static int find_skip_from(const cJSON *node_list, const cJSON *node, int layer_index)
{
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(node, "skip_from");
  if (item == NULL)
    return 0;
  if (cJSON_IsString(item))
    return find_node_index(node_list, item->valuestring) - layer_index;
  assert_msg(cJSON_IsNumber(item), "skip_from");
  return item->valueint;
}

npx_network_t *npx_parse_network_cfg(const char *net_fname, const char *opt_fname)
{
  cJSON *net_root = npx_json_read_file(net_fname);
  cJSON *opt_root = npx_json_read_file(opt_fname);

  const cJSON *global = cJSON_GetObjectItemCaseSensitive(net_root, "global");
  const cJSON *net_node_list = cJSON_GetObjectItemCaseSensitive(net_root, "nodes");
  const cJSON *opt_node_list = cJSON_GetObjectItemCaseSensitive(opt_root, "nodes");
  assert_msg(cJSON_IsObject(global), "%s", net_fname);
  assert_msg(cJSON_IsArray(net_node_list), "%s", net_fname);
  assert_msg(cJSON_IsArray(opt_node_list), "%s", opt_fname);
  assert_msg(cJSON_GetArraySize(net_node_list) == cJSON_GetArraySize(opt_node_list), "%s %s", net_fname, opt_fname);

  npx_network_t *net = npx_network_malloc(cJSON_GetArraySize(net_node_list));
  parse_global_cfg(global, net);

  int layer_index = 0;
  const cJSON *node;
  const cJSON *opt_node = opt_node_list->child;
  cJSON_ArrayForEach(node, net_node_list)
  {
    const char *type = npx_json_find_str(node, "type", "");
    assert_msg(strcmp(type, npx_json_find_str(opt_node, "type", "")) == 0, "layer %d: %s", layer_index, type);
    const char *operator = npx_json_copy_str(npx_json_find_str(opt_node, "operator", "cpu"));
    npx_layer_compute_t *layer_compute = net->layer_compute_seq[layer_index];

    if (strcmp(type, "Conv2d") == 0)
      parse_conv2d(layer_compute, node, operator);
    else if (strcmp(type, "Linear") == 0)
      parse_linear(layer_compute, node, operator);
    else if (strcmp(type, "Leaky") == 0)
      parse_leaky(layer_compute, node, operator);
    else if (strcmp(type, "MaxPool2d") == 0)
      parse_maxpool2d(layer_compute, node, operator);
    else if (strcmp(type, "AvgPool2d") == 0)
      parse_avgpool2d(layer_compute, node, operator);
    else if (strcmp(type, "Flatten") == 0)
      parse_flatten(layer_compute, node, operator);
    else if (strcmp(type, "Shortcut") == 0)
    {
      parse_shortcut(layer_compute, node, operator);
      ((npx_shortcut_layer_t *)(layer_compute->layer))->skip_from = find_skip_from(net_node_list, node, layer_index);
      assert(((npx_shortcut_layer_t *)(layer_compute->layer))->skip_from < 0);
    }
    else
      assert_msg(0, "%s", type);

    opt_node = opt_node->next;
    layer_index++;
  }
  cJSON_Delete(net_root);
  cJSON_Delete(opt_root);

  for (int i = 0; i < net->num_layer; i++)
  {
    if (net->layer_compute_seq[i]->layer_type != NPXL_SHORTCUT)
      continue;
    npx_shortcut_layer_t *shortcut_layer = (npx_shortcut_layer_t *)(net->layer_compute_seq[i]->layer);
    const int source_index = i + shortcut_layer->skip_from;
    assert(source_index >= 0);
    shortcut_layer->shortcut_input = &(net->layer_compute_seq[source_index]->output_tsseq);
  }

  rvx_log_info("NPX", "number of layers: %d\n", net->num_layer);

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

static void parse_global_cfg(const cJSON *node, npx_network_t *net)
{
  int size[2];
  npx_json_find_size(node, "input_size", size, 2);
  net->h = size[0];
  net->w = size[1];
  net->c = npx_json_find_int(node, "input_channels", 1);
  net->classes = npx_json_find_int(node, "output_classes", 1);

  rvx_log_info("NPX", "h,w,c: %d,%d,%d", net->h, net->w, net->c);
  rvx_log_info("NPX", "output_classes: %d", net->classes);
}

static void parse_iodata2d(npx_layer2d_iodata_t *iodata, const cJSON *node)
{
  iodata->in_channels = npx_json_find_int(node, "in_channels", 0);
  iodata->out_channels = npx_json_find_int(node, "out_channels", 0);
  npx_json_find_size(node, "in_size", iodata->in_size, 2);
  npx_json_find_size(node, "out_size", iodata->out_size, 2);
  iodata->in_is_quantized = npx_json_find_int(node, "in_is_quantized", -1);
  iodata->out_is_quantized = npx_json_find_int(node, "out_is_quantized", -1);
  iodata->in_is_binary = npx_json_find_int(node, "in_is_binary", -1);
  iodata->out_is_binary = npx_json_find_int(node, "out_is_binary", -1);
  iodata->out_maxvalue = npx_json_find_int(node, "out_maxvalue", -1);

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

static int get_weight_datatype(const cJSON *node)
{
  const char *neuron_type = npx_json_find_str(node, "neuron_type", "");
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

static void parse_conv2d(npx_layer_compute_t *layer_compute, const cJSON *node, const char *operator)
{
  npx_conv2d_layer_t *conv2d_layer = (npx_conv2d_layer_t *)calloc(1, sizeof(npx_conv2d_layer_t));
  layer_compute->layer_type = NPXL_CONV2D;
  layer_compute->layer = conv2d_layer;
  conv2d_layer->kernel_size = npx_json_find_int(node, "kernel_size", 0);
  conv2d_layer->stride = npx_json_find_int(node, "stride", 1);

  int pad_amount = npx_json_find_int(node, "padding", 0);
  conv2d_layer->pad_options.value = 0;
  conv2d_layer->pad_options.br.num_bottom = pad_amount;
  conv2d_layer->pad_options.br.num_top = pad_amount;
  conv2d_layer->pad_options.br.num_right = pad_amount;
  conv2d_layer->pad_options.br.num_left = pad_amount;
  if (pad_amount == 0)
    conv2d_layer->pad_options.br.mode = PADMODE_NONE;
  else
    conv2d_layer->pad_options.br.mode = PADMODE_ZEROS;

  conv2d_layer->groups = npx_json_find_int(node, "groups", 1);

  assert(conv2d_layer->kernel_size > 0);
  assert(conv2d_layer->stride > 0);
  assert(conv2d_layer->stride < 16);
  assert(conv2d_layer->groups > 0);

  parse_iodata2d(&(conv2d_layer->iodata), node);

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
  npx_tensor_set_datatype(conv2d_layer->weight_tensor, get_weight_datatype(node));
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

static void parse_shortcut(npx_layer_compute_t *layer_compute, const cJSON *node, const char *operator)
{
  npx_shortcut_layer_t *shortcut_layer = (npx_shortcut_layer_t *)calloc(1, sizeof(npx_shortcut_layer_t));
  layer_compute->layer_type = NPXL_SHORTCUT;
  layer_compute->layer = shortcut_layer;

  const char *shortcut_type = npx_json_find_str(node, "op", "Identity");
  if (strcmp(shortcut_type, "Identity") == 0)
  {
    shortcut_layer->shortcut_type = NPXL_IDENTITY;
    shortcut_layer->layer = NULL;
    parse_iodata2d(&(shortcut_layer->iodata), node);
  }
  else
  {
    npx_layer_compute_t inner_layer_compute;
    memset(&inner_layer_compute, 0, sizeof(npx_layer_compute_t));
    if (strcmp(shortcut_type, "Conv2d") == 0)
      parse_conv2d(&inner_layer_compute, node, operator);
    else if (strcmp(shortcut_type, "MaxPool2d") == 0)
      parse_pool2d(&inner_layer_compute, node, operator, NPXL_MAXPOOL2D);
    else if (strcmp(shortcut_type, "AvgPool2d") == 0)
      parse_pool2d(&inner_layer_compute, node, operator, NPXL_AVGPOOL2D);
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

static void parse_pool2d(npx_layer_compute_t *layer_compute, const cJSON *node, const char *operator, npx_layer_type_t layer_type)
{
  npx_pool2d_layer_t *pool2d_layer = (npx_pool2d_layer_t *)calloc(1, sizeof(npx_pool2d_layer_t));
  layer_compute->layer_type = layer_type;
  layer_compute->layer = pool2d_layer;
  pool2d_layer->kernel_size = npx_json_find_int(node, "kernel_size", 0);
  pool2d_layer->stride = npx_json_find_int(node, "stride", 1);

  const int pad_amount = npx_json_find_int(node, "padding", 0);
  pool2d_layer->pad_options.value = 0;
  pool2d_layer->pad_options.br.num_bottom = pad_amount;
  pool2d_layer->pad_options.br.num_top = pad_amount;
  pool2d_layer->pad_options.br.num_right = pad_amount;
  pool2d_layer->pad_options.br.num_left = pad_amount;
  if (pad_amount == 0)
    pool2d_layer->pad_options.br.mode = PADMODE_NONE;
  else
    pool2d_layer->pad_options.br.mode = PADMODE_ZEROS;

  assert(pool2d_layer->kernel_size > 0);
  assert(pool2d_layer->stride > 0);
  assert(pool2d_layer->stride < 16);
  assert(pool2d_layer->kernel_size == pool2d_layer->stride);

  parse_iodata2d(&(pool2d_layer->iodata), node);

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

static void parse_avgpool2d(npx_layer_compute_t *layer_compute, const cJSON *node, const char *operator)
{
  parse_pool2d(layer_compute, node, operator, NPXL_SUMPOOL2D);
}

static void parse_leaky(npx_layer_compute_t *layer_compute, const cJSON *node, const char *operator)
{
  npx_leaky_layer_t *leaky_layer = (npx_leaky_layer_t *)calloc(1, sizeof(npx_leaky_layer_t));
  layer_compute->layer_type = NPXL_LEAKY;
  layer_compute->layer = leaky_layer;

  leaky_layer->reset_mechanism = npx_json_copy_str(npx_json_find_str(node, "reset_mechanism", ""));
  leaky_layer->has_reset_delay = npx_json_find_bool(node, "reset_delay", 0);
  leaky_layer->reset_delay = leaky_layer->has_reset_delay ? "True" : "False";

  leaky_layer->is_hard_reset = (strcmp(leaky_layer->reset_mechanism, "zero") == 0);
  leaky_layer->is_soft_reset = (strcmp(leaky_layer->reset_mechanism, "subtract") == 0);
  assert(leaky_layer->is_hard_reset || leaky_layer->is_soft_reset);

  parse_iodata2d(&(leaky_layer->iodata), node);

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

static void parse_flatten(npx_layer_compute_t *layer_compute, const cJSON *node, const char *operator)
{
  npx_flatten_layer_t *flatten_layer = (npx_flatten_layer_t *)calloc(1, sizeof(npx_flatten_layer_t));
  layer_compute->layer_type = NPXL_FLATTEN;
  layer_compute->layer = flatten_layer;

  parse_iodata2d(&(flatten_layer->iodata), node);

  layer_compute->operator = operator;
  layer_compute->forward = npx_forward_flatten_layer_default;
}

static void parse_linear(npx_layer_compute_t *layer_compute, const cJSON *node, const char *operator)
{
  npx_linear_layer_t *linear_layer = (npx_linear_layer_t *)calloc(1, sizeof(npx_linear_layer_t));
  layer_compute->layer_type = NPXL_LINEAR;
  layer_compute->layer = linear_layer;
  linear_layer->in_features = npx_json_find_int(node, "in_features", 0);
  linear_layer->out_features = npx_json_find_int(node, "out_features", 0);

  assert(linear_layer->in_features >= 0);
  assert(linear_layer->out_features >= 0);

  parse_iodata2d(&(linear_layer->iodata), node);

  // weight
  linear_layer->weight_tensor = npx_tensor_alloc_wo_data(2);
  npx_tensor_set_size(linear_layer->weight_tensor, 0, linear_layer->in_features);
  npx_tensor_set_size(linear_layer->weight_tensor, 1, linear_layer->out_features);
  npx_tensor_set_datatype(linear_layer->weight_tensor, get_weight_datatype(node));
  npx_tensor_alloc_data(linear_layer->weight_tensor);

  linear_layer->transposed_weight_matrix = matrix_alloc(linear_layer->weight_tensor->datatype, linear_layer->in_features, linear_layer->out_features);

  layer_compute->operator = operator;
  layer_compute->forward = npx_forward_linear_layer_default;
}

static void parse_maxpool2d(npx_layer_compute_t *layer_compute, const cJSON *node, const char *operator)
{
  parse_pool2d(layer_compute, node, operator, NPXL_MAXPOOL2D);
}