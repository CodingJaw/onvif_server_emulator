#include "../include/ExternalToggleApi.h"

#include "../Simple-Web-Server/server_http.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <optional>
#include <sstream>
#include <string>

namespace
{
        std::optional<bool> parse_state(const std::string& body, const ILogger& logger)
        {
                namespace pt = boost::property_tree;

                std::stringstream stream(body);
                pt::ptree tree;

                try
                {
                        pt::read_json(stream, tree);
                }
                catch (const std::exception& e)
                {
                        logger.Warn(std::string("Failed to parse external toggle payload: ") + e.what());
                        return std::nullopt;
                }

                if (auto direct = tree.get_optional<bool>("state"))
                        return *direct;

                auto as_string = tree.get_optional<std::string>("state");
                if (!as_string)
                        return std::nullopt;

                auto lowered = *as_string;
                boost::algorithm::to_lower(lowered);

                if (lowered == "true" || lowered == "1")
                        return true;

                if (lowered == "false" || lowered == "0")
                        return false;

                return std::nullopt;
        }
} // namespace

namespace osrv
{
void register_external_toggle_api(HttpServer& server, std::shared_ptr<event::ExternalToggleEventGenerator> generator,
        const ILogger& logger)
{
        if (!generator)
        {
                logger.Warn("ExternalToggle generator is not configured; skipping API registration.");
                return;
        }

        auto generator_weak = std::weak_ptr<event::ExternalToggleEventGenerator>(generator);

        server.resource["^/api/external-toggle$"]["POST"] = [generator_weak, &logger](auto response, auto request) {
                auto generator_locked = generator_weak.lock();
                if (!generator_locked)
                {
                        response->write(SimpleWeb::StatusCode::server_error_service_unavailable,
                                "External toggle generator is unavailable\n");
                        return;
                }

                const auto state = parse_state(request->content.string(), logger);
                if (!state.has_value())
                {
                        response->write(SimpleWeb::StatusCode::client_error_bad_request,
                                "Body must be JSON with a boolean 'state' field\n");
                        return;
                }

                generator_locked->Trigger(*state);

                response->write(SimpleWeb::StatusCode::success_ok,
                        std::string("External toggle set to ") + (*state ? "true" : "false") + "\n");
        };

        logger.Info("External toggle API registered at /api/external-toggle");
}
} // namespace osrv
