#ifndef ERL_CV_UTIL_H
#define ERL_CV_UTIL_H

#include "erl_nif.h"
#include <stdio.h>
#include <string.h>

ERL_NIF_TERM make_atom(ErlNifEnv*, const char*);
ERL_NIF_TERM make_ok_tuple(ErlNifEnv*, ERL_NIF_TERM);
ERL_NIF_TERM make_error_tuple(ErlNifEnv*, const char*);
ERL_NIF_TERM make_binary(ErlNifEnv*, const void*, unsigned int);

#endif