/*
 * Copyright 2011 - 2017 Maas-Maarten Zeeman
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/


#include <string.h>
#include <stdio.h>

#include "erl_nif.h"
#include "queue.hpp"

#include "opencv2/opencv.hpp"

#define MAX_PATHNAME 512

static ErlNifResourceType *erl_video_capture_connection_type = NULL;

typedef struct {
    ErlNifTid tid;
    ErlNifThreadOpts* opts;
    ErlNifPid notification_pid;
    queue *commands;
    cv::VideoCapture* cap;

} erl_video_capture_connection;

static ErlNifResourceType *erl_mat_type = NULL;

typedef struct {
    cv::Mat* mat;
} erl_mat;

typedef enum {
    cmd_unknown,
    cmd_open,
    cmd_close,
    cmd_stop,
    cmd_is_opened,
    cmd_grab,
    cmd_retrieve,
    cmd_read,
    cmd_get,
    cmd_set,
} command_type;

typedef struct {
    command_type type;

    ErlNifEnv *env;
    ERL_NIF_TERM ref;
    ErlNifPid pid;
    ERL_NIF_TERM arg;
} erl_video_capture_command;

static ERL_NIF_TERM atom_erl_video_capture;

static ERL_NIF_TERM push_command(ErlNifEnv *env, erl_video_capture_connection *conn, erl_video_capture_command *cmd);

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
make_binary(ErlNifEnv *env, const void *bytes, unsigned int size)
{
    ErlNifBinary blob;
    ERL_NIF_TERM term;

    if(!enif_alloc_binary(size, &blob)) {
	    /* TODO: fix this */
	    return make_atom(env, "error");
    }

    memcpy(blob.data, bytes, size);
    term = enif_make_binary(env, &blob);
    enif_release_binary(&blob);

    return term;
}

static void
command_destroy(void *obj)
{
    erl_video_capture_command *cmd = (erl_video_capture_command *) obj;

    if(cmd->env != NULL)
	   enif_free_env(cmd->env);

    enif_free(cmd);
}

static erl_video_capture_command*
command_create()
{
    erl_video_capture_command *cmd = (erl_video_capture_command *) enif_alloc(sizeof(erl_video_capture_command));
    if(cmd == NULL)
	   return NULL;

    cmd->env = enif_alloc_env();
    if(cmd->env == NULL) {
	    command_destroy(cmd);
        return NULL;
    }

    cmd->type = cmd_unknown;
    cmd->ref = 0;
    cmd->arg = 0;
    return cmd;
}

static ERL_NIF_TERM
do_open(ErlNifEnv *env, erl_video_capture_connection *conn, const ERL_NIF_TERM arg)
{
    char filename[MAX_PATHNAME];
    unsigned int size;
    int rc;
    ERL_NIF_TERM error;

    size = enif_get_string(env, arg, filename, MAX_PATHNAME, ERL_NIF_LATIN1);
    if(size <= 0)
        return make_error_tuple(env, "invalid_filename");

    cv::VideoCapture* cap;
    cap = new cv::VideoCapture(filename);
    conn->cap = cap;
    
    if (!cap->isOpened()) {
        conn->cap->release();
        conn->cap = NULL;
        return make_atom(env, "error");
    }

    return make_atom(env, "ok");
}

static ERL_NIF_TERM
do_close(ErlNifEnv *env, erl_video_capture_connection *conn)
{
    if(conn->cap) {
        conn->cap->release();
        conn->cap = NULL;
    }
    return make_atom(env, "ok");
}

static ERL_NIF_TERM
do_is_opened(ErlNifEnv *env, erl_video_capture_connection *conn)
{
    cv::VideoCapture cap;

    if(conn->cap == NULL)
        return make_error_tuple(env, "not_open");
    cap = *conn->cap;
    return cap.isOpened() ? enif_make_atom(env, "true") : enif_make_atom(env, "false");
}

static ERL_NIF_TERM
do_grab(ErlNifEnv *env, erl_video_capture_connection *conn)
{
    cv::VideoCapture cap;

    if(conn->cap == NULL)
        return make_error_tuple(env, "not_open");
    cap = *conn->cap;

    if(!cap.isOpened())
        return make_error_tuple(env, "not_open");
    
    return cap.grab() ? make_atom(env, "true") : make_atom(env, "false");
}

