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

std::optional<osrv::event::CellMotionEventGenerator::MotionState> get_cell_motion_state();

bool set_cell_motion_state(bool enabled, bool state, std::chrono::seconds delay);
}
} // namespace osrv