#include "libultraship/bridge/windowbridge.h"
#include "ship/window/Window.h"
#include "ship/Context.h"

#include <chrono>
#include <thread>

extern "C" {

uint32_t WindowGetWidth() {
    return Ship::Context::GetRawInstance()->GetWindow()->GetWidth();
}

uint32_t WindowGetHeight() {
    return Ship::Context::GetRawInstance()->GetWindow()->GetHeight();
}

float WindowGetAspectRatio() {
    return Ship::Context::GetRawInstance()->GetWindow()->GetCurrentAspectRatio();
}

bool WindowIsRunning() {
    return Ship::Context::GetRawInstance()->GetWindow()->IsRunning();
}

bool WindowIsFrameReady() {
    auto window = Ship::Context::GetRawInstance()->GetWindow();
    window->HandleEvents();

    const bool ready = window->IsRunning() && window->IsFrameReady();
    if (!ready) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    return ready;
}

int32_t WindowGetPosX() {
    return Ship::Context::GetRawInstance()->GetWindow()->GetPosX();
}

int32_t WindowGetPosY() {
    return Ship::Context::GetRawInstance()->GetWindow()->GetPosY();
}

bool WindowIsFullscreen() {
    return Ship::Context::GetRawInstance()->GetWindow()->IsFullscreen();
}
}
