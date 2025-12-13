#include "deviceio_service.h"

#include "IOnvifServer.h"

#include "../Server.h"
#include "../onvif/OnvifRequest.h"
#include "../utility/HttpHelper.h"
#include "../utility/MediaProfilesManager.h"
#include "../utility/SoapHelper.h"
#include "../utility/XmlParser.h"

#include <boost/asio/steady_timer.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <cctype>
#include <algorithm>
#include <chrono>
#include <regex>
#include <optional>
#include <utility>

// List of implemented methods
const std::string GetVideoSources{"GetVideoSources"};
const std::string GetRelayOutputs{"GetRelayOutputs"};
const std::string GetDigitalInputs{"GetDigitalInputs"};
const std::string GetDigitalOutputs{"GetDigitalOutputs"};
const std::string SetRelayOutputState{"SetRelayOutputState"};
const std::string SetRelayOutputSettings{"SetRelayOutputSettings"};

namespace pt = boost::property_tree;

namespace osrv
{
struct GetVideoSourcesHandler : public OnvifRequestBase
{
private:
        const std::shared_ptr<IOnvifServer> onvif_srv_;

public:
        GetVideoSourcesHandler(const std::map<std::string, std::string>& xs, const std::shared_ptr<pt::ptree>& serviceConfigs,
                                                                                                 const std::shared_ptr<IOnvifServer>& srv)
                        : OnvifRequestBase(GetVideoSources, auth::SECURITY_LEVELS::READ_MEDIA, xs, serviceConfigs), onvif_srv_(srv)
        {
        }

        void operator()(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) override
        {
                static_cast<void>(request);

                auto envelope_tree = utility::soap::getEnvelopeTree(ns_);
                pt::ptree response_node;

                // currently all videosources described in the Media service's config file
                auto media_service_configs = onvif_srv_->MediaService()->Configs();
                const auto& videoSources = media_service_configs->get_child("GetVideoSources");
                for (const auto& [name, config_tree] : videoSources)
                {
                        response_node.add("tmd:GetVideoSourcesResponse.tmd:Token", config_tree.get<std::string>("token"));
                }

                envelope_tree.add_child("s:Body", response_node);

                pt::ptree root_tree;
                root_tree.put_child("s:Envelope", envelope_tree);

                std::ostringstream os;
                pt::write_xml(os, root_tree);

                utility::http::fillResponseWithHeaders(*response, os.str());
        }
};

struct GetRelayOutputsHandler : public OnvifRequestBase
{
private:
        const std::shared_ptr<ServerConfigs> server_configs_;

public:
        GetRelayOutputsHandler(const std::map<std::string, std::string>& xs, const std::shared_ptr<pt::ptree>& serviceConfigs,
                                                                                 std::shared_ptr<ServerConfigs> server_configs)
                        : OnvifRequestBase(GetRelayOutputs, auth::SECURITY_LEVELS::READ_MEDIA, xs, serviceConfigs),
                                server_configs_(std::move(server_configs))
        {
        }

        void operator()(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) override
        {
                static_cast<void>(request);

                auto envelope_tree = utility::soap::getEnvelopeTree(ns_);
                pt::ptree response_node;

                for (const auto& output : server_configs_->digital_outputs_)
                {
                        pt::ptree relay_node;
                        relay_node.add("<xmlattr>.token", output->GetToken());
                        relay_node.add("tt:Properties.tt:State", output->GetState());
                        relay_node.add("tt:Properties.tt:Enabled", output->IsEnabled());

                        response_node.add_child("tmd:GetRelayOutputsResponse.tmd:RelayOutputs", relay_node);
                }

                envelope_tree.add_child("s:Body", response_node);

                pt::ptree root_tree;
                root_tree.put_child("s:Envelope", envelope_tree);

                std::ostringstream os;
                pt::write_xml(os, root_tree);

                utility::http::fillResponseWithHeaders(*response, os.str());
        }
};

struct GetDigitalInputsHandler : public OnvifRequestBase
{
private:
        const std::shared_ptr<ServerConfigs> server_configs_;

public:
        GetDigitalInputsHandler(const std::map<std::string, std::string>& xs, const std::shared_ptr<pt::ptree>& serviceConfigs,
                                                         std::shared_ptr<ServerConfigs> server_configs)
                        : OnvifRequestBase(GetDigitalInputs, auth::SECURITY_LEVELS::READ_MEDIA, xs, serviceConfigs),
                                server_configs_(std::move(server_configs))
        {
        }

