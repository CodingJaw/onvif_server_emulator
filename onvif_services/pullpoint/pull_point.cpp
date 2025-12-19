#include "pull_point.h"

#include "../utility/XmlParser.h"
#include "../utility/SoapHelper.h"
#include "../utility/HttpHelper.h"
#include "../utility/DateTime.hpp"


#include <sstream>
#include <algorithm>
#include <vector>
#include <optional>
#include <stdexcept>

#include <boost/property_tree/xml_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/algorithm/string/trim.hpp>

namespace osrv
{

	namespace event {

                void PullPoint::PullMessages(pull_messages_handler_t handler, std::shared_ptr<HttpServer::Response> response,
                        int timeout_seconds, int message_limit)
                {
                        is_client_waiting_ = true;

                        handler_ = handler;
                        response_writer_ = response;

                        current_timeout_interval_seconds_ = timeout_seconds > 0 ? timeout_seconds : timeout_interval_;
                        current_message_limit_ = message_limit > 0 ? std::min(message_limit, max_messages_) : max_messages_;

                        if (!events_.empty())
                        {
                                // Response to a subcriber immediately
                                response_to_pullmessages();
                        }

                        // Do charge the timeout timer
                        pullmessages_timer_.cancel();
                        pullmessages_timer_.expires_after(std::chrono::seconds(current_timeout_interval_seconds_));
                        pullmessages_timer_.async_wait([self = shared_from_this()](const boost::system::error_code& error) {
                                        if (error)
                                                return;

                                        self->response_to_pullmessages();
                                });
                }

                void PullPoint::Notify(NotificationMessage&& event)
                {
                        if (max_messages_ > 0 && events_.size() >= static_cast<size_t>(max_messages_))
                        {
                                const auto to_trim = events_.size() - static_cast<size_t>(max_messages_) + 1;
                                for (size_t i = 0; i < to_trim; ++i)
                                        events_.pop_front();
                        }

                        events_.push_back(std::move(event));

                        response_to_pullmessages();
                }
		
		void PullPoint::response_to_pullmessages()
		{
			if (!is_client_waiting_)
				return;

			// Do serialize all stored events

                        // Do copy only less then specified in a PullMessages messages limit
                        // FIX: in current implementation all events is copied
                        std::deque<NotificationMessage> copied_events;

                        const auto messages_to_copy = std::min(events_.size(), static_cast<size_t>(current_message_limit_));
                        for (size_t i = 0; i < messages_to_copy; ++i)
                        {
                                copied_events.push_back(std::move(events_.front()));
                                events_.pop_front();
                        }
                        handler_(shared_from_this(), std::move(copied_events), response_writer_);
                        response_writer_.reset(); // it's required to reset writer ptr, otherwise response will not be written in time
                        is_client_waiting_ = false;
                }
		
                void PullPoint::SetSynchronizationPoint()
                {

                        // I think we should clean already saved NotificationMessages
                        events_.clear();

			for (const auto eg : connected_generators_)
			{
				auto gen_ev = eg->GenerateSynchronizationEvent();
                                events_.insert(events_.end(), gen_ev.begin(), gen_ev.end());
                        }
                }

                void PullPoint::RefreshTerminationTimer(std::chrono::steady_clock::duration duration,
                        std::function<void(const std::string&)> on_expired)
                {
                        expiration_handler_ = std::move(on_expired);

                        termination_timer_.cancel();
                        termination_timer_.expires_after(duration);

                        auto self = shared_from_this();
                        termination_timer_.async_wait([self](const boost::system::error_code& error) {
                                        if (error)
                                                return;

                                        if (self->expiration_handler_)
                                                self->expiration_handler_(self->GetSubscriptionReference());
                                });
                }
		
                namespace
                {
                        constexpr const char* kConcreteTopicDialect = "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet";

                        std::vector<std::string> parse_topic_filters(const std::vector<TopicExpression>& topic_filters)
                        {
                                std::vector<std::string> parsed_filters;

                                for (const auto& filter : topic_filters)
                                {
                                        std::string dialect = filter.dialect;
                                        boost::algorithm::trim(dialect);

                                        if (!dialect.empty() && dialect != kConcreteTopicDialect)
                                        {
                                                throw std::invalid_argument("Unsupported TopicExpression Dialect: " + dialect);
                                        }

                                        std::string expression = filter.expression;
                                        boost::algorithm::trim(expression);

                                        if (expression.empty())
                                                continue;

                                        std::istringstream expressions_stream(expression);
                                        std::string token;
                                        while (expressions_stream >> token)
                                        {
                                                parsed_filters.push_back(std::move(token));
                                        }
                                }

                                return parsed_filters;
                        }

