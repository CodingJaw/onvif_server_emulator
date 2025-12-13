#include "device_service.h"

#include "../onvif/OnvifRequest.h"

#include "../Logger.h"
#include "../Server.h"
#include "../utility/HttpDigestHelper.h"
#include "../utility/HttpHelper.h"
#include "../utility/SoapHelper.h"
#include "../utility/XmlParser.h"

#include "../Simple-Web-Server/server_http.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/algorithm/string.hpp>

#include <chrono>
#include <ctime>
#include <optional>
#include <iomanip>

// List of implemented methods
const std::string GetCapabilities = "GetCapabilities";
const std::string GetDeviceInformation = "GetDeviceInformation";
const std::string GetNetworkInterfaces = "GetNetworkInterfaces";
const std::string GetRelayOutputs = "GetRelayOutputs";
const std::string GetServices = "GetServices";
const std::string GetScopes = "GetScopes";
const std::string GetSystemDateAndTime = "GetSystemDateAndTime";
const std::string SetSystemDateAndTime = "SetSystemDateAndTime";

namespace pt = boost::property_tree;

namespace osrv
{
namespace
{
struct SystemDateTimeState
{
        std::string date_time_type{"NTP"};
        bool daylight_savings{false};
        bool use_utc{true};
        std::optional<std::chrono::system_clock::time_point> utc_time{};
};

SystemDateTimeState& system_time_state()
{
        static SystemDateTimeState state;
        return state;
}

std::tm to_utc_tm(const std::chrono::system_clock::time_point& tp)
{
        const auto time_val = std::chrono::system_clock::to_time_t(tp);
        std::tm utc_tm{};
        gmtime_r(&time_val, &utc_tm);
        return utc_tm;
}

std::string tm_to_string(const std::tm& tm)
{
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
}
} // namespace

struct GetCapabilitiesHandler : public OnvifRequestBase
{
        GetCapabilitiesHandler(const std::map<std::string, std::string>& xs, const std::shared_ptr<pt::ptree>& configs,
												 osrv::ServerConfigs& server_cfg, const std::string& server_address)
			: OnvifRequestBase(GetCapabilities, auth::SECURITY_LEVELS::PRE_AUTH, xs, configs), srv_cfgs_(server_cfg),
				srv_addr_(server_address)
	{
	}

	void operator()(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) override
	{
		// this processor will add a full network address to service's paths from configs
		struct XAddrProcessor
		{
			XAddrProcessor(const std::string& addr) : address_(addr)
			{
			}

			void operator()(const std::string& element, std::string& elData)
			{
				if (element == "XAddr")
				{
					elData = address_ + elData;
				}
			}

			const std::string address_;
		};

		auto capabilities_config = service_configs_->get_child("GetCapabilities");
                pt::ptree capabilities_node;
                utility::soap::jsonNodeToXml(capabilities_config, capabilities_node, "tt", XAddrProcessor(srv_addr_));

                // here cound of DI is overrided dynamically depending on the actually count of DI in the config file
                capabilities_node.add("tt:Device.tt:IO.tt:InputConnectors", srv_cfgs_.digital_inputs_.size());
                capabilities_node.add("tt:Device.tt:IO.tt:RelayOutputs", srv_cfgs_.digital_outputs_.size());
                capabilities_node.add("tt:Device.tt:Extension.tt:DeviceIO.tt:RelayOutputs",
                                     srv_cfgs_.digital_outputs_.size());

		auto envelope_tree = utility::soap::getEnvelopeTree(ns_);
		envelope_tree.add_child("s:Body.tds:GetCapabilitiesResponse.tds:Capabilities", capabilities_node);

		pt::ptree root_tree;
		root_tree.put_child("s:Envelope", envelope_tree);

		std::ostringstream os;
		pt::write_xml(os, root_tree);

		utility::http::fillResponseWithHeaders(*response, os.str());
	}

private:
	osrv::ServerConfigs& srv_cfgs_;
	const std::string srv_addr_;
};

struct GetDeviceInformationHandler : public OnvifRequestBase
{
	GetDeviceInformationHandler(const std::map<std::string, std::string>& xs, const std::shared_ptr<pt::ptree>& configs)
			: OnvifRequestBase(GetDeviceInformation, auth::SECURITY_LEVELS::READ_SYSTEM, xs, configs)
	{
	}

