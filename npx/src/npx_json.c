#include "ervp_assert.h"
#include "ervp_malloc.h"
#include "ervp_fakefile.h"

#include <string.h>

#include "npx_json.h"

static char *read_text_file(const char *fname)
{
  assert_msg(fakefile_exists(fname), "%s", fname);
  const int size = fakefile_get_size_by_name(fname);
  char *text = (char *)malloc(size + 1);
  assert(text);
  FAKEFILE *fp = ffopen(fname, "r");
  assert_msg(fp, "%s", fname);
  const size_t nread = ffread(text, 1, size, fp);
  assert_msg(nread == (size_t)size, "%s", fname);
  (void)nread;
  ffclose(fp);
  text[size] = 0;
  return text;
}

cJSON *npx_json_read_file(const char *fname)
{
  char *text = read_text_file(fname);
  cJSON_Minify(text);
  cJSON *root = cJSON_Parse(text);
  assert_msg(root, "%s: %s", fname, cJSON_GetErrorPtr());
  free(text);
  return root;
}

const cJSON *npx_json_get_object(const cJSON *root, const char *key, const char *fname)
{
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
  assert_msg(cJSON_IsObject(item), "%s: %s", fname, key);
  return item;
}

char *npx_json_copy_str(const char *str)
{
  char *result = (char *)malloc(strlen(str) + 1);
  assert(result);
  strcpy(result, str);
  return result;
}

int npx_json_find_int(const cJSON *object, const char *key, int def)
{
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
  if (item == NULL)
    return def;
  if (cJSON_IsBool(item))
    return cJSON_IsTrue(item);
  assert_msg(cJSON_IsNumber(item), "%s", key);
  return item->valueint;
}

const char *npx_json_find_str(const cJSON *object, const char *key, const char *def)
{
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
  if (item == NULL)
    return def;
  assert_msg(cJSON_IsString(item), "%s", key);
  return item->valuestring;
}

int npx_json_find_bool(const cJSON *object, const char *key, int def)
{
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
  if (item == NULL)
    return def;
  if (cJSON_IsString(item))
    return (strcmp(item->valuestring, "True") == 0) || (strcmp(item->valuestring, "true") == 0);
  if (cJSON_IsNumber(item))
    return item->valueint != 0;
  assert_msg(cJSON_IsBool(item), "%s", key);
  return cJSON_IsTrue(item);
}

void npx_json_find_size(const cJSON *object, const char *key, int *size_array, int num)
{
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
  for (int i = 0; i < num; i++)
    size_array[i] = 0;
  if (item == NULL)
    return;
  if (cJSON_IsNumber(item))
  {
    size_array[num - 1] = item->valueint;
    return;
  }
  assert_msg(cJSON_IsArray(item), "%s", key);
  assert_msg(cJSON_GetArraySize(item) >= num, "%s", key);
  for (int i = 0; i < num; i++)
    size_array[num - 1 - i] = cJSON_GetArrayItem(item, i)->valueint;
}