static ERL_NIF_TERM
do_retrieve(ErlNifEnv *env, erl_video_capture_connection *conn, const ERL_NIF_TERM arg)
{
    int flag;
    erl_mat  *emat;
    ERL_NIF_TERM emat_term;
    emat = (erl_mat*) (erl_mat*) enif_alloc_resource(erl_mat_type, sizeof(erl_mat));
    if(!emat)
        return make_error_tuple(env, "no_memory");
    emat->mat = new cv::Mat();

    if(!enif_get_int(env, arg, &flag))
        return make_error_tuple(env, "invalid_flag");

    if(conn->cap == NULL)
        return make_error_tuple(env, "not_open");

    cv::VideoCapture cap = *conn->cap;
    cv::Mat frame = *emat->mat;

    if(!cap.isOpened())
        return make_error_tuple(env, "not_open");

    if (!cap.retrieve(frame, flag))
        return make_atom(env, "false");

    if(frame.empty()) {
        emat_term = make_atom(env, "nil");
    } else {
        emat_term = enif_make_resource(env, emat);
    }
    enif_release_resource(emat);
    return make_ok_tuple(env, emat_term);
}

static ERL_NIF_TERM
do_read(ErlNifEnv *env, erl_video_capture_connection *conn)
{
    erl_mat *emat;
    ERL_NIF_TERM ret;
    emat = (erl_mat*) enif_alloc_resource(erl_mat_type, sizeof(erl_mat));

    if(!emat)
        return make_error_tuple(env, "no_memory");
    emat->mat = new cv::Mat();

    if(conn->cap == NULL)
        return make_error_tuple(env, "not_open");

    cv::VideoCapture cap = *conn->cap;
    cv::Mat frame = *emat->mat;

    if(!cap.isOpened())
        return make_error_tuple(env, "not_open");

    if(!cap.read(frame))
        return make_atom(env, "false");

    if(frame.empty()) {
        ret = make_atom(env, "nil");
    } else {
        ret = enif_make_resource(env, emat);
    }
    enif_release_resource(emat);
    return make_ok_tuple(env, ret);
}

static ERL_NIF_TERM
do_get(ErlNifEnv *env, erl_video_capture_connection *conn, const ERL_NIF_TERM arg)
{
    cv::VideoCapture cap;
    int propid;
    double value;

    if(conn->cap == NULL)
        return make_error_tuple(env, "not_open");
    if(!enif_get_int(env, arg, &propid))
        return make_error_tuple(env, "invalid_propid");
    cap = *conn->cap;

    value = cap.get(propid);
    return enif_make_double(env, value);
}

static ERL_NIF_TERM
do_set(ErlNifEnv *env, erl_video_capture_connection *conn, const ERL_NIF_TERM arg)
{
    cv::VideoCapture cap;
    int propid;
    double value;
    int kvArity;
    const ERL_NIF_TERM* kv;

    if(conn->cap == NULL)
        return make_error_tuple(env, "not_open");
    cap = *conn->cap;

    if(!enif_get_tuple(env, arg, &kvArity, &kv))
        return make_error_tuple(env, "invalid_arg");
    
    if(kvArity != 2)
        return make_error_tuple(env, "invalid_arg");
    
    if(!enif_get_int(env, kv[0], &propid))
        return make_error_tuple(env, "invalid_propid");
    
    if(!enif_get_double(env, kv[1], &value))
        return make_error_tuple(env, "invalid_propvalue");

    return cap.set(propid, value) ? enif_make_atom(env, "true") : enif_make_atom(env, "false");
}

static ERL_NIF_TERM
evaluate_command(erl_video_capture_command *cmd, erl_video_capture_connection *conn)
{
    switch(cmd->type) {
      case cmd_open:
        return do_open(cmd->env, conn, cmd->arg);
      case cmd_close:
        return do_close(cmd->env, conn);
      case cmd_is_opened:
        return do_is_opened(cmd->env, conn);
      case cmd_grab:
        return do_grab(cmd->env, conn);
      case cmd_retrieve:
        return do_retrieve(cmd->env, conn, cmd->arg);
      case cmd_read:
        return do_read(cmd->env, conn);
      case cmd_get:
        return do_get(cmd->env, conn, cmd->arg);
      case cmd_set:
        return do_set(cmd->env, conn, cmd->arg);
      default:
        return make_error_tuple(cmd->env, "invalid_command");
    }
}

