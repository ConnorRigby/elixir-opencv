defmodule :erl_cv_nif do
  @moduledoc false
  @on_load :load_nif
  def load_nif do
    require Logger
    nif_file = '#{:code.priv_dir(:open_cv)}/erl_cv_nif'

    case :erlang.load_nif(nif_file, 0) do
      :ok -> :ok
      {:error, {:reload, _}} -> :ok
      {:error, reason} -> Logger.warn("Failed to load nif: #{inspect(reason)}")
    end
  end

  def start(), do: :erlang.nif_error("erl_video_capture not loaded")

  # Video Capture
  def video_capture_open(_conn, _ref, _pid, _filename), do: :erlang.nif_error("erl_video_capture not loaded")
  def video_capture_close(_conn, _ref, _pid, _cap), do: :erlang.nif_error("erl_video_capture not loaded")
  def video_capture_is_opened(_conn, _ref, _pid, _cap), do: :erlang.nif_error("erl_video_capture not loaded")
  def video_capture_grab(_conn, _ref, _pid, _cap), do: :erlang.nif_error("erl_video_capture not loaded")
  def video_capture_retreive(_conn, _ref, _pid, _cap_flag), do: :erlang.nif_error("erl_video_capture not loaded")
  def video_capture_read(_conn, _ref, _pid, _cap), do: :erlang.nif_error("erl_video_capture not loaded")
  def video_capture_get(_conn, _ref, _pid, _cap_propid), do: :erlang.nif_error("erl_video_capture not loaded")
  def video_capture_set(_conn, _ref, _pid, _cap_propid_value), do: :erlang.nif_error("erl_video_capture not loaded")

  def imencode(_mat, _ref, _pid, _ext_params), do: :erlang.nif_error("nif not loaded")
  def new_mat(_mat, _ref, _pid, _arg), do: :erlang.nif_error("nif not loaded")
end