	void operator()(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) override
	{
		auto envelope_tree = utility::soap::getEnvelopeTree(ns_);

		auto device_info_config = service_configs_->get_child("GetDeviceInformation");
		pt::ptree device_info_node;

		utility::soap::jsonNodeToXml(device_info_config, device_info_node, "tds");

		envelope_tree.add_child("s:Body.tds:GetDeviceInformationResponse", device_info_node);

		pt::ptree root_tree;
		root_tree.put_child("s:Envelope", envelope_tree);

		std::ostringstream os;
		pt::write_xml(os, root_tree);

		utility::http::fillResponseWithHeaders(*response, os.str());
	}
};

struct GetNetworkInterfacesHandler : public OnvifRequestBase
{
	GetNetworkInterfacesHandler(const std::map<std::string, std::string>& xs, const std::shared_ptr<pt::ptree>& configs)
			: OnvifRequestBase(GetNetworkInterfaces, auth::SECURITY_LEVELS::READ_SYSTEM, xs, configs)
	{
	}

	void operator()(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) override
	{
		auto envelope_tree = utility::soap::getEnvelopeTree(ns_);

		auto network_interfaces_config = service_configs_->get_child("GetNetworkInterfaces");
		pt::ptree network_interfaces_node;

		// this processor will add a xml ns depending on element
		struct NsProcessor
		{
			void operator()(std::string& element, std::string& elData)
			{
				if (element == "NetworkInterfaces")
				{
					// currently implemented only Loopback address
					element = "tds:" + element;
				}
				else
				{
					element = "tt:" + element;
				}
			}
		};

		utility::soap::jsonNodeToXml(network_interfaces_config, network_interfaces_node, "", NsProcessor());

		envelope_tree.add_child("s:Body.tds:GetNetworkInterfacesResponse", network_interfaces_node);

		pt::ptree root_tree;
		root_tree.put_child("s:Envelope", envelope_tree);

		std::ostringstream os;
		pt::write_xml(os, root_tree);

		utility::http::fillResponseWithHeaders(*response, os.str());
	}
};

struct GetRelayOutputsHandler : public OnvifRequestBase
{
	GetRelayOutputsHandler(const std::map<std::string, std::string>& xs, const std::shared_ptr<pt::ptree>& configs)
			: OnvifRequestBase(GetRelayOutputs, auth::SECURITY_LEVELS::READ_MEDIA, xs, configs)
	{
	}

	void operator()(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) override
	{
		auto envelope_tree = utility::soap::getEnvelopeTree(ns_);

		// TODO: here is just stub response
		envelope_tree.add("s:Body.tds:GetRelayOutputsResponse", "");

		pt::ptree root_tree;
		root_tree.put_child("s:Envelope", envelope_tree);

		std::ostringstream os;
		pt::write_xml(os, root_tree);

		utility::http::fillResponseWithHeaders(*response, os.str());
	}
};

struct GetServicesHandler : public OnvifRequestBase
{
	GetServicesHandler(const std::map<std::string, std::string>& xs, const std::shared_ptr<pt::ptree>& configs,
										 const std::string& ip)
			: OnvifRequestBase(GetServices, auth::SECURITY_LEVELS::PRE_AUTH, xs, configs), ipv4_address_(ip)
	{
	}

