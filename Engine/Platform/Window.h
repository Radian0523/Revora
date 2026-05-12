#pragma once

#include <cstdint>

struct SDL_Window;

namespace Revora {

struct WindowDesc {
    const char* title  = "Revora";
    int         width  = 1280;
    int         height = 720;
};

class Window {
public:
    Window() = default;
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool Initialize(const WindowDesc& desc);
    void Shutdown();

    /// SDL イベントキューを消化する。ウィンドウ閉じ要求があれば true を返す。
    bool PumpEvents();

    /// Vulkan サーフェス生成に必要な SDL_Window ポインタを返す
    SDL_Window* GetSDLWindow() const { return window_; }

    int   GetWidth() const  { return width_; }
    int   GetHeight() const { return height_; }

private:
    SDL_Window* window_ = nullptr;
    int         width_  = 0;
    int         height_ = 0;
};

} // namespace Revora
