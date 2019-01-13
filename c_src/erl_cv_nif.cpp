#include "erl_nif.h"
#include "erl_cv_util.hpp"
#include "opencv2/opencv.hpp"

static ErlNifResourceType *erl_mat_type = NULL;

typedef struct {
    cv::Mat* mat;
} erl_mat;

static ERL_NIF_TERM
erl_cv_imencode(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    cv::Mat inFrame, outFrame;
    erl_mat *inemat;

    if(!enif_get_resource(env, argv[0], erl_mat_type, (void **) &inemat))
      return enif_make_badarg(env);

    inFrame = *inemat->mat;
    
    //buffer for storing frame
    std::vector<uchar> buff;
    std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 80};
    cv::imencode(".jpg", outFrame, buff, params);
    return make_binary(env, buff.data(), buff.size());
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
    rt = enif_open_resource_type(env, "erl_cv_nif", "erl_mat_type",
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
  {"imencode", 3, erl_cv_imencode}
};

ERL_NIF_INIT(Elixir.OpenCv.CvNif, nif_funcs, on_load, on_reload, on_upgrade, NULL);