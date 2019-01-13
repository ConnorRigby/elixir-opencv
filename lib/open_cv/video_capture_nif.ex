defmodule OpenCv.VideoCaptureNif do
  @moduledoc false
  @on_load :load_nif
  def load_nif do
    require Logger
    nif_file = '#{:code.priv_dir(:open_cv)}/erl_video_capture_nif'

    case :erlang.load_nif(nif_file, 0) do
      :ok -> :ok
      {:error, {:reload, _}} -> :ok
      {:error, reason} -> Logger.warn("Failed to load nif: #{inspect(reason)}")
    end
  end

  def start(), do: :erlang.nif_error("erl_video_capture not loaded")
  def open(_, _, _, _), do: :erlang.nif_error("erl_video_capture not loaded")
  def close(_, _, _), do: :erlang.nif_error("erl_video_capture not loaded")
  def get_frame(_, _, _, _rotation), do: :erlang.nif_error("erl_video_capture not loaded")
end