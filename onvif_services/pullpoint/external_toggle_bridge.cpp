#include "external_toggle_bridge.h"

#include "pull_point.h"

#include <algorithm>
#include <boost/asio.hpp>
#include <mutex>
#include <thread>

namespace
{
        std::mutex generator_mutex;
        std::weak_ptr<osrv::event::ExternalToggleEventGenerator> registered_generator;

        class ExternalToggleSocket
        {
        public:
                ExternalToggleSocket(uint16_t port, std::string address, const osrv::ILogger& logger)
                        : endpoint_address_(std::move(address)), logger_(&logger), acceptor_(io_context_)
                {
                        boost::asio::ip::tcp::endpoint endpoint{
                                boost::asio::ip::make_address(endpoint_address_), port};
                        acceptor_.open(endpoint.protocol());
                        acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
                        acceptor_.bind(endpoint);
                        acceptor_.listen();
                }

                ~ExternalToggleSocket()
                {
                        Stop();
                }

                void Start()
                {
                        worker_thread_ = std::thread([this]() {
                                do_accept();
                                io_context_.run();
                        });
                }

                void Stop()
                {
                        io_context_.stop();
                        if (worker_thread_.joinable())
                                worker_thread_.join();
                }

        private:
                void do_accept()
                {
                        auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_context_);
                        acceptor_.async_accept(*socket, [this, socket](const boost::system::error_code& ec) {
                                if (!ec)
                                {
                                        handle_socket(socket);
                                }
                                if (!io_context_.stopped())
                                        do_accept();
                        });
                }

                void handle_socket(const std::shared_ptr<boost::asio::ip::tcp::socket>& socket)
                {
                        auto buffer = std::make_shared<boost::asio::streambuf>();
                        boost::asio::async_read_until(*socket, *buffer, '\n',
                                [this, socket, buffer](const boost::system::error_code& ec, std::size_t) {
                                        if (!ec)
                                        {
                                                std::istream is(buffer.get());
                                                std::string line;
                                                std::getline(is, line);
                                                auto lowered = line;
                                                std::transform(lowered.begin(), lowered.end(), lowered.begin(), ::tolower);

                                                bool state = lowered == "1" || lowered == "on" || lowered == "true";
                                                osrv::event::ExternalToggleTrigger(state);

                                                if (logger_)
                                                        logger_->Debug("External toggle updated via TCP: " + lowered);
                                        }
                                        socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both);
                                        socket->close();
                                });
                }

                std::string endpoint_address_;
                const osrv::ILogger* logger_ = nullptr;
                boost::asio::io_context io_context_;
                boost::asio::ip::tcp::acceptor acceptor_;
                std::thread worker_thread_;
        };

        std::unique_ptr<ExternalToggleSocket> socket_server;
}

namespace osrv
{
    namespace event
    {
        void RegisterExternalToggleGenerator(const std::shared_ptr<ExternalToggleEventGenerator>& generator)
        {
                std::scoped_lock lk(generator_mutex);
                registered_generator = generator;
        }

        void ExternalToggleTrigger(bool state)
        {
                std::shared_ptr<ExternalToggleEventGenerator> locked;
                {
                        std::scoped_lock lk(generator_mutex);
                        locked = registered_generator.lock();
                }

                if (locked)
                        locked->Trigger(state);
        }

        bool HasExternalToggleGenerator()
        {
                std::scoped_lock lk(generator_mutex);
                return !registered_generator.expired();
        }

        void StartExternalToggleSocket(uint16_t port, const std::string& address, const ILogger& logger)
        {
                std::scoped_lock lk(generator_mutex);
                if (socket_server)
                        return;

                socket_server.reset(new ExternalToggleSocket(port, address, logger));
                socket_server->Start();
        }

        void StopExternalToggleSocket()
        {
                std::scoped_lock lk(generator_mutex);
                if (socket_server)
                {
                        socket_server->Stop();
                        socket_server.reset();
                }
        }
    }
}

extern "C"
{
        void osrv_external_toggle_trigger(int state)
        {
                osrv::event::ExternalToggleTrigger(state != 0);
        }

        int osrv_external_toggle_available()
        {
                return osrv::event::HasExternalToggleGenerator() ? 1 : 0;
        }
}

