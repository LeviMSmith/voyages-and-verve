# Yellow Copper

Fully simulated pixel world

## Building and running

Configure cmake:

`cmake =S . -B ./build`

Build the generated cmake config

`cmake --build ./build`

Now the executable is build in, likely, build/yellow-copper

## Progam structure

The idea is full data oriented design. No shuffling papers
with classes. Global variables are a yes. All in one translation unit
so that anything can be used anywhere and modified to be what it needs to be.

Memory should be allocated as much as possible ahead of time. Avoid VLAs.
Standard library is used to accelerate development, but ideally, things like
maps especially should be speciallized to their cases.
