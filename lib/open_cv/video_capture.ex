defmodule OpenCv.VideoCapture do
  require Logger

  @default_timeout 5000
  alias OpenCv.VideoCaptureNif
  def open(devpath, timeout \\ @default_timeout) do
    {:ok, conn} = VideoCaptureNif.start()
    ref = make_ref()
    :ok = VideoCaptureNif.open(conn, ref, self(), devpath)

    case receive_answer(ref, timeout) do
      :ok -> {:ok, {__MODULE__, make_ref(), conn}}
      err -> err
    end
  end

  def close({__MODULE__, _ref, conn}, timeout \\ @default_timeout) do
    ref = make_ref()
    :ok = VideoCaptureNif.close(conn, ref, self())
    receive_answer(ref, timeout)
  end

  def get_frame({__MODULE__, _ref, conn}, rotation \\ 0.0, timeout \\ @default_timeout) do
    ref = make_ref()
    :ok = VideoCaptureNif.get_frame(conn, ref, self(), rotation)
    receive_answer(ref, timeout)
  end

  defp receive_answer(ref, timeout) do
    start = :os.timestamp()

    receive do
      {:erl_video_capture, ^ref, resp} ->
        resp

      {:erl_video_capture, _, _} = stale ->
        Logger.error("Stale answer: #{inspect(stale)}")
        passed_mics = :timer.now_diff(:os.timestamp(), start) |> div(1000)

        new_timeout =
          case timeout - passed_mics do
            passed when passed < 0 -> 0
            to -> to
          end

        receive_answer(ref, new_timeout)
    after
      timeout -> {:error, {:timeout, ref}}
    end
  end
end