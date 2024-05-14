#CXXFLAGS := -g -IC:msys64\mingw64\include\SDL2
#LDFLAGS := -LC:msys64\mingw64\lib -lSDL2_mixer

.PHONY: all configure build run

# Default target executed when no arguments are given to make.
all: configure build run

# Target for configuring the project with CMake.
configure:
	cmake -S . -B ./build -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release
# Can change Debug to Release

# Target for building the project using the previously generated CMake configuration.
build:
	cmake --build ./build

# Target for running the executable.
run: build
	./build/Windows/Release/./voyages-and-verve
# Can change Debug to Release

# Optionally, you can include a clean target to remove build artifacts.
clean:
	rm -rf ./build
