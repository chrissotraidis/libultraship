#include "ship/controller/physicaldevice/SDLAddRemoveDeviceEventHandler.h"
#include <SDL2/SDL.h>
#include "ship/Context.h"
#include "ship/controller/controldeck/ControlDeck.h"

namespace Ship {

SDLAddRemoveDeviceEventHandler::~SDLAddRemoveDeviceEventHandler() {
}

void SDLAddRemoveDeviceEventHandler::InitElement() {
}

void SDLAddRemoveDeviceEventHandler::DrawElement() {
}

void SDLAddRemoveDeviceEventHandler::UpdateElement() {
    SDL_PumpEvents();
    auto manager = Context::GetInstance()->GetControlDeck()->GetConnectedPhysicalDeviceManager();
    uint32_t now = SDL_GetTicks();
    if (!mDidInitialReconcile) {
        manager->ReconcileConnectedSDLGamepads("startup");
        mDidInitialReconcile = true;
        mLastReconcileTicks = now;
    }
    SDL_Event event;
    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_CONTROLLERDEVICEADDED, SDL_CONTROLLERDEVICEADDED) > 0) {
        // from https://wiki.libsdl.org/SDL2/SDL_ControllerDeviceEvent: which - the joystick device index for
        // the SDL_CONTROLLERDEVICEADDED event
        manager->HandlePhysicalDeviceConnect(event.cdevice.which);
    }

    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_CONTROLLERDEVICEREMOVED, SDL_CONTROLLERDEVICEREMOVED) > 0) {
        // from https://wiki.libsdl.org/SDL2/SDL_ControllerDeviceEvent: which - the [...] instance id for the
        // SDL_CONTROLLERDEVICEREMOVED [...] event
        manager->HandlePhysicalDeviceDisconnect(event.cdevice.which);
    }

    while (SDL_PeepEvents(&event, 1, SDL_GETEVENT, SDL_CONTROLLERDEVICEREMAPPED, SDL_CONTROLLERDEVICEREMAPPED) > 0) {
        manager->ReconcileConnectedSDLGamepads("remap event");
    }

    if (static_cast<uint32_t>(now - mLastReconcileTicks) >= 1000) {
        manager->ReconcileConnectedSDLGamepads("active periodic check");
        mLastReconcileTicks = now;
    }
}
} // namespace Ship
