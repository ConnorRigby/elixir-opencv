defmodule OpenCv.Util do
  require Logger

  def receive_answer(ref, timeout) do
    start = :os.timestamp()

    receive do
      {:erl_cv_nif, ^ref, resp} ->
        resp

      {:erl_cv_nif, _, _} = stale ->
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
