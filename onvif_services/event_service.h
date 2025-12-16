#pragma once

#include "../HttpServerFwd.h"

#include <string>
#include <optional>
#include <vector>
#include <chrono>

class ILogger;

namespace osrv
{
struct ServerConfigs;

namespace event
{
void init_service(HttpServer& /*srv*/, const osrv::ServerConfigs& /*configs*/, const std::string& /*configs_path*/,
                                                                        ILogger& /*logger*/);

struct MotionState
{
        std::string token;
        bool enabled;
        bool state;
};

std::vector<MotionState> get_motion_states();
std::optional<MotionState> update_motion_state(const std::string& /*token*/, std::optional<bool> /*enabled*/, std::optional<bool> /*state*/, std::optional<std::chrono::seconds> /*active_duration*/);
}
} // namespace osrv