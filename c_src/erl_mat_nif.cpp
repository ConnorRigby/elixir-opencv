#include "erl_nif.h"
#include "opencv2/opencv.hpp"

static ErlNifResourceType *erl_mat_type = NULL;

typedef struct {
    cv::Mat* mat;
} erl_mat;

static ERL_NIF_TERM
make_atom(ErlNifEnv *env, const char *atom_name)
{
    ERL_NIF_TERM atom;

    if(enif_make_existing_atom(env, atom_name, &atom, ERL_NIF_LATIN1))
	   return atom;

    return enif_make_atom(env, atom_name);
}

static ERL_NIF_TERM
make_ok_tuple(ErlNifEnv *env, ERL_NIF_TERM value)
{
    return enif_make_tuple2(env, make_atom(env, "ok"), value);
}

static ERL_NIF_TERM
make_error_tuple(ErlNifEnv *env, const char *reason)
{
    return enif_make_tuple2(env, make_atom(env, "error"), make_atom(env, reason));
}

static ERL_NIF_TERM
emat_new(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    erl_mat *inemat;
    erl_mat *outemat;
    ERL_NIF_TERM ret;

    outemat = (erl_mat*) (erl_mat*) enif_alloc_resource(erl_mat_type, sizeof(erl_mat));
    if(!outemat)
        return make_error_tuple(env, "no_memory");

    if(enif_get_resource(env, argv[0], erl_mat_type, (void **) &inemat)) {
      cv::Mat inMat = *inemat->mat;
      outemat->mat = new cv::Mat(inMat);
    } else {
      outemat->mat = new cv::Mat();
    }
    ret = enif_make_resource(env, outemat);
    enif_release_resource(outemat);
    return make_ok_tuple(env, ret);
}

static void
destruct_erl_mat(ErlNifEnv *env, void *arg)
{
    erl_mat *emat = (erl_mat *)arg;
    if(emat->mat) {
        delete emat->mat;
    }
}

/*
 * Load the nif. Initialize some stuff and such
 */
static int
on_load(ErlNifEnv* env, void** priv, ERL_NIF_TERM info)
{
    ErlNifResourceType *rt;
    rt = enif_open_resource_type(env, "erl_mat_nif", "erl_mat_type",
                destruct_erl_mat, ERL_NIF_RT_CREATE, NULL);
    if(!rt)
        return -1;
    erl_mat_type = rt;    
    return 0;
}

static int on_reload(ErlNifEnv* env, void** priv_data, ERL_NIF_TERM load_info)
{
    return 0;
}

static int on_upgrade(ErlNifEnv* env, void** priv, void** old_priv_data, ERL_NIF_TERM load_info)
{
    return 0;
}

static ErlNifFunc nif_funcs[] = {
  {"new", 1, emat_new}
};

ERL_NIF_INIT(Elixir.OpenCv.MatNif, nif_funcs, on_load, on_reload, on_upgrade, NULL);