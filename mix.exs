defmodule OpenCv.MixProject do
  use Mix.Project

  def project do
    [
      app: :open_cv,
      version: "0.1.0",
      elixir: "~> 1.7",
      start_permanent: Mix.env() == :prod,
      make_clean: ["clean"],
      make_env: make_env(),
      make_cwd: __DIR__,
      compilers: [:elixir_make] ++ Mix.compilers(),
      deps: deps()
    ]
  end

  # Run "mix help compile.app" to learn about applications.
  def application do
    [
      extra_applications: [:logger]
    ]
  end

  # Run "mix help deps" to learn about dependencies.
  defp deps do
    [
      {:elixir_make, "~> 0.4", runtime: false}
    ]
  end

  defp make_env do
    case System.get_env("ERL_EI_INCLUDE_DIR") do
      nil ->
        %{
          "MAKE_CWD" => __DIR__,
          "MIX_TARGET" => System.get_env("MIX_TARGET") || "host",
          "ERL_EI_INCLUDE_DIR" => Path.join([:code.root_dir(), "usr", "include"]),
          "ERL_EI_LIBDIR" => Path.join([:code.root_dir(), "usr", "lib"])
        }

      _ ->
        %{"MAKE_CWD" => __DIR__, "MIX_TARGET" => System.get_env("MIX_TARGET") || "host"}
    end
  end
end