static ERL_NIF_TERM
push_command(ErlNifEnv *env, erl_video_capture_connection *conn, erl_video_capture_command *cmd) {
    if(!queue_push(conn->commands, cmd))
        return make_error_tuple(env, "command_push_failed");

    return make_atom(env, "ok");
}

static ERL_NIF_TERM
make_answer(erl_video_capture_command *cmd, ERL_NIF_TERM answer)
{
    return enif_make_tuple3(cmd->env, atom_erl_video_capture, cmd->ref, answer);
}

static void *
erl_video_capture_connection_run(void *arg)
{
    erl_video_capture_connection *conn = (erl_video_capture_connection *) arg;
    erl_video_capture_command *cmd;
    int continue_running = 1;

    while(continue_running) {
	    cmd = (erl_video_capture_command*)queue_pop(conn->commands);

        if(cmd->type == cmd_stop) {
	        continue_running = 0;
        } else {
	        enif_send(NULL, &cmd->pid, cmd->env, make_answer(cmd, evaluate_command(cmd, conn)));
        }

	    command_destroy(cmd);
    }

    return NULL;
}

/*
 * Start the processing thread
 */
static ERL_NIF_TERM
erl_video_capture_start(ErlNifEnv* env, int argc, const ERL_NIF_TERM argv[])
{
    erl_video_capture_connection *conn;
    ERL_NIF_TERM conn_resource;

    /* Initialize the resource */
    conn = (erl_video_capture_connection *) enif_alloc_resource(erl_video_capture_connection_type, sizeof(erl_video_capture_connection));
    if(!conn)
	    return make_error_tuple(env, "no_memory");

    /* Create command queue */
    conn->commands = queue_create();
    if(!conn->commands) {
	    enif_release_resource(conn);
	    return make_error_tuple(env, "command_queue_create_failed");
    }

    /* Start command processing thread */
    conn->opts = enif_thread_opts_create((char*) "erl_video_capture_thread_opts");
    if(enif_thread_create((char*) "erl_video_capture_connection", &conn->tid, erl_video_capture_connection_run, conn, conn->opts) != 0) {
	    enif_release_resource(conn);
	    return make_error_tuple(env, (char*)"thread_create_failed");
    }

    conn_resource = enif_make_resource(env, conn);
    enif_release_resource(conn);

    return make_ok_tuple(env, conn_resource);
}

static ERL_NIF_TERM
erl_video_capture_open(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    erl_video_capture_connection *conn;
    erl_video_capture_command *cmd = NULL;
    ErlNifPid pid;

    if(argc != 4)
	    return enif_make_badarg(env);
    if(!enif_get_resource(env, argv[0], erl_video_capture_connection_type, (void **) &conn))
	    return enif_make_badarg(env);
    if(!enif_is_ref(env, argv[1]))
	    return make_error_tuple(env, "invalid_ref");
    if(!enif_get_local_pid(env, argv[2], &pid))
	    return make_error_tuple(env, "invalid_pid");
    if(!enif_is_list(env, argv[3]))
        return enif_make_badarg(env);

    /* Note, no check is made for the type of the argument */
    cmd = command_create();
    if(!cmd)
	    return make_error_tuple(env, "command_create_failed");

    cmd->type = cmd_open;
    cmd->ref = enif_make_copy(cmd->env, argv[1]);
    cmd->pid = pid;
    cmd->arg = enif_make_copy(cmd->env, argv[3]);

    return push_command(env, conn, cmd);
}

static ERL_NIF_TERM
erl_video_capture_close(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    erl_video_capture_connection *conn;
    erl_video_capture_command *cmd = NULL;
    ErlNifPid pid;

    if(!enif_get_resource(env, argv[0], erl_video_capture_connection_type, (void **) &conn))
	    return enif_make_badarg(env);
    if(!enif_is_ref(env, argv[1]))
	    return make_error_tuple(env, "invalid_ref");
    if(!enif_get_local_pid(env, argv[2], &pid))
	    return make_error_tuple(env, "invalid_pid");

    cmd = command_create();
    if(!cmd)
	    return make_error_tuple(env, "command_create_failed");

    cmd->type = cmd_close;
    cmd->ref = enif_make_copy(cmd->env, argv[1]);
    cmd->pid = pid;
    return push_command(env, conn, cmd);
}

