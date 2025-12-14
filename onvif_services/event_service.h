#pragma once

#include "../HttpServerFwd.h"

#include <map>
#include <string>

class ILogger;

namespace osrv
{
struct ServerConfigs;

namespace event
{
class NotificationsManager;

void init_service(HttpServer& /*srv*/, const osrv::ServerConfigs& /*configs*/, const std::string& /*configs_path*/,
                                                                        ILogger& /*logger*/);

NotificationsManager* GetNotificationsManager();
const std::map<std::string, std::string>& GetEventXmlNamespaces();
}
} // namespace osrv