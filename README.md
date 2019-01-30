# OpenCv

OpenCv NIF Bindings for Erlang/Elixir.

## Current status

Currently almost nothing works. Opening a video device and capturing jpeg frames
works currently.

## Usage

Capturing a jpeg encoded frame frame can be done by:

```elixir
Interactive Elixir (1.7.3) - press Ctrl+C to exit (type h() ENTER for help)
iex(1)> {:ok, conn} = OpenCv.new()
{:ok, #Reference<0.1511271170.1095630852.139975>}
iex(2)> {:ok, cap} = OpenCv.VideoCapture.open(conn, '/dev/video0')
{:ok, #Reference<0.1511271170.1095630848.138924>}
iex(3)> true = OpenCv.VideoCapture.is_opened(conn, cap)
true
iex(4)> {:ok, frame} = OpenCv.VideoCapture.read(conn, cap)
{:ok, #Reference<0.1511271170.1095630848.138925>}
iex(5)> jpg = OpenCv.imencode(conn, frame, '.jpg', [])
<<255, 216, 255, 224, 0, 16, 74, 70, 73, 70, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 255,
  219, 0, 67, 0, 2, 1, 1, 1, 1, 1, 2, 1, 1, 1, 2, 2, 2, 2, 2, 4, 3, 2, 2, 2, 2,
  5, 4, 4, 3, ...>>
iex(6)> File.write("img.jpg", jpg)
:ok

```

## Building

This currently supports opencv3 and opencv4. It may support opencv2, but I have
not tested. Nerves builds are currently supported given you have a
system that has opencv installed. [this](https://github.com/FarmBot-Labs/farmbot_system_rpi3)
system has opencv 3 installed. I've only tested build on linux, and it is likely
that paths for the Makefile may be wrong.

## Installation

To pull in this package directly from GitHub, amend your list of
dependencies in `mix.exs` as follows:

```elixir
def deps do
  [
    {:open_cv, github: "ConnorRigby/elixir-opencv", branch: "master"}
  ]
end
```

If [available in Hex](https://hex.pm/docs/publish), the package can be installed
by adding `open_cv` to your list of dependencies in `mix.exs`:

```elixir
def deps do
  [
    {:open_cv, "~> 0.1.0"}
  ]
end
```

Documentation can be generated with [ExDoc](https://github.com/elixir-lang/ex_doc)
and published on [HexDocs](https://hexdocs.pm). Once published, the docs can
be found at [https://hexdocs.pm/open_cv](https://hexdocs.pm/open_cv).
