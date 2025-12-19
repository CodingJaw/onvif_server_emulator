#pragma once

#include "../Logger.h"
#include "../utility/DateTime.hpp"
#include "event_generators.h"

#include <deque>
#include <string>
#include <thread>
#include <memory>
#include <chrono>
#include <vector>
#include <optional>
#include <utility>

#include <boost/asio.hpp>
#include <boost/signals2.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/property_tree/ptree_fwd.hpp>

#include "../HttpServerFwd.h"
#include "../Simple-Web-Server/server_http.hpp"

namespace
{
	using StringPairsList_t = std::vector<std::pair<std::string, std::string>>;
}

namespace osrv
{
	namespace event
	{
                struct NotificationMessage
                {
                        std::string topic;
                        std::string utc_time;
                        std::string property_operation;

			// { name, value }
			StringPairsList_t source_item_descriptions;

                        std::string data_name;
                        std::string data_value;
                };

                struct TopicExpression
                {
                        std::string dialect;
                        std::string expression;
                };

                class PullPoint : public std::enable_shared_from_this<PullPoint>
                {
                public:

                        using pull_messages_handler_t = std::function<void(std::shared_ptr<PullPoint> pullpoint,
                                std::deque<NotificationMessage>&& events,
                                std::shared_ptr<HttpServer::Response>)>;

                        PullPoint(const std::string& subscription_reference, boost::asio::io_context& io_context, const ILogger& logger)
                                : logger_(&logger)
                                , io_context_(io_context)
                                , subscription_ref_(subscription_reference)
                                , pullmessages_timer_(io_context)
                                , termination_timer_(io_context)
                                , max_messages_(50)
                                , current_message_limit_(50)
                                , is_client_waiting_(false)
                        {
                        }

                        ~PullPoint()
                        {
                                pullmessages_timer_.cancel();
                                termination_timer_.cancel();
                                logger_->Debug("Destroying PullPoint: " + subscription_ref_);
                        }

			// Link a connected generator and set a related connection
			// NOTE: if this action is not done during initialization,
			// SetSynchronizationPoint() will return empty list
			void AddGenerator(const IEventGenerator* eg, boost::signals2::connection signal_connection)
			{
				if (eg)
				{
					connected_generators_.push_back(eg);
					signal_connections_.push_back(signal_connection);
				}
			}

			void DisconnectFromGenerators()
			{
				for (const auto& s : signal_connections_)
				{
					s.disconnect();
				}
			}

                        std::string GetSubscriptionReference() const
                        {
                                return subscription_ref_;
                        }

                        void SetSubscriptionAddress(std::string address)
                        {
                                subscription_address_ = std::move(address);
                        }

                        const std::string& GetSubscriptionAddress() const
                        {
                                return subscription_address_;
                        }

                        void SetServiceEndpoint(std::string endpoint)
                        {
                                service_endpoint_ = std::move(endpoint);
                        }

                        const std::string& GetServiceEndpoint() const
                        {
                                return service_endpoint_;
                        }

                        // This method is called when a subscriber want to pull events
                        void PullMessages(pull_messages_handler_t handler, std::shared_ptr<HttpServer::Response> response,
                                int timeout_seconds, int message_limit);

			// This is method by which event generators should pass events,
			// a new event should be stored to the queue
			void Notify(NotificationMessage&& event);

			void SetSynchronizationPoint();

                        std::string GetLastRenew() const
                        {
                                return utility::datetime::posix_datetime_to_utc(last_renew_time_);
                        }

                        const boost::posix_time::ptime& GetLastRenewTimePoint() const
                        {
                                return last_renew_time_;
                        }

                        std::string GetTerminationTime() const
                        {
                                return utility::datetime::posix_datetime_to_utc(termination_time_);
                        }

                        const boost::posix_time::ptime& GetTerminationTimePoint() const
                        {
                                return termination_time_;
                        }

                        void SetSubscriptionTimes(const boost::posix_time::ptime& created_at,
                                const boost::posix_time::ptime& last_renew_at,
                                const boost::posix_time::ptime& termination_time)
                        {
                                creation_time_ = created_at;
                                last_renew_time_ = last_renew_at;
                                termination_time_ = termination_time;
                        }

                        bool TryRenew(const boost::posix_time::ptime& requested_at,
                                const boost::posix_time::time_duration& lease_duration,
                                const boost::posix_time::time_duration& min_renew_interval)
                        {
                                if (!last_renew_time_.is_not_a_date_time() && (requested_at - last_renew_time_) < min_renew_interval)
                                {
                                        return false;
                                }

                                last_renew_time_ = requested_at;
                                termination_time_ = requested_at + lease_duration;

                                return true;
                        }

                        void RefreshTerminationTimer(std::chrono::steady_clock::duration duration,
                                std::function<void(const std::string&)> on_expired);

                        void CancelTerminationTimer()
                        {
                                termination_timer_.cancel();
                        }

                        void SetMaxMessages(size_t n)
                        {
                                max_messages_ = n;
                        }

                        void SetTimeoutInterval(int seconds)
                        {
                                timeout_interval_ = seconds;
                        }

                        bool IsExpired(const boost::posix_time::ptime& now) const
                        {
                                return !termination_time_.is_not_a_date_time() && now >= termination_time_;
                        }

