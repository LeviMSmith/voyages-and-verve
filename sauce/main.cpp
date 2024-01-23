#include "SDL.h"
#include <iostream>

int main(int argc, char* argv[]) {
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return -1;
    }

    // Create window
    SDL_Window* window = SDL_CreateWindow(
        "Hello World",               // window title
        SDL_WINDOWPOS_UNDEFINED,     // initial x position
        SDL_WINDOWPOS_UNDEFINED,     // initial y position
        800,                         // width, in pixels
        600,                         // height, in pixels
        SDL_WINDOW_SHOWN             // flags - window is visible
    );

    if (window == nullptr) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }

    // Get window surface
    SDL_Surface* screenSurface = SDL_GetWindowSurface(window);

    // Fill the surface white
    SDL_FillRect(screenSurface, nullptr, SDL_MapRGB(screenSurface->format, 255, 255, 255));

    // Update the surface
    SDL_UpdateWindowSurface(window);

    // Wait two seconds
    SDL_Delay(2000);

    // Destroy window
    SDL_DestroyWindow(window);

    // Quit SDL subsystems
    SDL_Quit();

    return 0;
}