	void operator()(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) override
	{
		auto envelope_tree = utility::soap::getEnvelopeTree(ns_);

		auto services_config = service_configs_->get_child(GetServices);
		pt::ptree services_node;

		// here's Services are enumerates as array, so handle them manualy
		for (auto elements : services_config)
		{
			if (!elements.second.get<bool>("Enabled", true))
				continue;

			pt::ptree xml_service_node;
			xml_service_node.put("tds:Namespace", elements.second.get<std::string>("namespace"));
			xml_service_node.put("tds:XAddr", ipv4_address_ + elements.second.get<std::string>("XAddr"));
			if (elements.second.get<std::string>("namespace") == "http://www.onvif.org/ver20/ptz/wsdl")
			{
				xml_service_node.put("tds:Capabilities.tptz:Capabilities", "");
			}
			xml_service_node.put("tds:Version.tt:Major", elements.second.get<std::string>("Version.Major"));
			xml_service_node.put("tds:Version.tt:Minor", elements.second.get<std::string>("Version.Minor"));

			services_node.add_child("tds:Service", xml_service_node);
		}

		envelope_tree.add_child("s:Body.tds:GetServicesResponse", services_node);

		pt::ptree root_tree;
		root_tree.put_child("s:Envelope", envelope_tree);

		std::ostringstream os;
		pt::write_xml(os, root_tree);

		utility::http::fillResponseWithHeaders(*response, os.str());
	}

private:
	const std::string ipv4_address_;
};

struct GetScopesHandler : public OnvifRequestBase
{
	GetScopesHandler(const std::map<std::string, std::string>& xs, const std::shared_ptr<pt::ptree>& configs)
			: OnvifRequestBase(GetScopes, auth::SECURITY_LEVELS::READ_SYSTEM, xs, configs)
	{
	}

	void operator()(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) override
	{
		auto envelope_tree = utility::soap::getEnvelopeTree(ns_);

		static const auto SCOPES_TREE = service_configs_->get_child("GetScopes");
		for (const auto& it : SCOPES_TREE)
		{
			pt::ptree scopes_tree;
			scopes_tree.put("tt:ScopeDef", "Fixed");
			scopes_tree.put("tt:ScopeItem", "onvif://www.onvif.org/" + it.first + "/" + it.second.get_value<std::string>());

			envelope_tree.add_child("s:Body.tds:GetScopesResponse", scopes_tree);
		}

		pt::ptree root_tree;
		root_tree.put_child("s:Envelope", envelope_tree);

		std::ostringstream os;
		pt::write_xml(os, root_tree);

		utility::http::fillResponseWithHeaders(*response, os.str());
	}
};

struct GetSystemDateAndTimeHandler : public OnvifRequestBase
{
	GetSystemDateAndTimeHandler(const std::map<std::string, std::string>& xs, const std::shared_ptr<pt::ptree>& configs)
			: OnvifRequestBase(GetSystemDateAndTime, auth::SECURITY_LEVELS::PRE_AUTH, xs, configs)
	{
	}

	void operator()(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) override
	{
                auto envelope_tree = utility::soap::getEnvelopeTree(ns_);

                const auto& state = system_time_state();
                const auto time_point = state.utc_time.value_or(std::chrono::system_clock::now());
                const auto tm = to_utc_tm(time_point);

                envelope_tree.put("s:Body.tds:GetSystemDateAndTimeResponse.tds:SystemDateAndTime.tt:DateTimeType",
                                  state.date_time_type);
                envelope_tree.put("s:Body.tds:GetSystemDateAndTimeResponse.tds:SystemDateAndTime.tt:DaylightSavings",
                                  state.daylight_savings ? "true" : "false");

                envelope_tree.put(
                                "s:Body.tds:GetSystemDateAndTimeResponse.tds:SystemDateAndTime.tt:UTCDateTime.tt:Time.tt:Hour",
                                tm.tm_hour);
                envelope_tree.put("s:Body.tds:GetSystemDateAndTimeResponse.tds:SystemDateAndTime.tt:UTCDateTime.tt:Time.tt:Minute",
                                  tm.tm_min);
                envelope_tree.put("s:Body.tds:GetSystemDateAndTimeResponse.tds:SystemDateAndTime.tt:UTCDateTime.tt:Time.tt:Second",
                                  tm.tm_sec);
                envelope_tree.put(
                                "s:Body.tds:GetSystemDateAndTimeResponse.tds:SystemDateAndTime.tt:UTCDateTime.tt:Date.tt:Year",
                                tm.tm_year + 1900);
                envelope_tree.put(
                                "s:Body.tds:GetSystemDateAndTimeResponse.tds:SystemDateAndTime.tt:UTCDateTime.tt:Date.tt:Month",
                                tm.tm_mon + 1);
                envelope_tree.put(
                                "s:Body.tds:GetSystemDateAndTimeResponse.tds:SystemDateAndTime.tt:UTCDateTime.tt:Date.tt:Day",
                                tm.tm_mday);

                pt::ptree root_tree;
                root_tree.put_child("s:Envelope", envelope_tree);

		std::ostringstream os;
		pt::write_xml(os, root_tree);

		utility::http::fillResponseWithHeaders(*response, os.str());
        }
};

struct SetSystemDateAndTimeHandler : public OnvifRequestBase
{
        SetSystemDateAndTimeHandler(const std::map<std::string, std::string>& xs, const std::shared_ptr<pt::ptree>& configs,
                                    const std::shared_ptr<ILogger>& logger)
                        : OnvifRequestBase(SetSystemDateAndTime, auth::SECURITY_LEVELS::ACTUATE, xs, configs), log_(logger)
        {
        }