/**
 * Returns true if video capturing has been initialized already.
    If the previous call to VideoCapture constructor or VideoCapture::open() succeeded, the method returns true. 
 * https://docs.opencv.org/3.4.5/d8/dfe/classcv_1_1VideoCapture.html#a9d2ca36789e7fcfe7a7be3b328038585
*/
static ERL_NIF_TERM
erl_video_capture_is_opened(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    erl_video_capture_connection *conn;
    erl_video_capture_command *cmd = NULL;
    ErlNifPid pid;

    if(!enif_get_resource(env, argv[0], erl_video_capture_connection_type, (void **) &conn))
	    return enif_make_badarg(env);
    if(!enif_is_ref(env, argv[1]))
	    return make_error_tuple(env, "invalid_ref");
    if(!enif_get_local_pid(env, argv[2], &pid))
	    return make_error_tuple(env, "invalid_pid");

    cmd = command_create();
    if(!cmd)
	    return make_error_tuple(env, "command_create_failed");

    cmd->type = cmd_is_opened;
    cmd->ref = enif_make_copy(cmd->env, argv[1]);
    cmd->pid = pid;
    return push_command(env, conn, cmd);
}

/**
 * Grabs the next frame from video file or capturing device. 
 * https://docs.opencv.org/3.4.5/d8/dfe/classcv_1_1VideoCapture.html#ae38c2a053d39d6b20c9c649e08ff0146
*/
static ERL_NIF_TERM
erl_video_capture_grab(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    erl_video_capture_connection *conn;
    erl_video_capture_command *cmd = NULL;
    ErlNifPid pid;

    if(!enif_get_resource(env, argv[0], erl_video_capture_connection_type, (void **) &conn))
	    return enif_make_badarg(env);
    if(!enif_is_ref(env, argv[1]))
	    return make_error_tuple(env, "invalid_ref");
    if(!enif_get_local_pid(env, argv[2], &pid))
	    return make_error_tuple(env, "invalid_pid");

    cmd = command_create();
    if(!cmd)
	    return make_error_tuple(env, "command_create_failed");

    cmd->type = cmd_grab;
    cmd->ref = enif_make_copy(cmd->env, argv[1]);
    cmd->pid = pid;
    return push_command(env, conn, cmd);
}

/**
 * grab and decode a frame
 * https://docs.opencv.org/3.2.0/d8/dfe/classcv_1_1VideoCapture.html#a9ac7f4b1cdfe624663478568486e6712
*/
static ERL_NIF_TERM
erl_video_capture_retrieve(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    erl_video_capture_connection *conn;
    erl_video_capture_command *cmd = NULL;
    ErlNifPid pid;

    if(!enif_get_resource(env, argv[0], erl_video_capture_connection_type, (void **) &conn))
	    return enif_make_badarg(env);
    if(!enif_is_ref(env, argv[1]))
	    return make_error_tuple(env, "invalid_ref");
    if(!enif_get_local_pid(env, argv[2], &pid))
	    return make_error_tuple(env, "invalid_pid");
    if(!enif_is_number(env, argv[3]))
        return make_error_tuple(env, "invalid_number");

    cmd = command_create();
    if(!cmd)
	    return make_error_tuple(env, "command_create_failed");

    cmd->type = cmd_retrieve;
    cmd->ref = enif_make_copy(cmd->env, argv[1]);
    cmd->pid = pid;
    cmd->arg = enif_make_copy(cmd->env, argv[3]);
    return push_command(env, conn, cmd);
}

/**
 * Grabs, decodes and returns the next video frame. 
 * https://docs.opencv.org/3.4.5/d8/dfe/classcv_1_1VideoCapture.html#a473055e77dd7faa4d26d686226b292c1
*/
static ERL_NIF_TERM
erl_video_capture_read(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    erl_video_capture_connection *conn;
    erl_video_capture_command *cmd = NULL;
    ErlNifPid pid;

    if(!enif_get_resource(env, argv[0], erl_video_capture_connection_type, (void **) &conn))
	    return enif_make_badarg(env);
    if(!enif_is_ref(env, argv[1]))
	    return make_error_tuple(env, "invalid_ref");
    if(!enif_get_local_pid(env, argv[2], &pid))
	    return make_error_tuple(env, "invalid_pid");

    cmd = command_create();
    if(!cmd)
	    return make_error_tuple(env, "command_create_failed");

    cmd->type = cmd_read;
    cmd->ref = enif_make_copy(cmd->env, argv[1]);
    cmd->pid = pid;
    return push_command(env, conn, cmd);
}

