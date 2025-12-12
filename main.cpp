#include <iostream>

#include "Logger.h"
#include "LoggerFactories.h"
#include "Server.h"

#include "onvif_services/service_configs.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <numeric>
#include <sstream>
#include <string>
#include <vector>

static const std::string TITLE = R"(
   ___  _   ___     _____ _____   ____                             _____                 _       _             
  / _ \| \ | \ \   / /_ _|  ___| / ___|  ___ _ ____   _____ _ __  | ____|_ __ ___  _   _| | __ _| |_ ___  _ __ 
 | | | |  \| |\ \ / / | || |_    \___ \ / _ \ '__\ \ / / _ \ '__| |  _| | '_ ` _ \| | | | |/ _` | __/ _ \| '__|
 | |_| | |\  | \ V /  | ||  _|    ___) |  __/ |   \ V /  __/ |    | |___| | | | | | |_| | | (_| | || (_) | |   
  \___/|_| \_|  \_/  |___|_|     |____/ \___|_|    \_/ \___|_|    |_____|_| |_| |_|\__,_|_|\__,_|\__\___/|_|   
)";

static const std::string SERVER_VERSION = "0.1";

static const std::string DEFAULT_CONFIGS_DIR = "./server_configs";

static const std::string LOG_FILENAME = "OnvifServer.log";

int main(int argc, char** argv)
{
	using namespace std;
	std::cout << TITLE << std::endl;
	std::cout << "Application version: " << SERVER_VERSION << std::endl;

        namespace fs = std::filesystem;

        fs::path configs_dir = DEFAULT_CONFIGS_DIR;

	std::stringstream ss;

	if (argc > 1)
	{
                configs_dir = argv[1];
                ss << "Command line arguments are found. Configs read from " << configs_dir;
        }

        const auto executable_dir = fs::absolute(fs::path(argv[0])).parent_path();
        const std::vector<fs::path> config_candidates = {
                configs_dir,
                executable_dir / configs_dir,
                executable_dir / ".." / "server_configs"
        };

        auto resolved_config_dir = std::find_if(config_candidates.begin(), config_candidates.end(), [](const fs::path& candidate) {
                std::error_code ec;
                return fs::exists(candidate, ec) && fs::is_directory(candidate, ec);
        });

        if (resolved_config_dir == config_candidates.end())
        {
                throw std::runtime_error("No valid configuration directory found. Tried: " +
                                         std::accumulate(std::next(config_candidates.begin()), config_candidates.end(),
                                                         config_candidates.front().string(),
                                                         [](std::string acc, const fs::path& val) {
                                                                acc += ", " + val.string();
                                                                return acc;
                                                         }));
        }

        configs_dir = resolved_config_dir->lexically_normal();
        const auto configs_dir_str = configs_dir.string();

        std::shared_ptr<boost::property_tree::ptree> serverConfigs = osrv::ServiceConfigs("common", configs_dir_str);
	std::shared_ptr<ILogger> logger;
	LoggerConfigs lconfigs(serverConfigs);
	if (lconfigs.LogOutput() == "console")
	{
		logger.reset(ConsoleLoggerFactory().GetLogger(lconfigs.LogLevel()));
	}
	else if (lconfigs.LogOutput() == "file")
	{
		logger.reset(FileLoggerFactory(LOG_FILENAME).GetLogger(lconfigs.LogLevel()));
	}
	else
		throw std::runtime_error("Could not create logger type: " + lconfigs.LogOutput());

	logger->Info("New run. " + ss.str());
	logger->Info("Logging level: " + logger->GetLogLevel());

        const char* env_gst_plugin_path_raw = std::getenv("GST_PLUGIN_PATH");
        const std::string env_gst_plugin_path = env_gst_plugin_path_raw ? env_gst_plugin_path_raw : "";

        if (env_gst_plugin_path.empty())
        {
                const std::string msg =
                        "For proper work please install required GStreamer plugins and add the GST_PLUGIN_PATH environment "
                        "variable to point at the installation directory!";
                logger->Warn(msg);
                std::cerr << msg << std::endl;
        }
        else
        {
                logger->Debug("Used GStreamer plugins directory: " + env_gst_plugin_path);
        }

	try
	{
		// std::shared_ptr<osrv::IOnvifServer> server = std::make_shared<osrv::Server>(configs_dir, logger);
                std::shared_ptr<osrv::Server> server{std::make_shared<osrv::Server>(configs_dir_str, logger)};
		server->init();
		server->run();
	}
	catch (const std::exception& e)
	{
		std::string msg = "Issues with the Server's work: ";
		msg += e.what();
		logger->Error(msg);
	}

	logger->Info("Server is stopped");

	std::cout << "\n\nPress any key to exit";
	getchar();
}