        void operator()(std::shared_ptr<HttpServer::Response> response, std::shared_ptr<HttpServer::Request> request) override
        {
                auto request_str = request->content.string();
                std::istringstream is(request_str);

                pt::ptree xml_tree;
                try
                {
                        pt::xml_parser::read_xml(is, xml_tree);
                }
                catch (const pt::xml_parser_error&)
                {
                        send_fault(response, "Unable to parse XML payload");
                        return;
                }

                const auto date_time_type = exns::find_hierarchy("Envelope.Body.SetSystemDateAndTime.DateTimeType", xml_tree);
                if (date_time_type.empty())
                {
                        send_fault(response, "DateTimeType is required");
                        return;
                }

                if (!boost::iequals(date_time_type, "NTP") && !boost::iequals(date_time_type, "Manual"))
                {
                        send_fault(response, "Unsupported DateTimeType: " + date_time_type);
                        return;
                }

                const auto daylight = exns::find_hierarchy("Envelope.Body.SetSystemDateAndTime.DaylightSavings", xml_tree);
                if (daylight.empty())
                {
                        send_fault(response, "DaylightSavings is required");
                        return;
                }

                const auto daylight_saving_enabled = parse_bool(daylight, false);
                const auto has_utc = !exns::find_hierarchy("Envelope.Body.SetSystemDateAndTime.UTCDateTime", xml_tree).empty();

                std::optional<std::chrono::system_clock::time_point> requested_time;

                if (boost::iequals(date_time_type, "Manual"))
                {
                        if (!has_utc)
                        {
                                send_fault(response, "Manual mode requires UTCDateTime");
                                return;
                        }

                        auto parsed = parse_utc_time(xml_tree);
                        if (!parsed)
                        {
                                send_fault(response, "UTCDateTime is missing required fields");
                                return;
                        }

                        requested_time = parsed.value();
                }

                auto& state = system_time_state();
                state.date_time_type = date_time_type;
                state.daylight_savings = daylight_saving_enabled;
                state.use_utc = has_utc;
                state.utc_time = requested_time.value_or(std::chrono::system_clock::now());

                const auto requested_tm = to_utc_tm(state.utc_time.value());
                if (log_)
                {
                        log_->Debug("SetSystemDateAndTime requested at " + request_time_string(xml_tree));
                        log_->Debug("System time updated to " + tm_to_string(requested_tm));
                }

                auto envelope_tree = utility::soap::getEnvelopeTree(ns_);
                pt::ptree response_node;
                envelope_tree.add_child("s:Body.tds:SetSystemDateAndTimeResponse", response_node);

                pt::ptree root_tree;
                root_tree.put_child("s:Envelope", envelope_tree);

                std::ostringstream os;
                pt::write_xml(os, root_tree);

                utility::http::fillResponseWithHeaders(*response, os.str());
        }

private:
        void send_fault(std::shared_ptr<HttpServer::Response> response, const std::string& reason) const
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