/**
 * Returns the specified VideoCapture property. 
 * https://docs.opencv.org/3.4.5/d8/dfe/classcv_1_1VideoCapture.html#aa6480e6972ef4c00d74814ec841a2939
*/
static ERL_NIF_TERM
erl_video_capture_get(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    erl_video_capture_connection *conn;
    erl_video_capture_command *cmd = NULL;
    ErlNifPid pid;

    if(!enif_get_resource(env, argv[0], erl_video_capture_connection_type, (void **) &conn))
	    return enif_make_badarg(env);
    if(!enif_is_ref(env, argv[1]))
	    return make_error_tuple(env, "invalid_ref");
    if(!enif_get_local_pid(env, argv[2], &pid))
	    return make_error_tuple(env, "invalid_pid");
    if(!enif_is_number(env, argv[3]))
        return make_error_tuple(env, "invalid_id");

    cmd = command_create();
    if(!cmd)
	    return make_error_tuple(env, "command_create_failed");

    cmd->type = cmd_get;
    cmd->ref = enif_make_copy(cmd->env, argv[1]);
    cmd->pid = pid;
    cmd->arg = enif_make_copy(cmd->env, argv[3]);
    return push_command(env, conn, cmd);
}

/**
 * Sets a property in the VideoCapture. 
 * https://docs.opencv.org/3.4.5/d8/dfe/classcv_1_1VideoCapture.html#a8c6d8c2d37505b5ca61ffd4bb54e9a7c
*/
static ERL_NIF_TERM
erl_video_capture_set(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    erl_video_capture_connection *conn;
    erl_video_capture_command *cmd = NULL;
    ErlNifPid pid;

    if(!enif_get_resource(env, argv[0], erl_video_capture_connection_type, (void **) &conn))
	    return enif_make_badarg(env);
    if(!enif_is_ref(env, argv[1]))
	    return make_error_tuple(env, "invalid_ref");
    if(!enif_get_local_pid(env, argv[2], &pid))
	    return make_error_tuple(env, "invalid_pid");
    if(!enif_is_tuple(env, argv[3]))
        return make_error_tuple(env, "invalid_arg");

    cmd = command_create();
    if(!cmd)
	    return make_error_tuple(env, "command_create_failed");

    cmd->type = cmd_set;
    cmd->ref = enif_make_copy(cmd->env, argv[1]);
    cmd->pid = pid;
    cmd->arg = enif_make_copy(cmd->env, argv[3]);
    return push_command(env, conn, cmd);
}

static void
destruct_erl_video_capture_connection(ErlNifEnv *env, void *arg)
{
    erl_video_capture_connection *conn = (erl_video_capture_connection *) arg;
    erl_video_capture_command *close_cmd = command_create();
    erl_video_capture_command *stop_cmd = command_create();

    /* Send the stop command
    */
    close_cmd->type = cmd_close;
    queue_push(conn->commands, close_cmd);

    /* Send the stop command
     */
    stop_cmd->type = cmd_stop;
    queue_push(conn->commands, stop_cmd);

    /* Wait for the thread to finish
     */
    enif_thread_join(conn->tid, NULL);

    enif_thread_opts_destroy(conn->opts);

    while(queue_has_item(conn->commands)) {
        command_destroy(queue_pop(conn->commands));
    }
    queue_destroy(conn->commands);
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

    rt = enif_open_resource_type(env, "erl_video_capture_nif", "erl_video_capture_connection_type",
				destruct_erl_video_capture_connection, ERL_NIF_RT_CREATE, NULL);
    if(!rt)
	    return -1;
    erl_video_capture_connection_type = rt;

    rt = enif_open_resource_type(env, "erl_video_capture_nif", "erl_mat_type",
                destruct_erl_mat, ERL_NIF_RT_CREATE, NULL);
    if(!rt)
        return -1;
    erl_mat_type = rt;    

    atom_erl_video_capture = make_atom(env, "erl_video_capture");
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
    {"start", 0, erl_video_capture_start},
    {"open", 4, erl_video_capture_open},
    {"close", 3, erl_video_capture_close},
    {"is_opened", 3, erl_video_capture_is_opened},
    {"grab", 3, erl_video_capture_grab},
    {"retreive", 4, erl_video_capture_retrieve},
    {"read", 3, erl_video_capture_read},
    {"get", 4, erl_video_capture_get},
    {"set", 4, erl_video_capture_set},
};
ERL_NIF_INIT(Elixir.OpenCv.VideoCaptureNif, nif_funcs, on_load, on_reload, on_upgrade, NULL);
