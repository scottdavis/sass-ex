#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "sass.h"
#include <stdio.h>
#include "erl_nif.h"



static inline ERL_NIF_TERM make_atom(ErlNifEnv* env, const char* name)
{
  ERL_NIF_TERM ret;
  if(enif_make_existing_atom(env, name, &ret, ERL_NIF_LATIN1)) {
    return ret;
  }
  return enif_make_atom(env, name);
}

static inline ERL_NIF_TERM make_tuple(ErlNifEnv* env, const char* mesg, const char* atom_string)
{
  int output_len = sizeof(char) * strlen(mesg);
  ErlNifBinary output_binary;
  enif_alloc_binary(output_len, &output_binary);
  strncpy((char*)output_binary.data, mesg, output_len);
  ERL_NIF_TERM atom = make_atom(env, atom_string);
  ERL_NIF_TERM str = enif_make_binary(env, &output_binary);
  return enif_make_tuple2(env, atom, str);
}

static int my_enif_list_size(ErlNifEnv* env, ERL_NIF_TERM list)
{
  ERL_NIF_TERM head, tail, nexttail;
  int size = 0;
  tail = list;
  while(enif_get_list_cell(env, tail, &head, &nexttail))
  {
    tail = nexttail;
    size = size+1;
  }
  return size;
}

static char* my_enif_get_string(ErlNifEnv *env, ERL_NIF_TERM list)
{
  char *buf;
  int size=my_enif_list_size(env, list);

  if (!(buf = (char*) enif_alloc(size+1)))
  {
    return NULL;
  }
  if (enif_get_string(env, list, buf, size+1, ERL_NIF_LATIN1)<1)
  {
    enif_free(buf);
    return NULL;
  }
  return buf;
}

static ERL_NIF_TERM sass_compile_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  ERL_NIF_TERM ret;

  if (argc != 1) {
    return enif_make_badarg(env);
  }

  char* sass_string = (char*)malloc(my_enif_list_size(env, argv[0]));
  memcpy(sass_string, my_enif_get_string(env, argv[0]), my_enif_list_size(env, argv[0]));

  if(!sass_string) {
    return enif_make_badarg(env);
  }

  struct Sass_Data_Context* ctx = sass_make_data_context(sass_string);

  struct Sass_Context* ctx_out = sass_data_context_get_context(ctx);
  struct Sass_Options* options = sass_context_get_options(ctx_out);

  sass_option_set_output_style(options, SASS_STYLE_NESTED);
  sass_option_set_precision(options, 5);
  sass_data_context_set_options(ctx, options);


  sass_compile_data_context(ctx);

  int error_status = sass_context_get_error_status(ctx_out);
  const char *error_message = sass_context_get_error_message(ctx_out);
  const char *output_string = sass_context_take_output_string(ctx_out);

  if (error_status) {
    if (error_message) {
      ret = make_tuple(env, error_message, "error");
    } else {
      ret = make_tuple(env, "An error occured; no error message available.", "error");
    }
  } else if (output_string) {
    ret = make_tuple(env, output_string, "ok");
  } else {
    ret = make_tuple(env, "Unknown internal error.", "error");
  }
  sass_delete_data_context(ctx);

  return ret;
}

static ERL_NIF_TERM sass_compile_file_nif(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
  ERL_NIF_TERM ret;

  if (argc != 1) {
    return enif_make_badarg(env);
  }


  char* sass_file = (char*)malloc(my_enif_list_size(env, argv[0]));
  memcpy(sass_file, my_enif_get_string(env, argv[0]), my_enif_list_size(env, argv[0]));

  if(!sass_file) {
    return enif_make_badarg(env);
  }

   char cwd[1024];
   if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("getcwd() error");
   }

  char * path = (char*)malloc(strlen(cwd) + strlen(sass_file) + 2);
  strcpy(path, cwd);
  strcat(path, "/");
  strcat(path, sass_file);

  // create the file context and get all related structs
  struct Sass_File_Context* file_ctx = sass_make_file_context(path);
  struct Sass_Context* ctx = sass_file_context_get_context(file_ctx);
  struct Sass_Options* options = sass_context_get_options(ctx);

  // configure some options ...
  sass_option_set_precision(options, 5);
  sass_option_set_output_style(options, SASS_STYLE_NESTED);

  int error_status = sass_compile_file_context(file_ctx);

  const char *error_message = sass_context_get_error_message(ctx);
  const char *output_string = sass_context_get_output_string(ctx);

  if (error_status) {
    if (error_message) {
      ret = make_tuple(env, error_message, "error");
    } else {
      ret = make_tuple(env, "An error occured; no error message available.", "error");
    }
  } else if (output_string) {
    ret = make_tuple(env, output_string, "ok");
  } else {
    ret = make_tuple(env, "Unknown internal error.", "error");
  }
  sass_delete_file_context(file_ctx);

  return ret;
}

static ErlNifFunc nif_funcs[] = {
  { "compile", 1, sass_compile_nif, 0 },
  { "compile_file", 1, sass_compile_file_nif, 0 },
};

ERL_NIF_INIT(Elixir.Sass.Compiler, nif_funcs, NULL, NULL, NULL, NULL);
