#ifndef __NPX_JSON_H__
#define __NPX_JSON_H__

#include "cJSON.h"

#ifdef __cplusplus
extern "C"
{
#endif

  cJSON *npx_json_read_file(const char *fname);
  const cJSON *npx_json_get_object(const cJSON *root, const char *key, const char *fname);
  char *npx_json_copy_str(const char *str);
  int npx_json_find_int(const cJSON *object, const char *key, int def);
  const char *npx_json_find_str(const cJSON *object, const char *key, const char *def);
  int npx_json_find_bool(const cJSON *object, const char *key, int def);
  void npx_json_find_size(const cJSON *object, const char *key, int *size_array, int num);

#ifdef __cplusplus
} // exter "C"
#endif

#endif // __NPX_JSON_H__
