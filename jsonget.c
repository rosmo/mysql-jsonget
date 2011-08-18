/*
Copyright (c) 2011 Taneli Leppä <rosmo@sektori.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "jsonget.h"

#include <yajl/yajl_parse.h>
#include <yajl/yajl_gen.h>

#define RES_NULL 0
#define RES_BOOLEAN 1
#define RES_DOUBLE 2
#define RES_INTEGER 3
#define RES_STRING 4
#define RES_STRING_NOALLOC 5

typedef struct {
  yajl_gen g;
  const char *index;
  int index_len;
  unsigned int depth;

  int status;
  int result_type;
  int bool_val;
  double double_val;
  long long integer_val;
  char *string_val;
  int string_len;

  char **udf_result;
} jsonrpc_ctx;

static int jsonrpc_parse_null(void * ctx)
{
  jsonrpc_ctx *c = (jsonrpc_ctx *)ctx;
  if (c->status == 1) 
  {
    c->status = 2;

    c->result_type = RES_NULL;
    return 0;
  }
  if (c->status == 2)
    return 0;
  return 1;
}

static int jsonrpc_parse_boolean(void * ctx, int boolean)
{
  jsonrpc_ctx *c = (jsonrpc_ctx *)ctx;
  if (c->status == 1) 
  {
    c->status = 2;

    c->result_type = RES_BOOLEAN;
    c->bool_val = boolean;
    return 0;
  }
  if (c->status == 2)
    return 0;
  return 1;
}

static int jsonrpc_parse_integer(void * ctx, long long integerVal)
{
  jsonrpc_ctx *c = (jsonrpc_ctx *)ctx;
  if (c->status == 1) 
  {
    c->status = 2;

    c->result_type = RES_INTEGER;
    c->integer_val = integerVal;
    return 0;
  }
  if (c->status == 2)
    return 0;
  return 1;
}

static int jsonrpc_parse_double(void * ctx, double doubleVal)
{
  jsonrpc_ctx *c = (jsonrpc_ctx *)ctx;
  if (c->status == 1) 
  {
    c->status = 2;

    c->result_type = RES_DOUBLE;
    c->double_val = doubleVal;
    return 0;
  }
  if (c->status == 2)
    return 0;
  return 1;
}


static int jsonrpc_parse_string(void * ctx, const unsigned char * stringVal,
				size_t stringLen)
{
  jsonrpc_ctx *c = (jsonrpc_ctx *)ctx;
  if (c->status == 1) 
  {
    c->status = 2;

    if (stringLen < 255)
    {
      memmove(*(c->udf_result), stringVal, stringLen);
      c->result_type = RES_STRING_NOALLOC;
      c->string_len = stringLen;
    } else {
      c->string_val = (char *)malloc((size_t) (sizeof(char) * stringLen));
      if (c->string_val != NULL) {
	memmove(c->string_val, stringVal, stringLen);
	c->result_type = RES_STRING;  
	c->string_len = stringLen;
      } else {
	c->result_type = RES_NULL;
      }
    }
    return 0;
  }
  if (c->status == 2)
    return 0;
  return 1;
}

static int jsonrpc_parse_map_key(void * ctx, const unsigned char * stringVal,
				 size_t stringLen)
{
  jsonrpc_ctx *c = (jsonrpc_ctx *)ctx;
  if (c->depth < 2 &&
      stringLen == c->index_len &&
      strncmp((char *)stringVal, c->index, c->index_len) == 0)
  {
    c->status = 1; // found the key, now for the data
  }
  return 1;
}

static int jsonrpc_parse_start_map(void * ctx)
{
  jsonrpc_ctx *c = (jsonrpc_ctx *)ctx;
  c->depth++;
  return 1;
}

static int jsonrpc_parse_end_map(void * ctx)
{
  jsonrpc_ctx *c = (jsonrpc_ctx *)ctx;
  c->depth--;
  return 1;
}

static yajl_callbacks callbacks = {
    jsonrpc_parse_null,
    jsonrpc_parse_boolean,
    jsonrpc_parse_integer,
    jsonrpc_parse_double,
    NULL,
    jsonrpc_parse_string,
    jsonrpc_parse_start_map,
    jsonrpc_parse_map_key,
    jsonrpc_parse_end_map,
    NULL,
    NULL
};

char *json_get(UDF_INIT *initid, UDF_ARGS *args, 
	      char *result, unsigned long *length, char *is_null, 
	      char *error)
{
  const char *index = (const char *)args->args[0];
  const char *json = (const char *)args->args[1];
  yajl_handle h;
  yajl_status stat;
  jsonrpc_ctx ctx = { 0 };
  int len;

  ctx.index = (const char *)index;
  ctx.index_len = args->lengths[0];
  ctx.udf_result = &result;

  ctx.g = yajl_gen_alloc(NULL);
  if (ctx.g == NULL) {
    *error = 1;
    return NULL;
  }

  h = yajl_alloc(&callbacks, NULL, (void *)&ctx);
  if (h == NULL) {
    yajl_gen_free(ctx.g);
    *error = 1;
    return NULL;
  }

  // no string validation
  yajl_config(h, yajl_dont_validate_strings, 1); 

  stat = yajl_parse(h, (unsigned char *)json, args->lengths[1]);
  if (stat == yajl_status_error)
  {
    yajl_gen_free(ctx.g);
    yajl_free(h);

    *error = 1;
    if (ctx.string_val != NULL) {
      free(ctx.string_val);
    }
    return NULL;
  }

  yajl_gen_free(ctx.g);
  yajl_free(h);

  initid->ptr = NULL;
  switch (ctx.result_type)
  {
    case RES_NULL:
      *is_null = 1;
      return NULL;
      break;
    case RES_INTEGER:
      len = snprintf(result, 255, "%lld", ctx.integer_val);
      *length = len;
      return result;
      break;
    case RES_BOOLEAN:
      len = snprintf(result, 255, "%d", ctx.bool_val);
      *length = len;
      return result;
      break;
    case RES_DOUBLE:
      len = snprintf(result, 255, "%f", ctx.double_val);
      *length = len;
      return result;
      break;
    case RES_STRING:
      initid->ptr = ctx.string_val;
      *length = ctx.string_len;
      return ctx.string_val;
      break;
    case RES_STRING_NOALLOC:
      *length = ctx.string_len;
      return result;
      break;
  }
  *is_null = 1;
  return NULL;
}

/**
 * Functions arguments are:
 * json_get(value, json)
 */
my_bool json_get_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
  if (args->arg_count != 2)
  {
    strcpy(message, "Too few or many arguments");
    return 1;
  }
  if (args->arg_type[1] != STRING_RESULT)
  {
    strcpy(message, "Wrong type of argument, string is expected");
    return 1;
  }
  args->arg_type[0] = STRING_RESULT; // coerce first argument to string

  initid->maybe_null = 1;
  initid->max_length = 0xFFFF;

  return 0;
}

void json_get_deinit(UDF_INIT *initid)
{
  if (initid->ptr != NULL) {
    free(initid->ptr);
  }
}


