# OpenCv

OpenCv NIF Bindings for Erlang/Elixir.

## Current status

Currently almost nothing works. Opening a video device and capturing jpeg frames
works currently.

## Usage

Capturing a jpeg encoded frame frame can be done by:

```elixir
iex()> {:ok, cap} = OpenCv.VideoCapture.open('/dev/video0')
{:ok,
 {OpenCv.VideoCapture, #Reference<0.3761111453.805568514.61221>,
  #Reference<0.3761111453.805699586.61219>}}
iex()> frame = OpenCv.VideoCapture.open(cap)
<<255, 216, 255, 224, 0, 16, 74, 70, 73, 70, 0, 1, 1, 0, 0, 1, 0, 1, 0, 0, 255,
  219, 0, 67, 0, 6, 4, 5, 6, 5, 4, 6, 6, 5, 6, 7, 7, 6, 8, 10, 16, 10, 10, 9, 9,
  10, 20, 14, 15, 12, ...>>
```

You can preview the frame by:

```elixir
iex()> File.write("/tmp/frame.jpg", frame)
:ok
```

## Building

This currently supports opencv3 and opencv4. It may support opencv2, but I have 
not tested. Nerves builds are currently supported given you have a 
system that has opencv installed. [this](https://github.com/FarmBot-Labs/farmbot_system_rpi3) 
system has opencv 3 installed. I've only tested build on linux, and it is likely
that paths for the Makefile may be wrong.

## Installation

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

