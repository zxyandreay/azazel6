#include "presenter.hpp"
#include "azazel6/input.hpp"
#include <SDL3/SDL_main.h>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace {
struct App {
    std::mutex mutex;
    Presenter presenter;
    azazel6::InputState input;
    Uint64 input_cutoff = 0;
    bool paused = false, failed = false;
};

// Android requires immediate lifecycle handling through an event watch: the
// normal event pump can be blocked by the platform while the activity is paused.
bool SDLCALL lifecycle(void* userdata, SDL_Event* event) {
    if (event->type != SDL_EVENT_WILL_ENTER_BACKGROUND &&
        event->type != SDL_EVENT_DID_ENTER_FOREGROUND) return true;
    auto& app = *static_cast<App*>(userdata);
    std::lock_guard lock(app.mutex);
    try {
        app.input.clear();
        app.input_cutoff = SDL_GetTicksNS();
        if (event->type == SDL_EVENT_WILL_ENTER_BACKGROUND) {
            app.paused = true;
            app.presenter.suspend();
            SDL_Log("Host probe suspended; input cleared");
        } else {
            app.paused = false;
            app.presenter.resize();
            SDL_Log("Host probe foreground; surface recreation pending");
        }
    } catch (const std::exception& error) {
        app.failed = true;
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Lifecycle failure: %s", error.what());
    }
    return true;
}
struct Watch {
    explicit Watch(App& app) : app_(app) {
        if (!SDL_AddEventWatch(lifecycle, &app_)) throw std::runtime_error(SDL_GetError());
    }
    ~Watch() { SDL_RemoveEventWatch(lifecycle, &app_); }
    App& app_;
};
using Gamepad = std::unique_ptr<SDL_Gamepad, decltype(&SDL_CloseGamepad)>;
void connect(Gamepad& pad, SDL_JoystickID preferred = 0) {
    if (preferred) {
        Gamepad replacement(SDL_OpenGamepad(preferred), SDL_CloseGamepad);
        if (replacement) pad = std::move(replacement);
        if (pad) SDL_Log("Gamepad selected: %s", SDL_GetGamepadName(pad.get()));
        return;
    }
    if (pad) return;
    int count = 0;
    auto* ids = SDL_GetGamepads(&count);
    for (int i = 0; ids && i < count && !pad; ++i) pad.reset(SDL_OpenGamepad(ids[i]));
    SDL_free(ids);
    if (pad) SDL_Log("Gamepad connected: %s", SDL_GetGamepadName(pad.get()));
}
azazel6::PadSample sample(SDL_Gamepad* pad) {
    using namespace azazel6;
    PadSample result;
    if (!pad || !SDL_GamepadConnected(pad)) return result;
    const std::pair<SDL_GamepadButton, std::uint32_t> buttons[] = {
        {SDL_GAMEPAD_BUTTON_SOUTH, cross}, {SDL_GAMEPAD_BUTTON_EAST, circle},
        {SDL_GAMEPAD_BUTTON_WEST, square}, {SDL_GAMEPAD_BUTTON_NORTH, triangle},
        {SDL_GAMEPAD_BUTTON_BACK, azazel6::select}, {SDL_GAMEPAD_BUTTON_START, start},
        {SDL_GAMEPAD_BUTTON_DPAD_UP, up}, {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, right},
        {SDL_GAMEPAD_BUTTON_DPAD_DOWN, down}, {SDL_GAMEPAD_BUTTON_DPAD_LEFT, left},
        {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, ltrigger}, {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, rtrigger}
    };
    for (auto [host, guest] : buttons) if (SDL_GetGamepadButton(pad, host)) result.buttons |= guest;
    result.x = analog_axis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX));
    result.y = analog_axis(SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY));
    return result;
}
}

