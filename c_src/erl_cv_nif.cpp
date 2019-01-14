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
#include "erl_cv_util.hpp"
#include "queue.hpp"

#include "opencv2/opencv.hpp"

#define MAX_PATHNAME 512

static ErlNifResourceType *erl_cv_type = NULL;
typedef struct {
    ErlNifTid tid;
    ErlNifThreadOpts* opts;
    ErlNifPid notification_pid;
    queue *commands;
} erl_cv_connection;

static ErlNifResourceType *erl_cv_mat_type = NULL;
typedef struct {
    cv::Mat* mat;
} erl_cv_mat;

static ErlNifResourceType *erl_cv_video_capture_type = NULL;
typedef struct {
    cv::VideoCapture* cap;
} erl_cv_video_capture;

typedef enum {
    cmd_unknown,
    cmd_stop,
    cmd_video_capture_open,
    cmd_video_capture_close,
    cmd_video_capture_is_opened,
    cmd_video_capture_grab,
    cmd_video_capture_retrieve,
    cmd_video_capture_read,
    cmd_video_capture_get,
    cmd_video_capture_set,
    cmd_imencode,
    cmd_new_mat,
} command_type;

typedef struct {
    command_type type;

    ErlNifEnv *env;
    ERL_NIF_TERM ref;
    ErlNifPid pid;
    ERL_NIF_TERM arg;
} erl_cv_command;

static ERL_NIF_TERM atom_erl_cv;

static ERL_NIF_TERM push_command(ErlNifEnv *env, erl_cv_connection *conn, erl_cv_command *cmd);

static void
command_destroy(void *obj)
{
    erl_cv_command *cmd = (erl_cv_command *) obj;

    if(cmd->env != NULL)
	   enif_free_env(cmd->env);

    enif_free(cmd);
}

