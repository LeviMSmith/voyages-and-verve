# Voyages & Verve

Fully simulated pixel world

## Building and running

Should build and run anywhere SDL2 will compile for, no extra configuration
required.

### Build dependancies

All code dependancies should be in vendor and built automatically by cmake,
so you'll just need that and a compiler.

That, and make sure that after cloning this repo, you'll also need to grab 
the submodules:

`git submodule update --init --recursive`

- [cmake](https://cmake.org/download/)

### Building

Configure cmake:

`cmake =S . -B ./build -DCMAKE_BUILD_TYPE=Debug`

Build the generated cmake config

`cmake --build ./build`

Now the executable is built in, likely, build/voyages-and-verve
