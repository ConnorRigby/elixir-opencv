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

typedef enum {
    cmd_unknown,
    cmd_open,
    cmd_close,
    cmd_stop,
    cmd_get_frame
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

/*
 *
 */
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
do_close(ErlNifEnv *env, erl_video_capture_connection *conn, const ERL_NIF_TERM arg)
{
    if(conn->cap) {
        conn->cap->release();
        conn->cap = NULL;
    }
    return make_atom(env, "ok");
}

static ERL_NIF_TERM
do_get_frame(ErlNifEnv *env, erl_video_capture_connection *conn, const ERL_NIF_TERM arg)
{
    double rotation_angle;
    cv::Mat inFrame, outFrame, matrix;
    cv::VideoCapture cap;

    if(!enif_get_double(env, arg, &rotation_angle))
        return enif_make_badarg(env);

    if(conn->cap == NULL)
        return make_atom(env, "nil");

    // Store the capture device on the conn struct
    cap = *conn->cap;

    if(!cap.isOpened())
        return make_atom(env, "nil");

    // Capture a frame
    cap >> inFrame;

    if(inFrame.empty()) 
        return make_atom(env, "nil");

    int sign;
    if(rotation_angle < 0) {
        sign = -1;
    } else {
        sign = 1;
    }

    int turns = rotation_angle / 90;
    int remainder = (int)abs(rotation_angle) % 90;
    if(remainder > 45)
        turns -= 1 * sign;
    rotation_angle += 90 * turns;

    int width, height;
    width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
    height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
    cv::Point2f center = cv::Point2f((width / 2), (height / 2)); 
    matrix = cv::getRotationMatrix2D(center, rotation_angle, 1.0);
    cv::warpAffine(inFrame, outFrame, matrix, cv::Size(width, height));

    //buffer for storing frame
    std::vector<uchar> buff;
    
    std::vector<int> param = {cv::IMWRITE_JPEG_QUALITY, 80};
    cv::imencode(".jpg", outFrame, buff, param);
    return make_binary(env, buff.data(), buff.size());
}

static ERL_NIF_TERM
evaluate_command(erl_video_capture_command *cmd, erl_video_capture_connection *conn)
{
    switch(cmd->type) {
      case cmd_open:
        return do_open(cmd->env, conn, cmd->arg);
      case cmd_close:
        return do_close(cmd->env, conn, cmd->arg);
      case cmd_get_frame:
        return do_get_frame(cmd->env, conn, cmd->arg);
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

static ERL_NIF_TERM
erl_video_capture_get_frame(ErlNifEnv *env, int argc, const ERL_NIF_TERM argv[])
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
	    return enif_make_badarg(env);

    cmd = command_create();
    if(!cmd)
	    return make_error_tuple(env, "command_create_failed");

    cmd->type = cmd_get_frame;
    cmd->ref = enif_make_copy(cmd->env, argv[1]);
    cmd->pid = pid;
    cmd->arg = enif_make_copy(cmd->env, argv[3]);

    return push_command(env, conn, cmd);
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
    {"get_frame", 4, erl_video_capture_get_frame}
};

ERL_NIF_INIT(Elixir.OpenCv.VideoCaptureNif, nif_funcs, on_load, on_reload, on_upgrade, NULL);