		protected:
			// This is called in 3 cases:
			// 1. when PullMessages requested and the event's queue is not empty (response immediately)
			// 2. when a new event is generated
			// 3. by timeout timer, if there are no events were generated (response with an empty message)
			void response_to_pullmessages();

		private:
			const ILogger* logger_;
			boost::asio::io_context& io_context_;
                        boost::asio::steady_timer pullmessages_timer_;
                        boost::asio::steady_timer termination_timer_;

                        boost::posix_time::ptime creation_time_;
                        boost::posix_time::ptime last_renew_time_;
                        boost::posix_time::ptime termination_time_;

                        const std::string subscription_ref_;
                        int timeout_interval_ = 60;
                        int current_timeout_interval_seconds_ = timeout_interval_;

                        int max_messages_ = 50;
                        int current_message_limit_ = 50;

			std::deque<NotificationMessage> events_;

                        pull_messages_handler_t handler_;
                        std::shared_ptr<HttpServer::Response> response_writer_;

                        bool is_client_waiting_;

                        std::function<void(const std::string&)> expiration_handler_;

                        // supposed to used only to get SynchronizationPoint
                        std::vector<const IEventGenerator*> connected_generators_;
                        std::vector<boost::signals2::connection> signal_connections_;

                        std::string subscription_address_;
                        std::string service_endpoint_;
                };
		using PullPoints_t = std::vector<std::shared_ptr<PullPoint>>;

		// NotificationsManager class links clients, PullPoint instances and event generators.
		// Logic of their cooperation work is implemented in this class.
		class NotificationsManager
		{
		public:
                        NotificationsManager(const ILogger& logger, const std::map<std::string, std::string>& xml_namespaces,
                                int subscription_lease_seconds, int min_renew_interval_seconds, int pullmessages_timeout_seconds)
                                : logger_(&logger)
                                , subscription_lease_seconds_(subscription_lease_seconds)
                                , min_renew_interval_seconds_(min_renew_interval_seconds)
                                , pullmessages_timeout_seconds_(pullmessages_timeout_seconds)
                        {
                                // XML namespaces are those, which added in the beginning of responses
                                xml_namespaces_ = &xml_namespaces;
                        }

			// This method is used to handle corresponding Onvif PullPoint subscription request
			// It's required to generate unique link for each subscriber 
			// Also need to schedule a subscription expiration timeout - and in that case delete subscription
			// Returns the created subscription's reference
                        std::shared_ptr<PullPoint> CreatePullPoint(const std::vector<TopicExpression>& topic_filters);

			// If there are messages for specified subscriber - return them immediately
			// Otherwise wait until timeout or any events will be generated 
			void PullMessages(std::shared_ptr<HttpServer::Response> /*response*/,
				const std::string& /*subscription_reference*/, const std::string& /*msg_id*/, int /*timeout*/, int /*msg_limit*/);

			void SetSynchronizationPoint(const std::string& /*subscr_ref*/);

			// Delete PullPoint and cancel all related timers
			void Unsubscribe(const std::string& /*subscription_reference*/);

                        void Renew(std::shared_ptr<HttpServer::Response> /*response*/,
                                const std::string& /*header_to*/,
                                const std::string& /*header_msg_id*/,
                                std::optional<int> requested_lease_seconds);

			void Run();

			void AddGenerator(std::shared_ptr<IEventGenerator> eg)
			{
				event_generators_.push_back(eg);
			}

			boost::asio::io_context& GetIoContext()
			{
				return io_context_;
			}

			~NotificationsManager() {}

		private:
                        void do_pullmessages_response(std::shared_ptr<PullPoint> /*pullpoint*/, const std::string& /*msg_id*/,
                                std::deque<NotificationMessage>&& /*events*/, std::shared_ptr<HttpServer::Response> /*response*/);

		private:
			const ILogger* logger_;

                        boost::asio::io_context io_context_;
                        using work_t = boost::asio::io_context::work;
                        std::unique_ptr<work_t> io_work_;
                        std::unique_ptr<std::thread> worker_thread_;

			// each subcriber have it's PullPoint instance
			std::vector<std::shared_ptr<PullPoint>> pullpoints_;

                        std::vector<std::shared_ptr<IEventGenerator>> event_generators_;

                        const std::map<std::string, std::string>* xml_namespaces_ = nullptr;

                        int subscription_lease_seconds_ = 300;
                        int min_renew_interval_seconds_ = 1;
                        int pullmessages_timeout_seconds_ = 60;
                        size_t subscription_counter_ = 0;
                };

		struct PullMessagesRequest
		{
			std::string timeout;
			int messages_limit;

			std::string header_action;
			std::string header_to;
			std::string msg_id;
		};

		void pullmessages_response_to_soap(PullPoint);

		//return compare references without address, only end
		bool compare_subscription_references(const std::string& /*ref_with_address_prefix*/,
			const std::string& /*reference_path*/);

                boost::property_tree::ptree serialize_notification_messages(std::deque<NotificationMessage>& /*messages*/,
                        const PullPoint& /*pullpoint*/);

		PullPoints_t::const_iterator find_pullpoint(const PullPoints_t& /*pullpoints*/,
			const std::string& /*subscription_reference*/);
	}

}