        static bool parse_bool(const std::string& value, bool default_value)
        {
                if (value.empty())
                        return default_value;

                if (value == "1" || boost::iequals(value, "true"))
                        return true;
                if (value == "0" || boost::iequals(value, "false"))
                        return false;

                return default_value;
        }

        static std::optional<std::chrono::system_clock::time_point> parse_utc_time(const pt::ptree& xml_tree)
        {
                try
                {
                        const auto year = std::stoi(
                                        exns::find_hierarchy("Envelope.Body.SetSystemDateAndTime.UTCDateTime.Date.Year", xml_tree));
                        const auto month = std::stoi(
                                        exns::find_hierarchy("Envelope.Body.SetSystemDateAndTime.UTCDateTime.Date.Month", xml_tree));
                        const auto day = std::stoi(
                                        exns::find_hierarchy("Envelope.Body.SetSystemDateAndTime.UTCDateTime.Date.Day", xml_tree));
                        const auto hour = std::stoi(
                                        exns::find_hierarchy("Envelope.Body.SetSystemDateAndTime.UTCDateTime.Time.Hour", xml_tree));
                        const auto minute = std::stoi(
                                        exns::find_hierarchy("Envelope.Body.SetSystemDateAndTime.UTCDateTime.Time.Minute", xml_tree));
                        const auto second = std::stoi(
                                        exns::find_hierarchy("Envelope.Body.SetSystemDateAndTime.UTCDateTime.Time.Second", xml_tree));

                        std::tm tm{};
                        tm.tm_year = year - 1900;
                        tm.tm_mon = month - 1;
                        tm.tm_mday = day;
                        tm.tm_hour = hour;
                        tm.tm_min = minute;
                        tm.tm_sec = second;

                        const auto time_val = timegm(&tm);
                        if (time_val == -1)
                                return std::nullopt;

                        return std::chrono::system_clock::from_time_t(time_val);
                }
                catch (const std::exception&)
                {
                        return std::nullopt;
                }
        }

        static std::string request_time_string(const pt::ptree& xml_tree)
        {
                auto parsed_time = parse_utc_time(xml_tree);
                if (!parsed_time)
                        return "<invalid time>";

                const auto tm = to_utc_tm(parsed_time.value());
                return tm_to_string(tm);
        }

        const std::shared_ptr<ILogger> log_;
};

DeviceService::DeviceService(const std::string& service_uri, const std::string& service_name,
														 std::shared_ptr<IOnvifServer> srv)
		: IOnvifService(service_uri, service_name, srv)
{
	requestHandlers_.push_back(std::make_shared<GetCapabilitiesHandler>(xml_namespaces_, configs_ptree_,
																																			*srv->GetServerConfigs(), srv->ServerAddress()));
	requestHandlers_.push_back(std::make_shared<GetDeviceInformationHandler>(xml_namespaces_, configs_ptree_));
	requestHandlers_.push_back(std::make_shared<GetNetworkInterfacesHandler>(xml_namespaces_, configs_ptree_));
        requestHandlers_.push_back(std::make_shared<GetRelayOutputsHandler>(xml_namespaces_, configs_ptree_));
        requestHandlers_.push_back(
                        std::make_shared<GetServicesHandler>(xml_namespaces_, configs_ptree_, srv->ServerAddress()));
        requestHandlers_.push_back(std::make_shared<GetScopesHandler>(xml_namespaces_, configs_ptree_));
        requestHandlers_.push_back(std::make_shared<GetSystemDateAndTimeHandler>(xml_namespaces_, configs_ptree_));
        requestHandlers_.push_back(std::make_shared<SetSystemDateAndTimeHandler>(xml_namespaces_, configs_ptree_, log_));
}
} // namespace osrv