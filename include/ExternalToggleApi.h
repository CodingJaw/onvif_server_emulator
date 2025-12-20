#pragma once

#include "HttpServerFwd.h"
#include "Logger.h"
#include "onvif_services/pullpoint/event_generators.h"

namespace osrv
{
/**
 * Register a lightweight HTTP endpoint that triggers the ExternalToggle event generator.
 *
 * The endpoint accepts POST requests with a JSON payload containing a boolean field named
 * "state". When invoked, it forwards the provided state to the generator's Trigger method
 * so the value is broadcast to connected PullPoint subscribers.
 */
void register_external_toggle_api(HttpServer& server,
        std::shared_ptr<event::ExternalToggleEventGenerator> generator,
        const ILogger& logger);
} // namespace osrv