        void operator()(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) override
        {
                static_cast<void>(request);

                auto envelope_tree = utility::soap::getEnvelopeTree(ns_);
                pt::ptree response_node;

                for (const auto& input : server_configs_->digital_inputs_)
                {
                        pt::ptree input_node;
                        input_node.add("<xmlattr>.token", input->GetToken());
                        input_node.add("tt:Properties.tt:State", input->GetState());
                        input_node.add("tt:Properties.tt:Enabled", input->IsEnabled());

                        response_node.add_child("tmd:GetDigitalInputsResponse.tmd:DigitalInputs", input_node);
                }

                envelope_tree.add_child("s:Body", response_node);

                pt::ptree root_tree;
                root_tree.put_child("s:Envelope", envelope_tree);

                std::ostringstream os;
                pt::write_xml(os, root_tree);

                utility::http::fillResponseWithHeaders(*response, os.str());
        }
};

struct GetDigitalOutputsHandler : public OnvifRequestBase
{
private:
        const std::shared_ptr<ServerConfigs> server_configs_;

public:
        GetDigitalOutputsHandler(const std::map<std::string, std::string>& xs, const std::shared_ptr<pt::ptree>& serviceConfigs,
                                                          std::shared_ptr<ServerConfigs> server_configs)
                        : OnvifRequestBase(GetDigitalOutputs, auth::SECURITY_LEVELS::READ_MEDIA, xs, serviceConfigs),
                                server_configs_(std::move(server_configs))
        {
        }

        void operator()(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) override
        {
                static_cast<void>(request);

                auto envelope_tree = utility::soap::getEnvelopeTree(ns_);
                pt::ptree response_node;

                for (const auto& output : server_configs_->digital_outputs_)
                {
                        pt::ptree output_node;
                        output_node.add("<xmlattr>.token", output->GetToken());
                        output_node.add("tt:Properties.tt:State", output->GetState());
                        output_node.add("tt:Properties.tt:Enabled", output->IsEnabled());

                        response_node.add_child("tmd:GetDigitalOutputsResponse.tmd:DigitalOutputs", output_node);
                }

                envelope_tree.add_child("s:Body", response_node);

                pt::ptree root_tree;
                root_tree.put_child("s:Envelope", envelope_tree);

                std::ostringstream os;
                pt::write_xml(os, root_tree);

                utility::http::fillResponseWithHeaders(*response, os.str());
        }
};

struct SetRelayOutputStateHandler : public OnvifRequestBase
{
private:
        const std::shared_ptr<ServerConfigs> server_configs_;

        static bool to_bool_state(std::string logical_state)
        {
                std::transform(logical_state.begin(), logical_state.end(), logical_state.begin(), [](unsigned char c) {
                        return static_cast<char>(std::tolower(c));
                });

                return logical_state == "active" || logical_state == "true" || logical_state == "1";
        }

public:
        SetRelayOutputStateHandler(const std::map<std::string, std::string>& xs, const std::shared_ptr<pt::ptree>& serviceConfigs,
                                                           std::shared_ptr<ServerConfigs> server_configs)
                        : OnvifRequestBase(SetRelayOutputState, auth::SECURITY_LEVELS::ACTUATE, xs, serviceConfigs),
                                server_configs_(std::move(server_configs))
        {
        }

