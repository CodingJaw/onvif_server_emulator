#include "deviceio_service.h"

#include "IOnvifServer.h"

#include "../Server.h"
#include "../onvif/OnvifRequest.h"
#include "../utility/HttpHelper.h"
#include "../utility/SoapHelper.h"
#include "../utility/XmlParser.h"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <cctype>
#include <algorithm>
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

        static std::optional<bool> get_bool_property(const pt::ptree& node, const std::vector<std::string>& keys)
        {
                for (const auto& key : keys)
                {
                        if (auto val = node.get_optional<bool>(key))
                                return std::make_optional(*val);
                }

                return std::nullopt;
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

                        for (const auto& output : server_configs_->digital_outputs_)
                        {
                                if (output->GetToken() == token)
                                {
                                        if (state.has_value())
                                                output->SetState(*state);

                                        if (enabled.has_value())
                                        {
                                                if (*enabled)
                                                        output->Enable();
                                                else
                                                        output->Disable();
                                        }

                                        break;
                                }
                        }
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
