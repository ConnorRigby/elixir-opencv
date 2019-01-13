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
  def open(_conn, _ref, _pid, _filename), do: :erlang.nif_error("erl_video_capture not loaded")
  def close(_conn, _ref, _pid), do: :erlang.nif_error("erl_video_capture not loaded")
  def is_opened(_conn, _ref, _pid), do: :erlang.nif_error("erl_video_capture not loaded")
  def grab(_conn, _ref, _pid), do: :erlang.nif_error("erl_video_capture not loaded")
  def retreive(_conn, _ref, _pid, _flag), do: :erlang.nif_error("erl_video_capture not loaded")
  def read(_conn, _ref, _pid), do: :erlang.nif_error("erl_video_capture not loaded")
  def get(_conn, _ref, _pid, _propid), do: :erlang.nif_error("erl_video_capture not loaded")
  def set(_conn, _ref, _pid, _propid_value), do: :erlang.nif_error("erl_video_capture not loaded")
  def imencode(_mat, _ext, _params), do: :erlang.nif_error("nif not loaded")
end