        void operator()(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) override
        {
                pt::ptree request_xml_tree;
                pt::xml_parser::read_xml(request->content, request_xml_tree);

                auto requested_token = exns::find_hierarchy("Envelope.Body.SetRelayOutputState.RelayOutputToken", request_xml_tree);
                auto requested_state = exns::find_hierarchy("Envelope.Body.SetRelayOutputState.LogicalState", request_xml_tree);

                if (!requested_token.empty())
                {
                        for (const auto& output : server_configs_->digital_outputs_)
                        {
                                if (output->GetToken() == requested_token)
                                {
                                        output->SetState(to_bool_state(requested_state));
                                        break;
                                }
                        }
                }

                auto envelope_tree = utility::soap::getEnvelopeTree(ns_);
                envelope_tree.add("s:Body.tmd:SetRelayOutputStateResponse", "");

                pt::ptree root_tree;
                root_tree.put_child("s:Envelope", envelope_tree);

                std::ostringstream os;
                pt::write_xml(os, root_tree);

                utility::http::fillResponseWithHeaders(*response, os.str());
        }
};

struct SetRelayOutputSettingsHandler : public OnvifRequestBase
{
private:
        const std::shared_ptr<ServerConfigs> server_configs_;

        static std::string to_lower_copy(std::string value)
        {
                std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                return value;
        }

        static std::optional<bool> get_bool_property(const pt::ptree& node, const std::vector<std::string>& keys)
        {
                for (const auto& key : keys)
                {
                        if (auto val = node.get_optional<bool>(key))
                                return std::make_optional(*val);
                }

                return std::nullopt;
        }

        static std::optional<std::string> get_string_property(const pt::ptree& node, const std::vector<std::string>& keys)
        {
                for (const auto& key : keys)
                {
                        if (auto val = node.get_optional<std::string>(key))
                                return std::make_optional(*val);
                }

                return std::nullopt;
        }

        static std::optional<std::chrono::milliseconds> parse_duration(const std::string& duration)
        {
                // Accept ISO 8601 duration in PT#S or PT#.#S format and plain milliseconds as integer
                static const std::regex iso_duration(R"(^P(T)?(?:(\d+(?:\.\d+)?)S)$)", std::regex_constants::icase);
                std::smatch match;
                if (std::regex_match(duration, match, iso_duration))
                {
                        double seconds = std::stod(match[2]);
                        if (seconds < 0)
                                return std::nullopt;

                        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::duration<double>(seconds));
                }

                try
                {
                        auto millis = std::stoll(duration);
                        if (millis < 0)
                                return std::nullopt;

                        return std::chrono::milliseconds(millis);
                }
                catch (const std::exception&)
                {
                        return std::nullopt;
                }
        }

        void send_invalid_arg_fault(std::shared_ptr<HttpServer::Response> response, const std::string& reason) const
        {
            auto envelope_tree = utility::soap::getEnvelopeTree(ns_);

            boost::property_tree::ptree code_node;
            code_node.add("s:Value", "s:Sender");
            code_node.add("s:Subcode.s:Value", "ter:InvalidArgVal");
            envelope_tree.add_child("s:Body.s:Fault.s:Code", code_node);
            envelope_tree.put("s:Body.s:Fault.s:Reason.s:Text", reason);
            envelope_tree.put("s:Body.s:Fault.s:Reason.s:Text.<xmlattr>.xml:lang", "en");

            pt::ptree root_tree;
            root_tree.put_child("s:Envelope", envelope_tree);

            std::ostringstream os;
            pt::write_xml(os, root_tree);

            utility::http::fillResponseWithHeaders(*response, os.str(), utility::http::ClientErrorDefaultWriter);
        }

        static RelayMode parse_mode(const std::string& raw_mode)
        {
                const auto normalized = to_lower_copy(raw_mode);
                if (normalized == "bistable")
                        return RelayMode::Bistable;

                if (normalized == "monostable")
                        return RelayMode::Monostable;

                throw std::invalid_argument("Unsupported relay mode");
        }

