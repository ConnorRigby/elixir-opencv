# OpenCv

OpenCv NIF Bindings for Erlang/Elixir.

## Current status

Currently almost nothing works. Opening a video device and capturing jpeg frames
works currently.

## Usage

Capturing a jpeg encoded frame frame can be done by:

```elixir
Interactive Elixir (1.7.3) - press Ctrl+C to exit (type h() ENTER for help)
iex(1)> {:ok, cap} = OpenCv.VideoCapture.open('/dev/video0')                      
{:ok,
 {OpenCv.VideoCapture, #Reference<0.2545925687.771227649.125639>,
  #Reference<0.2545925687.771358721.125637>}}
iex(2)> {:ok, frame} = OpenCv.VideoCapture.read(cap)                              
{:ok, #Reference<0.2545925687.771358720.123753>}
iex(3)> File.write!("img.jpg", OpenCv.VideoCaptureNif.imencode(frame, '.jpg', []))
:ok
iex(4)> 
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

