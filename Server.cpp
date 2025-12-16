#include "Server.h"
#include "../onvif_services/device_service.h"
#include "../onvif_services/discovery_service.h"
#include "../onvif_services/event_service.h"
#include "../onvif_services/imaging_service.h"
#include "../onvif_services/media2_service.h"
#include "../onvif_services/media_service.h"
#include "../onvif_services/physical_components/IDigitalOutput.h"
#include "../onvif_services/physical_components/IDigitalInput.h"
#include "../onvif_services/ptz_service.h"
#include "../onvif_services/recording_search_service.h"
#include "include/onvif_services/service_configs.h"

#include "utility/AuthHelper.h"
#include "utility/MediaProfilesManager.h"
#include "utility/XmlParser.h"

#include "MediaFormats.h"

#include "Simple-Web-Server/server_http.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/property_tree/json_parser.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

static const std::string COMMON_CONFIGS_NAME = "common.config";

namespace osrv
{

AUTH_SCHEME str_to_auth(const std::string& /*scheme*/);

Server::Server(const std::string& configs_dir, std::shared_ptr<ILogger> log) : IOnvifServer(configs_dir, log)
{
}

Server::~Server()
{
	discovery::stop();

	io_context_work_.reset();
	try
	{
		if (io_context_thread_ && io_context_thread_->joinable())
		{
			io_context_thread_->join();
			logger_->Debug("Async IO Context's thread is joined.");
		}
	}
	catch (const std::exception&)
	{
	}

	if (rtspServer_)
		delete rtspServer_;
}

void Server::init()
{
	http_server_->default_resource["GET"] = [](std::shared_ptr<HttpServer::Response> response,
																						 std::shared_ptr<HttpServer::Request> request) {
		response->write(SimpleWeb::StatusCode::client_error_bad_request, "Could not open path " + request->path);
	};

        http_server_->default_resource["POST"] = [this](std::shared_ptr<HttpServer::Response> response,
                                                                                                                               std::shared_ptr<HttpServer::Request> request) {
                logger_->Warn("The server could not handle a request:" + request->method + " " + request->path);
                response->write(SimpleWeb::StatusCode::client_error_bad_request, "Bad request");
        };

	http_server_->on_error = [this](std::shared_ptr<HttpServer::Request> request, const SimpleWeb::error_code& ec) {
		// if (ec != SimpleWeb::errc::operation_canceled && SimpleWeb::error_code::)
		//{
		//	logger_->Error("HTTP server internal error: " + ec.message());
		//}
	};

        auto configs_dir = configs_path_ + "/";
        server_configs_ = read_server_configs(configs_dir + COMMON_CONFIGS_NAME);

        http_server_->config.address = server_configs_->ipv4_address_;
        http_server_->config.port = std::stoi(server_configs_->http_port_);

	if (server_configs_->enabled_http_port_forwarding)
		logger_->Info("HTTP port forwarding simulated on port: " + std::to_string(server_configs_->forwarded_http_port));

        if (server_configs_->enabled_rtsp_port_forwarding)
                logger_->Info("RTSP port forwarding simulated on port: " + std::to_string(server_configs_->forwarded_rtsp_port));

        server_configs_->digest_session_ = std::make_shared<utility::digest::DigestSessionImpl>();
        // TODO: here is the same list is copied into digest_session, although it's already stored in server_configs
        server_configs_->digest_session_->set_users_list(server_configs_->system_users_);

        http_server_->resource["^/api/io/input$"]["GET"] = [this](const std::shared_ptr<HttpServer::Response>& response,
                                                                                                   const std::shared_ptr<HttpServer::Request>& /*request*/) {
                namespace pt = boost::property_tree;
                pt::ptree root;
                pt::ptree inputs;

                for (const auto& di : server_configs_->digital_inputs_)
                {
                        pt::ptree di_node;
                        di_node.put("token", di->GetToken());
                        di_node.put("state", di->GetState());
                        di_node.put("enabled", di->IsEnabled());
                        inputs.push_back(std::make_pair("", di_node));
                }

                root.add_child("inputs", inputs);

                std::ostringstream os;
                pt::write_json(os, root);
                response->write(SimpleWeb::StatusCode::success_ok, os.str());
        };

        http_server_->resource["^/api/io/output$"]["GET"] = [this](const std::shared_ptr<HttpServer::Response>& response,
                                                                                                    const std::shared_ptr<HttpServer::Request>& /*request*/) {
                namespace pt = boost::property_tree;
                pt::ptree root;
                pt::ptree outputs;

                for (const auto& output : server_configs_->digital_outputs_)
                {
                        pt::ptree output_node;
                        output_node.put("token", output->GetToken());
                        output_node.put("state", output->GetState());
                        output_node.put("enabled", output->IsEnabled());
                        outputs.push_back(std::make_pair("", output_node));
                }

                root.add_child("outputs", outputs);

                std::ostringstream os;
                pt::write_json(os, root);
                response->write(SimpleWeb::StatusCode::success_ok, os.str());
        };

        http_server_->resource["^/api/io/motion$"]["GET"] = [](const std::shared_ptr<HttpServer::Response>& response,
                                                                  const std::shared_ptr<HttpServer::Request>& /*request*/) {
                namespace pt = boost::property_tree;

                pt::ptree root;
                pt::ptree motions;

                for (const auto& motion_state : osrv::event::get_motion_states())
                {
                        pt::ptree motion_node;
                        motion_node.put("token", motion_state.token);
                        motion_node.put("state", motion_state.state);
                        motion_node.put("enabled", motion_state.enabled);
                        motions.push_back(std::make_pair("", motion_node));
                }

                root.add_child("motions", motions);

                std::ostringstream os;
                pt::write_json(os, root);
                response->write(SimpleWeb::StatusCode::success_ok, os.str());
        };

        auto io_input_handler = [this](const std::shared_ptr<HttpServer::Response>& response,
                                       const std::shared_ptr<HttpServer::Request>& request) {
                namespace pt = boost::property_tree;

                std::stringstream ss;
                ss << request->content.string();

                pt::ptree request_body;
                try
                {
                        pt::read_json(ss, request_body);
                }
                catch (const std::exception&)
                {
                        response->write(SimpleWeb::StatusCode::client_error_bad_request, "Invalid JSON");
                        return;
                }

                const auto token = request_body.get_optional<std::string>("token");
                if (!token)
                {
                        response->write(SimpleWeb::StatusCode::client_error_bad_request, "Missing token");
                        return;
                }

                auto input_it = std::find_if(server_configs_->digital_inputs_.begin(), server_configs_->digital_inputs_.end(),
                        [&token](const std::shared_ptr<IDigitalInput>& input) { return input->GetToken() == *token; });

                if (input_it == server_configs_->digital_inputs_.end())
                {
                        response->write(SimpleWeb::StatusCode::client_error_not_found, "Unknown token");
                        return;
                }

                if (auto enabled = request_body.get_optional<bool>("enabled"))
                {
                        if (*enabled)
                                (*input_it)->Enable();
                        else
                                (*input_it)->Disable();
                }

                if (auto state = request_body.get_optional<bool>("state"))
                {
                        (*input_it)->SetState(*state);
                }

                pt::ptree input_node;
                input_node.put("token", (*input_it)->GetToken());
                input_node.put("state", (*input_it)->GetState());
                input_node.put("enabled", (*input_it)->IsEnabled());

                pt::ptree root;
                root.add_child("input", input_node);

                std::ostringstream os;
                pt::write_json(os, root);
                response->write(SimpleWeb::StatusCode::success_ok, os.str());
        };

        http_server_->resource["^/api/io/input$"]["POST"] = io_input_handler;
        http_server_->resource["^/api/io/input$"]["PUT"] = io_input_handler;

        auto io_output_handler = [this](const std::shared_ptr<HttpServer::Response>& response,
                                        const std::shared_ptr<HttpServer::Request>& request) {
                namespace pt = boost::property_tree;

                std::stringstream ss;
                ss << request->content.string();

                pt::ptree request_body;
                try
                {
                        pt::read_json(ss, request_body);
                }
                catch (const std::exception&)
                {
                        response->write(SimpleWeb::StatusCode::client_error_bad_request, "Invalid JSON");
                        return;
                }

                const auto token = request_body.get_optional<std::string>("token");
                if (!token)
                {
                        response->write(SimpleWeb::StatusCode::client_error_bad_request, "Missing token");
                        return;
                }

                auto output_it = std::find_if(server_configs_->digital_outputs_.begin(), server_configs_->digital_outputs_.end(),
                        [&token](const std::shared_ptr<IDigitalOutput>& output) { return output->GetToken() == *token; });

                if (output_it == server_configs_->digital_outputs_.end())
                {
                        response->write(SimpleWeb::StatusCode::client_error_not_found, "Unknown token");
                        return;
                }

                if (auto enabled = request_body.get_optional<bool>("enabled"))
                {
                        if (*enabled)
                                (*output_it)->Enable();
                        else
                                (*output_it)->Disable();
                }

                if (auto state = request_body.get_optional<bool>("state"))
                {
                        (*output_it)->SetState(*state);
                }

                pt::ptree output_node;
                output_node.put("token", (*output_it)->GetToken());
                output_node.put("state", (*output_it)->GetState());
                output_node.put("enabled", (*output_it)->IsEnabled());

                pt::ptree root;
                root.add_child("output", output_node);

                std::ostringstream os;
                pt::write_json(os, root);
                response->write(SimpleWeb::StatusCode::success_ok, os.str());
        };

        http_server_->resource["^/api/io/output$"]["POST"] = io_output_handler;
        http_server_->resource["^/api/io/output$"]["PUT"] = io_output_handler;

        auto io_motion_handler = [](const std::shared_ptr<HttpServer::Response>& response,
                                    const std::shared_ptr<HttpServer::Request>& request) {
                namespace pt = boost::property_tree;

                std::stringstream ss;
                ss << request->content.string();

                pt::ptree request_body;
                try
                {
                        pt::read_json(ss, request_body);
                }
                catch (const std::exception&)
                {
                        response->write(SimpleWeb::StatusCode::client_error_bad_request, "Invalid JSON");
                        return;
                }

                const auto token = request_body.get_optional<std::string>("token");
                if (!token)
                {
                        response->write(SimpleWeb::StatusCode::client_error_bad_request, "Missing token");
                        return;
                }

                auto enabled = request_body.get_optional<bool>("enabled");
                auto state = request_body.get_optional<bool>("state");

                if (!enabled && !state)
                {
                        response->write(SimpleWeb::StatusCode::client_error_bad_request, "Missing state or enabled flag");
                        return;
                }

                auto updated_state = osrv::event::update_motion_state(*token, enabled ? std::optional<bool>(*enabled) : std::optional<bool>(),
                                                                                      state ? std::optional<bool>(*state) : std::optional<bool>());

                if (!updated_state)
                {
                        response->write(SimpleWeb::StatusCode::client_error_not_found, "Unknown token");
                        return;
                }

                pt::ptree motion_node;
                motion_node.put("token", updated_state->token);
                motion_node.put("state", updated_state->state);
                motion_node.put("enabled", updated_state->enabled);

                pt::ptree root;
                root.add_child("motion", motion_node);

                std::ostringstream os;
                pt::write_json(os, root);
                response->write(SimpleWeb::StatusCode::success_ok, os.str());
        };

        http_server_->resource["^/api/io/motion$"]["POST"] = io_motion_handler;
        http_server_->resource["^/api/io/motion$"]["PUT"] = io_motion_handler;

        profiles_config_ = osrv::ServiceConfigs("media_profiles", configs_dir);

	DeviceService()->Run();
	DeviceIOService()->Run();
	ImagingService()->Run();
	MediaService()->Run();
	Media2Service()->Run();
	PTZService()->Run();
	RecordingSearchService()->Run();
	ReplayControlService()->Run();

	event::init_service(*http_server_, *server_configs_, configs_dir, *logger_);
	discovery::init_service(configs_dir, *logger_);

	// TODO: impl. logic for multichannel cannel
	auto audio_node = profiles_config_->get_child(CONFIGURATION_ENUMERATION[CONFIGURATION_TYPE::AUDIOENCODER]).front();
	audio_node.second.get_value<std::string>("Encoding");
	audio_node.second.get_value<std::string>("Encoding");

	rtsp::AudioInfo ainfo{
			audio_node.second.get<std::string>("Encoding"),
			audio_node.second.get<unsigned int>("Bitrate") * 1000,
			audio_node.second.get<unsigned int>("SampleRate") * 1000,
	};

	rtspServer_ = new rtsp::Server(&*logger_, *server_configs_, std::move(ainfo));

	if (auto delay = server_configs_->network_delay_simulation_; delay > 0)
	{
		logger_->Info("Network delay simulation is enabled. Equals (ms): " + std::to_string(delay));
	}

	io_context_ = std::make_shared<boost::asio::io_context>();
	io_context_work_ = std::make_shared<boost::asio::io_context::work>(*io_context_);
	io_context_thread_ = std::make_shared<std::thread>([this]() {
		logger_->Debug("Async IO Context's thread is running...");
		io_context_->run();
	});

	server_configs_->io_context_ = io_context_;
}

void Server::run()
{
	using namespace std;

	// Start server and receive assigned port when server is listening for requests
	promise<unsigned short> server_port;
	thread server_thread([this, &server_port]() {
		// Start HTTP server
		try
		{
			http_server_->start([&server_port](unsigned short port) { server_port.set_value(port); });
		}
		catch (const std::exception& e)
		{
			logger_->Error(std::string("HTTP server finished with a critical erroe: ") + e.what());
		}
	});

	rtspServer_->run();
	try
	{
		discovery::start();
	}
	catch (const std::exception& e)
	{
		std::string what(e.what());
		logger_->Error("Can't start Discovery Service: " + what);
	}

	std::string msg("Server is successfully started on port: ");
	msg += std::to_string(server_port.get_future().get());
	logger_->Info(msg);

	server_thread.join();
}

std::shared_ptr<ServerConfigs> read_server_configs(const std::string& config_path)
{

        std::ifstream configs_file(config_path);
        if (!configs_file.is_open())
		throw std::runtime_error("Could not read a config file");

	namespace pt = boost::property_tree;
	pt::ptree configs_tree;
	pt::read_json(configs_file, configs_tree);

	auto read_configs = std::make_shared<ServerConfigs>();

	read_configs->ipv4_address_ = configs_tree.get<std::string>("addresses.ipv4");
	read_configs->http_port_ = configs_tree.get<std::string>("addresses.http_port");
	read_configs->rtsp_port_ = configs_tree.get<std::string>("addresses.rtsp_port");

	read_configs->enabled_http_port_forwarding =
			configs_tree.get<bool>("portForwardingSimulation.enabled_for_http", false);
	if (read_configs->enabled_http_port_forwarding)
		read_configs->forwarded_http_port = configs_tree.get<unsigned short>("portForwardingSimulation.http_port");

	read_configs->enabled_rtsp_port_forwarding =
			configs_tree.get<bool>("portForwardingSimulation.enabled_for_rtsp", false);
	if (read_configs->enabled_rtsp_port_forwarding)
		read_configs->forwarded_rtsp_port = configs_tree.get<unsigned short>("portForwardingSimulation.rtsp_port");

	auto auth_scheme = configs_tree.get<std::string>("authentication");
	read_configs->auth_scheme_ = str_to_auth(auth_scheme);

	auto users_node = configs_tree.get_child("users");
	if (users_node.empty())
		throw std::runtime_error("Could not read Users list");

	for (auto user : users_node)
	{
		read_configs->system_users_.emplace_back(osrv::auth::UserAccount{
				user.second.get<std::string>(auth::UserAccount::LOGIN), user.second.get<std::string>(auth::UserAccount::PASS),
				osrv::auth::str_to_usertype(user.second.get<std::string>(auth::UserAccount::TYPE))});
	}

	read_configs->network_delay_simulation_ = configs_tree.get<unsigned short>("networkDelaySimulation.milliseconds");

        read_configs->multichannel_enabled_ = configs_tree.get<bool>("multichannelSimulation.enabled");
        read_configs->channels_count_ = configs_tree.get<unsigned char>("multichannelSimulation.channelCount");

        if (configs_tree.get<bool>("fileStreaming.enabled"))
        {
                read_configs->rtsp_streaming_file_ = configs_tree.get<std::string>("fileStreaming.filePath");
        }

        if (auto digital_inputs_node = configs_tree.get_child_optional("DigitalInputs"))
        {
                read_configs->digital_inputs_ = read_digital_inputs(*digital_inputs_node);
        }

        if (auto digital_outputs_node = configs_tree.get_child_optional("DigitalOutputs"))
        {
                read_configs->digital_outputs_ = read_digital_outputs(*digital_outputs_node);
        }

        namespace fs = std::filesystem;
        const fs::path config_fs_path(config_path);
        const auto device_config_path = config_fs_path.parent_path() / "device.config";
        std::ifstream device_config_file(device_config_path.string());

        if (device_config_file.is_open())
        {
                pt::ptree device_configs_tree;
                pt::read_json(device_config_file, device_configs_tree);

                if (auto device_digital_inputs = device_configs_tree.get_child_optional("DigitalInputs"))
                {
                        auto digital_inputs = read_digital_inputs(*device_digital_inputs);
                        read_configs->digital_inputs_.insert(read_configs->digital_inputs_.end(), digital_inputs.begin(), digital_inputs.end());
                }

                if (auto device_digital_outputs = device_configs_tree.get_child_optional("DigitalOutputs"))
                {
                        auto digital_outputs = read_digital_outputs(*device_digital_outputs);
                        read_configs->digital_outputs_.insert(read_configs->digital_outputs_.end(), digital_outputs.begin(), digital_outputs.end());
                }
        }

        if (!device_config_file.is_open() && !fs::exists(device_config_path))
        {
                // ignore missing device config silently; digital IO can be configured directly in common config
        }

        return read_configs;
}

DigitalInputsList read_digital_inputs(const boost::property_tree::ptree& configs_node)
{
        std::vector<std::shared_ptr<IDigitalInput>> result;
        for (const auto& t : configs_node)
        {
		auto di = std::make_shared<SimpleDigitalInputImpl>(
				SimpleDigitalInputImpl(t.second.get<std::string>("Token"), t.second.get<bool>("InitialState")));

		if (t.second.get<bool>("GenerateEvent"))
		{
			di->Enable();
		}
		else
		{
			di->Disable();
		}

		result.push_back(di);
	}

        return result;
}

DigitalOutputsList read_digital_outputs(const boost::property_tree::ptree& configs_node)
{
        std::vector<std::shared_ptr<IDigitalOutput>> result;
        for (const auto& t : configs_node)
        {
                auto output = std::make_shared<SimpleDigitalOutputImpl>(
                                SimpleDigitalOutputImpl(t.second.get<std::string>("Token"),
                                                        t.second.get<bool>("InitialState", false)));

                if (t.second.get<bool>("Enabled", true))
                {
                        output->Enable();
                }
                else
                {
                        output->Disable();
                }

                result.push_back(output);
        }

        return result;
}

AUTH_SCHEME str_to_auth(const std::string& scheme)
{
	if (scheme == "digest/ws-security")
		return AUTH_SCHEME::DIGEST_WSS;

	if (scheme == "digest")
		return AUTH_SCHEME::DIGEST;

	if (scheme == "ws-security")
		return AUTH_SCHEME::WSS;

	return AUTH_SCHEME::NONE;
}

} // namespace osrv