                        bool topic_matches_filters(const std::string& topic, const std::vector<std::string>& filters)
                        {
                                if (filters.empty())
                                        return true;

                                return std::any_of(filters.begin(), filters.end(), [&topic](const std::string& filter) {
                                        if (topic == filter)
                                                return true;

                                        const auto has_prefix = topic.size() > filter.size()
                                                && topic.compare(0, filter.size(), filter) == 0
                                                && topic[filter.size()] == '/';

                                        return has_prefix;
                                });
                        }
                }

                std::shared_ptr<PullPoint> NotificationsManager::CreatePullPoint(const std::vector<TopicExpression>& topic_filters)
                {
                        // When register a new PullPoint
                        // depending on subcription filter in a request
                        // need to connect a PullPoint instance only with appropriate event generators
                        // FIX: the current implementation connects PullPoint instances with all generators

                        // FIX: current implementation handles only 1 subscriber, if some pullpoint did not be renewed,
                        // it should be deleted by timeout
                        auto test_subscription_reference = "onvif/event_service/s" + std::to_string(subscription_counter_++);
                        auto pp = std::shared_ptr<PullPoint>(new PullPoint(test_subscription_reference, io_context_, *logger_));
                        pp->SetMaxMessages(max_message_limit_);
                        pp->SetTimeoutInterval(pullmessages_timeout_seconds_);

                        auto now = boost::posix_time::microsec_clock::universal_time();
                        auto termination_time = now + boost::posix_time::seconds(subscription_lease_seconds_);
                        pp->SetSubscriptionTimes(now, now, termination_time);
                        pp->RefreshTerminationTimer(std::chrono::seconds(subscription_lease_seconds_),
                                [this, weak_pp = std::weak_ptr<PullPoint>(pp)](const std::string& ref) {
                                        handle_expired_pullpoint(ref, weak_pp);
                                });

                        const auto parsed_filters = parse_topic_filters(topic_filters);
                        pullpoints_.push_back(pp);
                        for (auto& eg : event_generators_)
                        {
                                if (!topic_matches_filters(eg->Topic(), parsed_filters))
                                        continue;

                                auto signal_connection = eg->Connect([pp, this](NotificationMessage event_description) {
                                                pp->Notify(std::move(event_description));
                                        });
                                pp->AddGenerator(eg.get(), signal_connection);
                        }

                        return pp;
                }
		
                void NotificationsManager::PullMessages(std::shared_ptr<HttpServer::Response> response,
                        const std::string& subscription_reference, const std::string& msg_id, int timeout, int msg_limit)
                {
                        auto now = boost::posix_time::microsec_clock::universal_time();
                        prune_expired_pullpoints(now);

                        auto pp_it = find_pullpoint(pullpoints_, subscription_reference);

                        if (pp_it != pullpoints_.end())
                        {
                                now = boost::posix_time::microsec_clock::universal_time();
                                if ((*pp_it)->IsExpired(now))
                                {
                                        (*pp_it)->DisconnectFromGenerators();
                                        pullpoints_.erase(pp_it);

                                        logger_->Error("PullMessages received for expired subscription: " + subscription_reference);
                                        auto envelope_tree = utility::soap::getEnvelopeTree(*xml_namespaces_);
                                        boost::property_tree::ptree code_node;
                                        code_node.add("s:Value", "s:Sender");
                                        code_node.add("s:Subcode.s:Value", "wstop:ResourceUnknown");
                                        envelope_tree.add_child("s:Body.s:Fault.s:Code", code_node);
                                        envelope_tree.put("s:Body.s:Fault.s:Reason.s:Text", "Subscription expired");
                                        envelope_tree.put("s:Body.s:Fault.s:Reason.s:Text.<xmlattr>.xml:lang", "en");

                                        boost::property_tree::ptree root_tree;
                                        root_tree.put_child("s:Envelope", envelope_tree);

                                        std::ostringstream os;
                                        boost::property_tree::write_xml(os, root_tree);

                                        utility::http::fillResponseWithHeaders(*response, os.str(), utility::http::ClientErrorDefaultWriter);
                                        return;
                                }

                                (*pp_it)->PullMessages([msg_id, this](std::shared_ptr<PullPoint> pullpoint, std::deque<NotificationMessage> events,
                                                std::shared_ptr<HttpServer::Response> response) {
                                                do_pullmessages_response(pullpoint, msg_id, std::move(events), response);
                                        }, response, timeout, msg_limit);
                        }
                        else
                        {
                                // ? Need to check specification, more likely it's need to response with an error code
                                logger_->Error("Not found subscription reference: " + subscription_reference);
                                auto envelope_tree = utility::soap::getEnvelopeTree(*xml_namespaces_);

                                boost::property_tree::ptree code_node;
                                code_node.add("s:Value", "s:Sender");
                                code_node.add("s:Subcode.s:Value", "ter:InvalidArgVal");
                                envelope_tree.add_child("s:Body.s:Fault.s:Code", code_node);
                                envelope_tree.put("s:Body.s:Fault.s:Reason.s:Text", "Unknown SubscriptionReference");
                                envelope_tree.put("s:Body.s:Fault.s:Reason.s:Text.<xmlattr>.xml:lang", "en");

                                boost::property_tree::ptree root_tree;
                                root_tree.put_child("s:Envelope", envelope_tree);

                                std::ostringstream os;
                                boost::property_tree::write_xml(os, root_tree);

                                utility::http::fillResponseWithHeaders(*response, os.str(), utility::http::ClientErrorDefaultWriter);
                                return;
                        }
                }

