#pragma once

#include "../HttpServerFwd.h"
#include "pullpoint/event_generators.h"

#include <chrono>
#include <optional>
#include <string>

class ILogger;

namespace osrv
{
struct ServerConfigs;

namespace event
{
void init_service(HttpServer& /*srv*/, const osrv::ServerConfigs& /*configs*/, const std::string& /*configs_path*/,
                                                                        ILogger& /*logger*/);

// Cell motion state is updated by external controllers (e.g. Rust glue or the REST API),
// and exposed here so those integrations do not have to reach into the generator directly.
std::optional<osrv::event::CellMotionEventGenerator::MotionState> get_cell_motion_state();

bool set_cell_motion_state(bool enabled, bool state, std::chrono::seconds delay);
}
} // namespace osrv