int main(int, char**) {
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) { SDL_Log("%s", SDL_GetError()); return 1; }
    int status = 0;
    {
        std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
            SDL_CreateWindow("Azazel6 Host Probe (synthetic)", 960, 544,
                             SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE), SDL_DestroyWindow);
        try {
            if (!window) throw std::runtime_error(SDL_GetError());
            App app;
            Watch watch(app); // Removed before app/presenter/window destruction.
            Gamepad pad(nullptr, SDL_CloseGamepad);
            connect(pad);
            bool running = true;
            bool initialized = false;
            bool focused = (SDL_GetWindowFlags(window.get()) & SDL_WINDOW_INPUT_FOCUS) != 0;
            std::uint32_t last_buttons = 0;
            std::size_t last_contacts = 0;
            std::uint64_t frames = 0;
            SDL_Log("Synthetic host probe started; no guest code, rendering or clock");
            while (running) {
                SDL_Event event;
                // Never hold app.mutex while pumping SDL: lifecycle watches may run here.
                while (SDL_PollEvent(&event)) {
                    // Opening/closing pads can itself dispatch SDL events. Keep
                    // that work outside the lifecycle lock to avoid lock inversion.
                    if (event.type == SDL_EVENT_GAMEPAD_ADDED) connect(pad, event.gdevice.which);
                    if (event.type == SDL_EVENT_GAMEPAD_REMOVED && pad &&
                        SDL_GetGamepadID(pad.get()) == event.gdevice.which) {
                        pad.reset();
                        connect(pad);
                    }
                    std::lock_guard lock(app.mutex);
                    switch (event.type) {
                    case SDL_EVENT_QUIT: running = false; break;
                    case SDL_EVENT_WINDOW_FOCUS_LOST:
                        focused = false; app.input.clear(); app.input_cutoff = SDL_GetTicksNS(); break;
                    case SDL_EVENT_WINDOW_FOCUS_GAINED:
                        focused = true; app.input.clear(); app.input_cutoff = SDL_GetTicksNS(); break;
                    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    case SDL_EVENT_WINDOW_SAFE_AREA_CHANGED:
                        app.input.clear(); app.input_cutoff = SDL_GetTicksNS(); app.presenter.resize(); break;
                    case SDL_EVENT_GAMEPAD_REMOVED:
                    case SDL_EVENT_GAMEPAD_REMAPPED:
                        app.input.clear();
                        break;
                    case SDL_EVENT_FINGER_UP:
                    case SDL_EVENT_FINGER_CANCELED:
                        app.input.release(event.tfinger.touchID, event.tfinger.fingerID); break;
                    case SDL_EVENT_FINGER_DOWN:
                    case SDL_EVENT_FINGER_MOTION: {
                        if (!focused || app.paused || event.tfinger.timestamp <= app.input_cutoff) break;
                        int w = 0, h = 0;
                        SDL_Rect safe{};
                        if (SDL_GetWindowSize(window.get(), &w, &h) && SDL_GetWindowSafeArea(window.get(), &safe))
                            app.input.touch(event.tfinger.touchID, event.tfinger.fingerID,
                                event.tfinger.x * w, event.tfinger.y * h,
                                static_cast<float>(safe.x), static_cast<float>(safe.y),
                                static_cast<float>(safe.w), static_cast<float>(safe.h),
                                event.type == SDL_EVENT_FINGER_DOWN);
                        else app.input.clear();
                        break;
                    }
                    default: break;
                    }
                }
                {
                    std::lock_guard lock(app.mutex);
                    if (app.failed) throw std::runtime_error("Android lifecycle failed");
                    if (!running) break;
                    if (!app.paused) {
                        if (!initialized) {
                            app.presenter.initialize(window.get());
                            initialized = true;
                        }
                        app.input.pad = focused ? sample(pad.get()) : azazel6::PadSample{};
                        std::size_t contacts = 0;
                        for (const auto& touch : app.input.touches) if (touch) ++contacts;
                        if (last_buttons != app.input.pad.buttons || last_contacts != contacts) {
                            SDL_Log("Host input: buttons=0x%08x contacts=%zu", app.input.pad.buttons, contacts);
                            last_buttons = app.input.pad.buttons; last_contacts = contacts;
                        }
                        if (app.presenter.present(app.input.pad.buttons ? 0.5f : 0.05f, contacts ? 0.5f : 0.08f)) {
                            if (++frames == 1) SDL_Log("First synthetic frame presented");
                            if (frames % 120 == 0)
                                SDL_Log("Synthetic progress: frames=%llu", static_cast<unsigned long long>(frames));
                        }
                    }
                }
                SDL_Delay(1); // Host event-loop yield, never guest simulation time.
            }
            SDL_Log("Host probe exit: synthetic frames=%llu", static_cast<unsigned long long>(frames));
        } catch (const std::exception& error) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Host probe failure: %s", error.what());
            status = 1;
        }
    }
    SDL_Quit();
    return status;
}
