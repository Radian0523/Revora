#include "Window.h"

#include <SDL.h>
#include <SDL_vulkan.h>

namespace Revora {

Window::~Window() {
    Shutdown();
}

bool Window::Initialize(const WindowDesc& desc) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        return false;
    }

    window_ = SDL_CreateWindow(
        desc.title,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        desc.width,
        desc.height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN
    );

    if (!window_) {
        SDL_Quit();
        return false;
    }

    width_  = desc.width;
    height_ = desc.height;
    return true;
}

void Window::Shutdown() {
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
        SDL_Quit();
    }
}

bool Window::PumpEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return true;
        }
    }
    return false;
}

} // namespace Revora
