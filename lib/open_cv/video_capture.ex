defmodule OpenCv.VideoCapture do
  require Logger
  import OpenCv.Util

  @default_timeout 5000

  def open(conn, devpath, timeout \\ @default_timeout) do
    ref = make_ref()
    :ok = :erl_cv_nif.video_capture_open(conn, ref, self(), devpath)
    receive_answer(ref, timeout)
  end

  def close(conn, cap, timeout \\ @default_timeout) do
    ref = make_ref()
    :ok = :erl_cv_nif.video_capture_close(conn, ref, self(), cap)
    receive_answer(ref, timeout)
  end

  def is_opened(conn, cap, timeout \\ @default_timeout) do
    ref = make_ref()
    :ok = :erl_cv_nif.video_capture_is_opened(conn, ref, self(), cap)
    receive_answer(ref, timeout)
  end

  def grab(conn, cap, timeout \\ @default_timeout) do
    ref = make_ref()
    :ok = :erl_cv_nif.video_capture_grab(conn, ref, self(), cap)
    receive_answer(ref, timeout)
  end

  def retreive(conn, cap, flag \\ 0, timeout \\ @default_timeout) do
    ref = make_ref()
    :ok = :erl_cv_nif.video_capture_retreive(conn, ref, self(), {cap, flag})
    receive_answer(ref, timeout)
  end

  def read(conn, cap, timeout \\ @default_timeout) do
    ref = make_ref()
    :ok = :erl_cv_nif.video_capture_read(conn, ref, self(), cap)
    receive_answer(ref, timeout)
  end

  def get(conn, cap, propid, timeout \\ @default_timeout) do
    ref = make_ref()
    :ok = :erl_cv_nif.video_capture_get(conn, ref, self(), {cap, propid})
    receive_answer(ref, timeout)
  end

  def set(conn, cap, propid, propval, timeout \\ @default_timeout) do
    ref = make_ref()
    :ok = :erl_cv_nif.video_capture_set(conn, ref, self(), {cap, propid, propval})
    receive_answer(ref, timeout)
  end
end