		void NotificationsManager::SetSynchronizationPoint(const std::string& subscr_ref)
		{
			auto pp_it = find_pullpoint(pullpoints_, subscr_ref);

			if (pp_it == pullpoints_.end())
			{
				throw std::runtime_error("Invalid subscription reference");
			}

			(*pp_it)->SetSynchronizationPoint();
		}

		void NotificationsManager::Unsubscribe(const std::string& subscription_reference)
		{
                        auto pp_it = find_pullpoint(pullpoints_, subscription_reference);
                        if (pp_it != pullpoints_.end())
                        {
                                (*pp_it)->CancelTerminationTimer();
                                (*pp_it)->DisconnectFromGenerators();
                                pullpoints_.erase(pp_it);
                        }
			else
			{
				// TODO: Probably it should be throwed an exception
			}
		}

                void NotificationsManager::Renew(std::shared_ptr<HttpServer::Response> response, const std::string& header_to, const std::string& header_msg_id,
                        std::optional<int> requested_lease_seconds)
                {
                        if (!xml_namespaces_)
                                throw std::runtime_error("XML namespaces not initialized in NotificationManager!");

                        logger_->Debug("Sending RenewRequest: " + header_to);

                        auto now = boost::posix_time::microsec_clock::universal_time();
                        prune_expired_pullpoints(now);

                        auto pp_it = find_pullpoint(pullpoints_, header_to);
                        if (pp_it == pullpoints_.end())
                        {
                                auto envelope_tree = utility::soap::getEnvelopeTree(*xml_namespaces_);

                                boost::property_tree::ptree code_node;
                                code_node.add("s:Value", "s:Sender");
                                code_node.add("s:Subcode.s:Value", "ter:InvalidArgVal");
                                envelope_tree.add_child("s:Body.s:Fault.s:Code", code_node);
                                envelope_tree.put("s:Body.s:Fault.s:Reason.s:Text", "Unknown SubscriptionReference");
                                envelope_tree.put("s:Body.s:Fault.s:Reason.s:Text.<xmlattr>.xml:lang", "en");

                                boost::property_tree::ptree root_tree;
                                root_tree.put_child("s:Envelope", envelope_tree);

                                std::ostringstream os;
                                boost::property_tree::write_xml(os, root_tree);

                                utility::http::fillResponseWithHeaders(*response, os.str(), utility::http::ClientErrorDefaultWriter);
                                return;
                        }

                        if ((*pp_it)->IsExpired(now))
                        {
                                (*pp_it)->DisconnectFromGenerators();
                                pullpoints_.erase(pp_it);

                                auto envelope_tree = utility::soap::getEnvelopeTree(*xml_namespaces_);
                                boost::property_tree::ptree code_node;
                                code_node.add("s:Value", "s:Sender");
                                code_node.add("s:Subcode.s:Value", "wstop:ResourceUnknown");
                                envelope_tree.add_child("s:Body.s:Fault.s:Code", code_node);
                                envelope_tree.put("s:Body.s:Fault.s:Reason.s:Text", "Subscription expired");
                                envelope_tree.put("s:Body.s:Fault.s:Reason.s:Text.<xmlattr>.xml:lang", "en");

                                boost::property_tree::ptree root_tree;
                                root_tree.put_child("s:Envelope", envelope_tree);

                                std::ostringstream os;
                                boost::property_tree::write_xml(os, root_tree);

                                utility::http::fillResponseWithHeaders(*response, os.str(), utility::http::ClientErrorDefaultWriter);
                                return;
                        }

                        const int requested_seconds = requested_lease_seconds.value_or(subscription_lease_seconds_);
                        const int max_lease_seconds = std::max(subscription_lease_seconds_, 1);
                        const int granted_seconds = std::clamp(requested_seconds, 1, max_lease_seconds);

                        bool renewed = (*pp_it)->TryRenew(now,
                                        boost::posix_time::seconds(granted_seconds),
                                        boost::posix_time::seconds(min_renew_interval_seconds_));
                        if (renewed)
                        {
                                (*pp_it)->RefreshTerminationTimer(std::chrono::seconds(granted_seconds),
                                [this, weak_pp = std::weak_ptr<PullPoint>(*pp_it)](const std::string& ref) {
                                        handle_expired_pullpoint(ref, weak_pp);
                                });
                        }
                        else
                        {
                                logger_->Warn("Ignoring rapid renew request for subscription: " + header_to);
                        }

                        namespace pt = boost::property_tree;

                        pt::ptree analytics_configs;
                        auto envelope_tree = utility::soap::getEnvelopeTree(*xml_namespaces_);

			envelope_tree.add("s:Header.wsa:MessageID", header_msg_id);
			envelope_tree.add("s:Header.wsa:To", "http://www.w3.org/2005/08/addressing/anonymous");
			envelope_tree.add("s:Header.wsa:Action", "http://docs.oasis-open.org/wsn/bw-2/SubscriptionManager/RenewResponse");

                        pt::ptree response_node;
                        response_node.add("wsnt:TerminationTime", (*pp_it)->GetTerminationTime());
                        response_node.add("wsnt:CurrentTime", (*pp_it)->GetLastRenew());
                        envelope_tree.add_child("s:Body.wsnt:RenewResponse", response_node);

			pt::ptree root_tree;
			root_tree.put_child("s:Envelope", envelope_tree);

			std::ostringstream os;
			pt::write_xml(os, root_tree);

                        utility::http::fillResponseWithHeaders(*response, os.str());
                }