        static std::optional<bool> parse_idle_state(const std::string& raw_state)
        {
                const auto normalized = to_lower_copy(raw_state);

                if (normalized == "open" || normalized == "inactive" || normalized == "false" || normalized == "0")
                        return false;

                if (normalized == "closed" || normalized == "active" || normalized == "true" || normalized == "1")
                        return true;

                return std::nullopt;
        }

        void apply_state_with_delay(const std::shared_ptr<IDigitalOutput>& output, bool state, std::chrono::milliseconds delay,
                                                                std::optional<std::chrono::milliseconds> pulse_time, bool idle_state)
        {
                auto set_state_now = [output, state]() { output->SetState(state); };

                if (delay.count() > 0 && server_configs_->io_context_)
                {
                        auto timer = std::make_shared<boost::asio::steady_timer>(*server_configs_->io_context_, delay);
                        timer->async_wait([set_state_now, timer](const boost::system::error_code& ec) {
                                if (!ec)
                                        set_state_now();
                        });
                }
                else
                {
                        set_state_now();
                }

                if (pulse_time && pulse_time->count() > 0 && server_configs_->io_context_)
                {
                        auto revert_timer = std::make_shared<boost::asio::steady_timer>(*server_configs_->io_context_,
                                                                                                                           delay + *pulse_time);
                        revert_timer->async_wait([output, idle_state, revert_timer](const boost::system::error_code& ec) {
                                if (!ec)
                                        output->SetState(idle_state);
                        });
                }
        }

public:
        SetRelayOutputSettingsHandler(const std::map<std::string, std::string>& xs, const std::shared_ptr<pt::ptree>& serviceConfigs,
                                                              std::shared_ptr<ServerConfigs> server_configs)
                        : OnvifRequestBase(SetRelayOutputSettings, auth::SECURITY_LEVELS::ACTUATE, xs, serviceConfigs),
                                server_configs_(std::move(server_configs))
        {
        }