static erl_cv_command*
command_create()
{
    erl_cv_command *cmd = (erl_cv_command *) enif_alloc(sizeof(erl_cv_command));
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
do_vc_open(ErlNifEnv *env, erl_cv_connection*, const ERL_NIF_TERM arg)
{
    ERL_NIF_TERM ret;
    char filename[MAX_PATHNAME];
    unsigned int size;
    erl_cv_video_capture* ecap;

    size = enif_get_string(env, arg, filename, MAX_PATHNAME, ERL_NIF_LATIN1);
    if(size <= 0)
        return make_error_tuple(env, "invalid_filename");

    ecap = (erl_cv_video_capture*) enif_alloc_resource(erl_cv_video_capture_type, sizeof(erl_cv_video_capture));
    if(!ecap)
        return make_error_tuple(env, "no_memory");
    ecap->cap = new cv::VideoCapture(filename);

    ret = enif_make_resource(env, ecap);
    enif_release_resource(ecap);
    return make_ok_tuple(env, ret);
}

static ERL_NIF_TERM
do_vc_close(ErlNifEnv *env, erl_cv_connection*, const ERL_NIF_TERM arg)
{
    erl_cv_video_capture* ecap;
    if(!enif_get_resource(env, arg, erl_cv_video_capture_type, (void **) &ecap))
        return enif_make_badarg(env);

    if(ecap->cap) {
        delete ecap->cap;
    }

    return make_atom(env, "ok");
}

static ERL_NIF_TERM
do_vc_is_opened(ErlNifEnv *env, erl_cv_connection*, const ERL_NIF_TERM arg)
{
    erl_cv_video_capture* ecap;
    if(!enif_get_resource(env, arg, erl_cv_video_capture_type, (void **) &ecap))
        return enif_make_badarg(env);
    if(ecap->cap == NULL)
        return make_error_tuple(env, "not_open");
    return ecap->cap->isOpened() ? enif_make_atom(env, "true") : enif_make_atom(env, "false");
}

static ERL_NIF_TERM
do_vc_grab(ErlNifEnv *env, erl_cv_connection*, const ERL_NIF_TERM arg)
{
    erl_cv_video_capture* ecap;    
    if(!enif_get_resource(env, arg, erl_cv_video_capture_type, (void **) &ecap))
        return enif_make_badarg(env);

    if(ecap->cap == NULL)
        return make_error_tuple(env, "not_open");

    if(!ecap->cap->isOpened())
        return make_error_tuple(env, "not_open");
    
    return ecap->cap->grab() ? make_atom(env, "true") : make_atom(env, "false");
}

static ERL_NIF_TERM
do_vc_retrieve(ErlNifEnv *env, erl_cv_connection*, const ERL_NIF_TERM arg)
{
    erl_cv_video_capture *ecap;
    erl_cv_mat *emat;
    int flag;
    int argc;
    const ERL_NIF_TERM *argv;
    if(!enif_get_tuple(env, arg, &argc, &argv))
        return enif_make_badarg(env);
    
    if(argc != 2)
        return enif_make_badarg(env);
    
    if(!enif_get_resource(env, argv[0], erl_cv_video_capture_type, (void **) &ecap))
        return enif_make_badarg(env);

    if(!enif_get_int(env, argv[1], &flag))
        return make_error_tuple(env, "invalid_flag");

    ERL_NIF_TERM emat_term;
    emat = (erl_cv_mat*) enif_alloc_resource(erl_cv_mat_type, sizeof(erl_cv_mat));
    if(!emat)
        return make_error_tuple(env, "no_memory");
    emat->mat = new cv::Mat();

    if(ecap->cap == NULL)
        return make_error_tuple(env, "not_open");

    if(!ecap->cap->isOpened())
        return make_error_tuple(env, "not_open");

    if (!ecap->cap->retrieve(*emat->mat, flag))
        return make_atom(env, "false");

    if(emat->mat->empty()) {
        emat_term = make_atom(env, "nil");
    } else {
        emat_term = enif_make_resource(env, emat);
    }
    enif_release_resource(emat);
    return make_ok_tuple(env, emat_term);
}

static ERL_NIF_TERM
do_vc_read(ErlNifEnv *env, erl_cv_connection*, const ERL_NIF_TERM arg)
{
    erl_cv_video_capture *ecap;
    erl_cv_mat *emat;
    ERL_NIF_TERM ret;
    if(!enif_get_resource(env, arg, erl_cv_video_capture_type, (void **) &ecap))
        return enif_make_badarg(env);

    emat = (erl_cv_mat*) enif_alloc_resource(erl_cv_mat_type, sizeof(erl_cv_mat));

    if(!emat)
        return make_error_tuple(env, "no_memory");
    emat->mat = new cv::Mat();

    if(ecap->cap == NULL)
        return make_error_tuple(env, "not_open");

    if(!ecap->cap->isOpened())
        return make_error_tuple(env, "not_open");

    if(!ecap->cap->read(*emat->mat))
        return make_atom(env, "false");

    if(emat->mat->empty()) {
        ret = make_atom(env, "nil");
    } else {
        ret = enif_make_resource(env, emat);
    }
    enif_release_resource(emat);
    return make_ok_tuple(env, ret);
}

static ERL_NIF_TERM
do_vc_get(ErlNifEnv *env, erl_cv_connection*, const ERL_NIF_TERM arg)
{
    erl_cv_video_capture *ecap;
    int argc, propid;
    double value;
    const ERL_NIF_TERM *argv;

    if(!enif_get_tuple(env, arg, &argc, &argv))
        return enif_make_badarg(env);
    
    if(argc != 2)
        return enif_make_badarg(env);

    enif_fprintf(stderr, "%T ????\r\n", argv[0]);
    if(!enif_get_resource(env, argv[0], erl_cv_video_capture_type, (void **) &ecap))
        return enif_make_badarg(env);

    if(ecap->cap == NULL)
        return make_error_tuple(env, "not_open");

    if(!enif_get_int(env, argv[1], &propid))
        return make_error_tuple(env, "invalid_propid");

    value = ecap->cap->get(propid);
    return enif_make_double(env, value);
}

static ERL_NIF_TERM
do_vc_set(ErlNifEnv *env, erl_cv_connection*, const ERL_NIF_TERM arg)
{
    erl_cv_video_capture *ecap;
    int argc, propid;
    double value;
    const ERL_NIF_TERM* argv;

    if(!enif_get_tuple(env, arg, &argc, &argv))
        return enif_make_badarg(env);
    
    if(argc != 3)
        return enif_make_badarg(env);

    if(!enif_get_resource(env, argv[0], erl_cv_video_capture_type, (void **) &ecap))
        return enif_make_badarg(env);

    if(ecap->cap == NULL)
        return make_error_tuple(env, "not_open");
    
    if(!enif_get_int(env, argv[1], &propid))
        return make_error_tuple(env, "invalid_propid");
    
    if(!enif_get_double(env, argv[2], &value))
        return make_error_tuple(env, "invalid_propvalue");

    return ecap->cap->set(propid, value) ? enif_make_atom(env, "true") : enif_make_atom(env, "false");
}

static ERL_NIF_TERM
do_imencode(ErlNifEnv *env, erl_cv_connection*, const ERL_NIF_TERM arg)
{
    erl_cv_mat *inemat;

    int strSize;
    unsigned int listLength;
    int argc;
    const ERL_NIF_TERM* argv;

    if(!enif_get_tuple(env, arg, &argc, &argv))
        return enif_make_badarg(env);

    if(argc != 3)
        return enif_make_badarg(env);

    // emat resource
    if(!enif_get_resource(env, argv[0], erl_cv_mat_type, (void **) &inemat))
        return enif_make_badarg(env);

    // encoding extension
    if(!enif_get_list_length(env, argv[1], &listLength))
        return make_error_tuple(env, "invalid_string");
    char ext[listLength+1];
    strSize = enif_get_string(env, argv[1], ext, listLength+1, ERL_NIF_LATIN1);

    // encoding params
    if(!enif_get_list_length(env, argv[2], &listLength))
        return make_error_tuple(env, "invalid_params");
    std::vector<int> params;
    ERL_NIF_TERM head;
    ERL_NIF_TERM tail;
    for(unsigned int i = 0; i<listLength; i++) {
        int val;
        if(!enif_get_list_cell(env, argv[2], &head, &tail))
            return make_error_tuple(env, "invalid_params");
        if(!enif_get_int(env, head, &val))
            return make_error_tuple(env, "invalid_param_value");
        params.push_back(val);
    }

    if(strSize <= 0)
        return make_error_tuple(env, "invalid_filename");

    //buffer for storing frame
    std::vector<uchar> buff;
    cv::imencode(ext, *inemat->mat, buff, params);
    return make_binary(env, buff.data(), buff.size());
}

static ERL_NIF_TERM
do_new_mat(ErlNifEnv *env, erl_cv_connection*, const ERL_NIF_TERM arg)
{
    erl_cv_mat *inemat;
    erl_cv_mat *outemat;
    ERL_NIF_TERM ret;

    outemat = (erl_cv_mat*) enif_alloc_resource(erl_cv_mat_type, sizeof(erl_cv_mat));
    if(!outemat)
        return make_error_tuple(env, "no_memory");

    if(!enif_get_resource(env, arg, erl_cv_mat_type, (void **) &inemat)) {
      cv::Mat inMat = *inemat->mat;
      outemat->mat = new cv::Mat(inMat);
    } else {
      outemat->mat = new cv::Mat();
    }
    ret = enif_make_resource(env, outemat);
    enif_release_resource(outemat);
    return make_ok_tuple(env, ret);
}

static ERL_NIF_TERM
evaluate_command(erl_cv_command *cmd, erl_cv_connection *conn)
{
    switch(cmd->type) {
    // Video Capture
      case cmd_video_capture_open:
        return do_vc_open(cmd->env, conn, cmd->arg);
      case cmd_video_capture_close:
        return do_vc_close(cmd->env, conn, cmd->arg);
      case cmd_video_capture_is_opened:
        return do_vc_is_opened(cmd->env, conn, cmd->arg);
      case cmd_video_capture_grab:
        return do_vc_grab(cmd->env, conn, cmd->arg);
      case cmd_video_capture_retrieve:
        return do_vc_retrieve(cmd->env, conn, cmd->arg);
      case cmd_video_capture_read:
        return do_vc_read(cmd->env, conn, cmd->arg);
      case cmd_video_capture_get:
        return do_vc_get(cmd->env, conn, cmd->arg);
      case cmd_video_capture_set:
        return do_vc_set(cmd->env, conn, cmd->arg);

    // Utility
      case cmd_imencode:
        return do_imencode(cmd->env, conn, cmd->arg);
      case cmd_new_mat:
        return do_new_mat(cmd->env, conn, cmd->arg);
      default:
        return make_error_tuple(cmd->env, "invalid_command");
    }
}

static ERL_NIF_TERM
push_command(ErlNifEnv *env, erl_cv_connection *conn, erl_cv_command *cmd) {
    if(!queue_push(conn->commands, cmd))
        return make_error_tuple(env, "command_push_failed");

    return make_atom(env, "ok");
}

static ERL_NIF_TERM
make_answer(erl_cv_command *cmd, ERL_NIF_TERM answer)
{
    return enif_make_tuple3(cmd->env, atom_erl_cv, cmd->ref, answer);
}

static void *
erl_cv_connection_run(void *arg)
{
    erl_cv_connection *conn = (erl_cv_connection *) arg;
    erl_cv_command *cmd;
    int continue_running = 1;

    while(continue_running) {
	    cmd = (erl_cv_command*)queue_pop(conn->commands);

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
erl_cv_start(ErlNifEnv* env, int, const ERL_NIF_TERM[])
{
    erl_cv_connection *conn;
    ERL_NIF_TERM conn_resource;

    /* Initialize the resource */
    conn = (erl_cv_connection *) enif_alloc_resource(erl_cv_type, sizeof(erl_cv_connection));
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
    if(enif_thread_create((char*) "erl_cv_connection", &conn->tid, erl_cv_connection_run, conn, conn->opts) != 0) {
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
    erl_cv_connection *conn;
    erl_cv_command *cmd = NULL;
    ErlNifPid pid;

    if(argc != 4)
	    return enif_make_badarg(env);
    if(!enif_get_resource(env, argv[0], erl_cv_type, (void **) &conn))
	    return enif_make_badarg(env);
    if(!enif_is_ref(env, argv[1]))
	    return make_error_tuple(env, "invalid_ref");
    if(!enif_get_local_pid(env, argv[2], &pid))
	    return make_error_tuple(env, "invalid_pid");
    if(!enif_is_list(env, argv[3]))
	    return make_error_tuple(env, "invalid_arg");

    /* Note, no check is made for the type of the argument */
    cmd = command_create();
    if(!cmd)
	    return make_error_tuple(env, "command_create_failed");

    cmd->type = cmd_video_capture_open;
    cmd->ref = enif_make_copy(cmd->env, argv[1]);
    cmd->pid = pid;
    cmd->arg = enif_make_copy(cmd->env, argv[3]);

    return push_command(env, conn, cmd);
}

static ERL_NIF_TERM
erl_video_capture_close(ErlNifEnv *env, int, const ERL_NIF_TERM argv[])
{
    erl_cv_connection *conn;
    erl_cv_command *cmd = NULL;
    ErlNifPid pid;

    if(!enif_get_resource(env, argv[0], erl_cv_type, (void **) &conn))
	    return enif_make_badarg(env);
    if(!enif_is_ref(env, argv[1]))
	    return make_error_tuple(env, "invalid_ref");
    if(!enif_get_local_pid(env, argv[2], &pid))
	    return make_error_tuple(env, "invalid_pid");
    if(!enif_is_ref(env, argv[3]))
        return make_error_tuple(env, "invalid_arg");

    cmd = command_create();
    if(!cmd)
	    return make_error_tuple(env, "command_create_failed");

    cmd->type = cmd_video_capture_close;
    cmd->ref = enif_make_copy(cmd->env, argv[1]);
    cmd->pid = pid;
    cmd->arg = enif_make_copy(cmd->env, argv[3]);
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
    erl_cv_connection *conn;
    erl_cv_command *cmd = NULL;
    ErlNifPid pid;

    if(argc != 4)
        return enif_make_badarg(env);
    if(!enif_get_resource(env, argv[0], erl_cv_type, (void **) &conn))
	    return enif_make_badarg(env);
    if(!enif_is_ref(env, argv[1]))
	    return make_error_tuple(env, "invalid_ref");
    if(!enif_get_local_pid(env, argv[2], &pid))
	    return make_error_tuple(env, "invalid_pid");
    if(!enif_is_ref(env, argv[3]))
        return make_error_tuple(env, "invalid_arg");

    cmd = command_create();
    if(!cmd)
	    return make_error_tuple(env, "command_create_failed");

    cmd->type = cmd_video_capture_is_opened;
    cmd->ref = enif_make_copy(cmd->env, argv[1]);
    cmd->pid = pid;
    cmd->arg = enif_make_copy(cmd->env, argv[3]);
    return push_command(env, conn, cmd);
}

/**
 * Grabs the next frame from video file or capturing device. 
 * https://docs.opencv.org/3.4.5/d8/dfe/classcv_1_1VideoCapture.html#ae38c2a053d39d6b20c9c649e08ff0146
*/
static ERL_NIF_TERM
erl_video_capture_grab(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    erl_cv_connection *conn;
    erl_cv_command *cmd = NULL;
    ErlNifPid pid;

    if(argc != 4)
        return enif_make_badarg(env);
    if(!enif_get_resource(env, argv[0], erl_cv_type, (void **) &conn))
	    return enif_make_badarg(env);
    if(!enif_is_ref(env, argv[1]))
	    return make_error_tuple(env, "invalid_ref");
    if(!enif_get_local_pid(env, argv[2], &pid))
	    return make_error_tuple(env, "invalid_pid");
    if(!enif_is_ref(env, argv[3]))
        return make_error_tuple(env, "invalid_arg");

    cmd = command_create();
    if(!cmd)
	    return make_error_tuple(env, "command_create_failed");

    cmd->type = cmd_video_capture_grab;
    cmd->ref = enif_make_copy(cmd->env, argv[1]);
    cmd->pid = pid;
    cmd->arg = enif_make_copy(cmd->env, argv[3]);
    return push_command(env, conn, cmd);
}

/**
 * grab and decode a frame
 * https://docs.opencv.org/3.2.0/d8/dfe/classcv_1_1VideoCapture.html#a9ac7f4b1cdfe624663478568486e6712
*/
static ERL_NIF_TERM
erl_video_capture_retrieve(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    erl_cv_connection *conn;
    erl_cv_command *cmd = NULL;
    ErlNifPid pid;

    if(argc != 4)
        return enif_make_badarg(env);
    if(!enif_get_resource(env, argv[0], erl_cv_type, (void **) &conn))
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

    cmd->type = cmd_video_capture_retrieve;
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
    erl_cv_connection *conn;
    erl_cv_command *cmd = NULL;
    ErlNifPid pid;

    if(argc != 4)
        return enif_make_badarg(env);
    if(!enif_get_resource(env, argv[0], erl_cv_type, (void **) &conn))
	    return enif_make_badarg(env);
    if(!enif_is_ref(env, argv[1]))
	    return make_error_tuple(env, "invalid_ref");
    if(!enif_get_local_pid(env, argv[2], &pid))
	    return make_error_tuple(env, "invalid_pid");
   if(!enif_is_ref(env, argv[3]))
        return make_error_tuple(env, "invalid_arg");

    cmd = command_create();
    if(!cmd)
	    return make_error_tuple(env, "command_create_failed");

    cmd->type = cmd_video_capture_read;
    cmd->ref = enif_make_copy(cmd->env, argv[1]);
    cmd->pid = pid;
    cmd->arg = enif_make_copy(cmd->env, argv[3]);
    return push_command(env, conn, cmd);
}

/**
 * Returns the specified VideoCapture property. 
 * https://docs.opencv.org/3.4.5/d8/dfe/classcv_1_1VideoCapture.html#aa6480e6972ef4c00d74814ec841a2939
*/
static ERL_NIF_TERM
erl_video_capture_get(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    erl_cv_connection *conn;
    erl_cv_command *cmd = NULL;
    ErlNifPid pid;

    if(argc != 4)
        return enif_make_badarg(env);
    if(!enif_get_resource(env, argv[0], erl_cv_type, (void **) &conn))
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

    cmd->type = cmd_video_capture_get;
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
    erl_cv_connection *conn;
    erl_cv_command *cmd = NULL;
    ErlNifPid pid;

    if(argc != 4)
        return enif_make_badarg(env);
    if(!enif_get_resource(env, argv[0], erl_cv_type, (void **) &conn))
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

    cmd->type = cmd_video_capture_set;
    cmd->ref = enif_make_copy(cmd->env, argv[1]);
    cmd->pid = pid;
    cmd->arg = enif_make_copy(cmd->env, argv[3]);
    return push_command(env, conn, cmd);
}

/**
 * Encode an image
 * https://docs.opencv.org/3.4/d4/da8/group__imgcodecs.html#ga26a67788faa58ade337f8d28ba0eb19e
*/
static ERL_NIF_TERM
erl_cv_imencode(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    erl_cv_connection *conn;
    erl_cv_command *cmd = NULL;
    ErlNifPid pid;

    if(argc != 4)
        return enif_make_badarg(env);
    if(!enif_get_resource(env, argv[0], erl_cv_type, (void **) &conn))
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

    cmd->type = cmd_imencode;
    cmd->ref = enif_make_copy(cmd->env, argv[1]);
    cmd->pid = pid;
    cmd->arg = enif_make_copy(cmd->env, argv[3]);
    return push_command(env, conn, cmd);
}

/**
 * Constructs a new Mat
 * https://docs.opencv.org/3.4.5/d3/d63/classcv_1_1Mat.html#af1d014cecd1510cdf580bf2ed7e5aafc
*/
static ERL_NIF_TERM
erl_cv_new_mat(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
{
    erl_cv_connection *conn;
    erl_cv_command *cmd = NULL;
    ErlNifPid pid;

    if(argc != 4)
        return enif_make_badarg(env);
    if(!enif_get_resource(env, argv[0], erl_cv_type, (void **) &conn))
	    return enif_make_badarg(env);
    if(!enif_is_ref(env, argv[1]))
	    return make_error_tuple(env, "invalid_ref");
    if(!enif_get_local_pid(env, argv[2], &pid))
	    return make_error_tuple(env, "invalid_pid");
    if(!enif_is_ref(env, argv[3]))
        return make_error_tuple(env, "invalid_arg");

    cmd = command_create();
    if(!cmd)
	    return make_error_tuple(env, "command_create_failed");

    cmd->type = cmd_new_mat;
    cmd->ref = enif_make_copy(cmd->env, argv[1]);
    cmd->pid = pid;
    cmd->arg = enif_make_copy(cmd->env, argv[3]);
    return push_command(env, conn, cmd);
}


static void
destruct_cv_connection(ErlNifEnv*, void *arg)
{
    enif_fprintf(stderr, "destruct cv_conn\r\n");
    erl_cv_connection *conn = (erl_cv_connection *) arg;
    erl_cv_command *close_cmd = command_create();


    /* Send the stop command
    */
    close_cmd->type = cmd_stop;
    queue_push(conn->commands, close_cmd);

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
destruct_cv_mat(ErlNifEnv*, void *arg)
{
    enif_fprintf(stderr, "destruct cv_mat\r\n");
    erl_cv_mat *emat = (erl_cv_mat *)arg;
    if(emat->mat) {
        delete emat->mat;
    }
}

static void
destruct_cv_video_capture(ErlNifEnv*, void *arg)
{
    enif_fprintf(stderr, "destruct cv_cap\r\n");
    erl_cv_video_capture *ecap = (erl_cv_video_capture *)arg;
    if(ecap->cap) {
        delete ecap->cap;
    }
}

/*
 * Load the nif. Initialize some stuff and such
 */
static int
on_load(ErlNifEnv* env, void**, ERL_NIF_TERM)
{
    ErlNifResourceType *rt;

    rt = enif_open_resource_type(env, "erl_cv_nif", "erl_cv_type",
				destruct_cv_connection, ERL_NIF_RT_CREATE, NULL);
    if(!rt)
	    return -1;
    erl_cv_type = rt;

    rt = enif_open_resource_type(env, "erl_cv_nif", "erl_cv_video_capture_type",
                destruct_cv_video_capture, ERL_NIF_RT_CREATE, NULL);
    if(!rt)
        return -1;
    erl_cv_video_capture_type = rt;

    rt = enif_open_resource_type(env, "erl_cv_nif", "erl_cv_mat_type",
                destruct_cv_mat, ERL_NIF_RT_CREATE, NULL);
    if(!rt)
        return -1;
    erl_cv_mat_type = rt;

    atom_erl_cv = make_atom(env, "erl_cv_nif");
    return 0;
}

static int on_reload(ErlNifEnv*, void**, ERL_NIF_TERM)
{
    return 0;
}

static int on_upgrade(ErlNifEnv*, void**, void**, ERL_NIF_TERM)
{
    return 0;
}

static ErlNifFunc nif_funcs[] = {
    // VideoCapture
    {"start", 0, erl_cv_start, 0},
    {"video_capture_open", 4, erl_video_capture_open, 0},
    {"video_capture_close", 4, erl_video_capture_close, 0},
    {"video_capture_is_opened", 4, erl_video_capture_is_opened, 0},
    {"video_capture_grab", 4, erl_video_capture_grab, 0},
    {"video_capture_retreive", 4, erl_video_capture_retrieve, 0},
    {"video_capture_read", 4, erl_video_capture_read, 0},
    {"video_capture_get", 4, erl_video_capture_get, 0},
    {"video_capture_set", 4, erl_video_capture_set, 0},
    
    // Utility
    {"imencode", 4, erl_cv_imencode, 0},
    {"new_mat", 4, erl_cv_new_mat, 0}
};
ERL_NIF_INIT(erl_cv_nif, nif_funcs, on_load, on_reload, on_upgrade, NULL);