                void NotificationsManager::handle_expired_pullpoint(const std::string& ref,
                        const std::weak_ptr<PullPoint>& weak_pullpoint)
                {
                        auto shared_pp = weak_pullpoint.lock();
                        if (shared_pp)
                                shared_pp->DisconnectFromGenerators();

                        auto it = find_pullpoint(pullpoints_, ref);
                        if (it != pullpoints_.end())
                                pullpoints_.erase(it);
                }

                void NotificationsManager::prune_expired_pullpoints(const boost::posix_time::ptime& now)
                {
                        pullpoints_.erase(std::remove_if(pullpoints_.begin(), pullpoints_.end(),
                                [this, &now](const std::shared_ptr<PullPoint>& pullpoint) {
                                        if (!pullpoint->IsExpired(now))
                                                return false;

                                        pullpoint->CancelTerminationTimer();
                                        pullpoint->DisconnectFromGenerators();
                                        return true;
                                }),
                                pullpoints_.end());
                }
		
		void NotificationsManager::Run()
		{
			for (auto& eg : event_generators_)
			{
				eg->Run();
			}

			io_work_ = std::unique_ptr<work_t>(new work_t(io_context_));
			
			worker_thread_ = std::unique_ptr<std::thread>(new std::thread(
				[this]() {
					io_context_.run();
				}
			));

			logger_->Debug("NotificationsManager is run successfully");
		}

