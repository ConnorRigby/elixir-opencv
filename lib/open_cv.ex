defmodule OpenCv do
  import OpenCv.Util

  @default_timeout 5000

  def new do 
    :erl_cv_nif.start()
  end

  def mat(conn, arg \\ nil, timeout \\ @default_timeout) do
    ref = make_ref()
    :ok = :erl_cv_nif.new_mat(conn, ref, self(), arg)
    receive_answer(ref, timeout)
  end

  def imencode(conn, mat, ext, params, timeout \\ @default_timeout) do
    ref = make_ref()
    :ok = :erl_cv_nif.imencode(conn, ref, self(), {mat, ext, params})
    receive_answer(ref, timeout)
  end

  def test do
    {:ok, conn} = OpenCv.new()
    {:ok, cap} = OpenCv.VideoCapture.open(conn, '/dev/video0')
    true = OpenCv.VideoCapture.is_opened(conn, cap)
    {:ok, frame} = OpenCv.VideoCapture.read(conn, cap)
    jpg = OpenCv.imencode(conn, frame, '.jpg', [])
    File.write("img.jpg", jpg)
  end
end