        void operator()(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) override
        {
                pt::ptree request_xml_tree;
                pt::xml_parser::read_xml(request->content, request_xml_tree);

                auto relay_nodes = exns::find_hierarchy_elements("Envelope.Body.SetRelayOutputSettings.RelayOutput", request_xml_tree);
                if (!relay_nodes.empty())
                {
                        const auto& relay_node = relay_nodes.front()->second;
                        auto token = relay_node.get<std::string>("<xmlattr>.token", "");

                        auto state = get_bool_property(relay_node,
                                                                                         {"tt:Properties.tt:State", "Properties.State", "tmd:Properties.tmd:State"});
                        auto enabled = get_bool_property(relay_node,
                                                                                           {"tt:Properties.tt:Enabled", "Properties.Enabled", "tmd:Properties.tmd:Enabled"});
                        auto mode_str = get_string_property(relay_node,
                                                                                          {"tt:Properties.tt:Mode", "Properties.Mode", "tmd:Properties.tmd:Mode"});
                        auto idle_state_str = get_string_property(relay_node,
                                                                                                  {"tt:Properties.tt:IdleState", "Properties.IdleState", "tmd:Properties.tmd:IdleState"});
                        auto delay_time_str = get_string_property(relay_node,
                                                                                                   {"tt:Properties.tt:DelayTime", "Properties.DelayTime", "tmd:Properties.tmd:DelayTime"});
                        auto pulse_time_str = get_string_property(relay_node,
                                                                                                   {"tt:Properties.tt:Extension.tt:PulseTime", "Properties.PulseTime", "tmd:Properties.tmd:PulseTime"});

                        bool token_found = false;
                        for (const auto& output : server_configs_->digital_outputs_)
                        {
                                if (output->GetToken() == token)
                                {
                                        token_found = true;
                                        auto idle_state = idle_state_str ? parse_idle_state(*idle_state_str) : std::optional<bool>{};
                                        if (idle_state_str && !idle_state)
                                        {
                                                send_invalid_arg_fault(response, "Unsupported IdleState value");
                                                return;
                                        }

                                        std::optional<std::chrono::milliseconds> delay_time;
                                        if (delay_time_str)
                                        {
                                                delay_time = parse_duration(*delay_time_str);
                                                if (!delay_time)
                                                {
                                                        send_invalid_arg_fault(response, "Invalid DelayTime value");
                                                        return;
                                                }
                                        }

                                        std::optional<std::chrono::milliseconds> pulse_time;
                                        if (pulse_time_str)
                                        {
                                                pulse_time = parse_duration(*pulse_time_str);
                                                if (!pulse_time)
                                                {
                                                        send_invalid_arg_fault(response, "Invalid PulseTime value");
                                                        return;
                                                }
                                        }

                                        RelayMode mode = output->GetMode();
                                        if (mode_str)
                                        {
                                                try
                                                {
                                                        mode = parse_mode(*mode_str);
                                                }
                                                catch (const std::invalid_argument&)
                                                {
                                                        send_invalid_arg_fault(response, "Unsupported relay Mode");
                                                        return;
                                                }
                                        }

                                        if (mode == RelayMode::Bistable && pulse_time)
                                        {
                                                send_invalid_arg_fault(response, "PulseTime is only valid for Monostable relays");
                                                return;
                                        }

                                        if (state.has_value())
                                        {
                                                const auto delay = delay_time.value_or(output->GetDelayTime());
                                                const auto pulse = mode == RelayMode::Monostable
                                                                                           ? (pulse_time ? pulse_time : std::optional<std::chrono::milliseconds>{output->GetPulseTime()})
                                                                                           : std::nullopt;
                                                const auto idle = idle_state.value_or(output->GetIdleState());
                                                apply_state_with_delay(output, *state, delay, pulse, idle);
                                        }

                                        if (enabled.has_value())
                                        {
                                                if (*enabled)
                                                        output->Enable();
                                                else
                                                        output->Disable();
                                        }

                                        if (idle_state)
                                                output->SetIdleState(*idle_state);

                                        if (mode_str)
                                                output->SetMode(mode);

                                        if (delay_time)
                                                output->SetDelayTime(*delay_time);

                                        if (pulse_time)
                                                output->SetPulseTime(*pulse_time);

                                        break;
                                }
                        }

                        if (!token_found)
                                throw osrv::invalid_token();
                }
                else
                {
                        send_invalid_arg_fault(response, "RelayOutput element is missing");
                        return;
                }

                auto envelope_tree = utility::soap::getEnvelopeTree(ns_);
                envelope_tree.add("s:Body.tmd:SetRelayOutputSettingsResponse", "");

                pt::ptree root_tree;
                root_tree.put_child("s:Envelope", envelope_tree);

                std::ostringstream os;
                pt::write_xml(os, root_tree);

                utility::http::fillResponseWithHeaders(*response, os.str());
        }
};

DeviceIOService::DeviceIOService(const std::string& service_uri, const std::string& service_name,
                                                                 std::shared_ptr<IOnvifServer> srv)
                : IOnvifService(service_uri, service_name, srv)
{
        requestHandlers_.push_back(std::make_shared<GetVideoSourcesHandler>(xml_namespaces_, configs_ptree_, srv));
        requestHandlers_.push_back(std::make_shared<GetRelayOutputsHandler>(xml_namespaces_, configs_ptree_, server_configs_));
        requestHandlers_.push_back(std::make_shared<GetDigitalInputsHandler>(xml_namespaces_, configs_ptree_, server_configs_));
        requestHandlers_.push_back(std::make_shared<GetDigitalOutputsHandler>(xml_namespaces_, configs_ptree_, server_configs_));
        requestHandlers_.push_back(std::make_shared<SetRelayOutputStateHandler>(xml_namespaces_, configs_ptree_, server_configs_));
        requestHandlers_.push_back(std::make_shared<SetRelayOutputSettingsHandler>(xml_namespaces_, configs_ptree_, server_configs_));
}

} // namespace osrv
