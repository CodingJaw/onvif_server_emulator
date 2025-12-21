#pragma once

#include "Logger.h"

#include <memory>
#include <string>

namespace osrv
{
    namespace event
    {
        class ExternalToggleEventGenerator;

        // Register the generator so external triggers can invoke it.
        void RegisterExternalToggleGenerator(const std::shared_ptr<ExternalToggleEventGenerator>& generator);

        // Trigger the generator when registered. No-op if unavailable.
        void ExternalToggleTrigger(bool state);

        // Returns true when a generator has been registered.
        bool HasExternalToggleGenerator();

        // Start/stop a TCP listener that accepts ASCII ON/OFF messages and triggers the generator.
        void StartExternalToggleSocket(uint16_t port, const std::string& address, const ILogger& logger);
        void StopExternalToggleSocket();
    }
}

extern "C"
{
        // C ABI shim that allows foreign callers to toggle the generator directly.
        void osrv_external_toggle_trigger(int state);
        int osrv_external_toggle_available();
}