                void NotificationsManager::do_pullmessages_response(std::shared_ptr<PullPoint> pullpoint, const std::string& msg_id,
                        std::deque<NotificationMessage>&& events, std::shared_ptr<HttpServer::Response> response)
                {
                        logger_->Debug("Sending PullPoint response with msg id: " + msg_id
                                + ", subscription: " + pullpoint->GetSubscriptionReference());

			/**
				PullMessagesResponse response format:
				CurrentTime
				TerminationTime
				NotificationMessage
			*/

			if (!xml_namespaces_)
				throw std::runtime_error("XML namespaces not initialized in NotificationManager!");

			namespace pt = boost::property_tree;

			pt::ptree analytics_configs;
			auto envelope_tree = utility::soap::getEnvelopeTree(*xml_namespaces_);

			envelope_tree.add("s:Header.wsa:MessageID", msg_id);
			envelope_tree.add("s:Header.wsa:To", "http://www.w3.org/2005/08/addressing/anonymous");
			envelope_tree.add("s:Header.wsa:Action", "http://www.onvif.org/ver10/events/wsdl/PullPointSubscription/PullMessagesResponse");

                        pt::ptree response_node = serialize_notification_messages(events, *pullpoint);

			envelope_tree.add_child("s:Body.tet:PullMessagesResponse", response_node);

			pt::ptree root_tree;
			root_tree.put_child("s:Envelope", envelope_tree);

			std::ostringstream os;
			pt::write_xml(os, root_tree);

			utility::http::fillResponseWithHeaders(*response, os.str());
		}

                bool compare_subscription_references(const std::string& full_ref, const std::string& short_ref)
                {
                        if (full_ref.size() < short_ref.size())
                                return false;

                        const auto suffix_pos = full_ref.size() - short_ref.size();
                        const auto is_suffix = full_ref.compare(suffix_pos, short_ref.size(), short_ref) == 0;
                        if (!is_suffix)
                                return false;

                        return suffix_pos == 0 || full_ref[suffix_pos - 1] == '/';
                }

                boost::property_tree::ptree serialize_notification_messages(std::deque<NotificationMessage>& msgs,
                        const PullPoint& pullpoint)
                {
                        namespace pt = boost::property_tree;
                        pt::ptree result;

                        namespace ptime = boost::posix_time;
                        result.add("tet:CurrentTime", utility::datetime::system_utc_datetime());

                        result.add("tet:TerminationTime", pullpoint.GetTerminationTime());

                        const auto& subscription_reference = pullpoint.GetSubscriptionAddress();
                        const auto& producer_reference = pullpoint.GetServiceEndpoint();

                        while(!msgs.empty())
                        {
                                auto msg = msgs.front();
                                msgs.pop_front();

                                pt::ptree msg_node;
                                if (!subscription_reference.empty())
                                        msg_node.add("wsnt:SubscriptionReference.wsa:Address", subscription_reference);

                                if (!producer_reference.empty())
                                        msg_node.add("wsnt:ProducerReference.wsa:Address", producer_reference);

                                msg_node.add("wsnt:Topic", msg.topic);
                                msg_node.add("wsnt:Topic.<xmlattr>.Dialect", "http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet");

                                msg_node.add("wsnt:Message.tt:Message.<xmlattr>.PropertyOperation", msg.property_operation);
                                msg_node.add("wsnt:Message.tt:Message.<xmlattr>.UtcTime", msg.utc_time);

                                for (const auto& [name, value] : msg.source_item_descriptions)
                                {
                                        pt::ptree item_descr;
                                        item_descr.add("<xmlattr>.Name", name);
                                        item_descr.add("<xmlattr>.Value", value);
                                        msg_node.add_child("wsnt:Message.tt:Message.tt:Source.tt:SimpleItem", item_descr);
                                }

                                msg_node.add("wsnt:Message.tt:Message.tt:Data.tt:SimpleItem.<xmlattr>.Value", msg.data_value);
                                msg_node.add("wsnt:Message.tt:Message.tt:Data.tt:SimpleItem.<xmlattr>.Name", msg.data_name);

                                result.add_child("wsnt:NotificationMessage", msg_node);
                        }

                        return result;
                }

		PullPoints_t::const_iterator find_pullpoint(const PullPoints_t& pullpoints, const std::string& subscription_reference)
		{
			return std::find_if(pullpoints.begin(), pullpoints.end(),
				[subscription_reference](PullPoints_t::value_type pp) {

					return compare_subscription_references(subscription_reference,
						pp->GetSubscriptionReference());

				});
		}

	}
}
