#include "event_generators.h"
#include "pull_point.h"

#include "../utility/DateTime.hpp"

#include <algorithm>
namespace osrv
{
	namespace event
	{

		DInputEventGenerator::DInputEventGenerator(int interval, const std::string& topic, boost::asio::io_context& io_context, const ILogger& logger_)
			: IEventGenerator(interval, topic, io_context, logger_)
		{
		}

                void DInputEventGenerator::SetDigitalInputsList(const DigitalInputsList& di_list)
                {
                        di_list_ = &di_list;

                        known_states_.clear();

                        for (const auto& input : di_list)
                        {
                                known_states_[input->GetToken()] = InputState{ input->IsEnabled(), input->GetState() };
                        }
                }

                std::deque<NotificationMessage> DInputEventGenerator::GenerateSynchronizationEvent() const
                {
                        TRACE_LOG(logger_);

			if (!di_list_)
				return {};

                        std::deque<NotificationMessage> result;
                        for (const auto& di : *di_list_)
                        {
                                const auto state = InputState{ di->IsEnabled(), di->GetState() };
                                known_states_[di->GetToken()] = state;

                                NotificationMessage nm;
                                nm.topic = notifications_topic_;
                                nm.utc_time = utility::datetime::system_utc_datetime();
                                nm.property_operation = "Initialized";
                                nm.source_item_descriptions.push_back({"InputToken", di->GetToken()});
                                nm.data_name = "LogicalState";
                                nm.data_value = state.enabled && state.state ? "true" : "false";

                                result.push_back(nm);
                        }

                        return result;
		}

                void DInputEventGenerator::generate_event()
                {
                        TRACE_LOG(logger_);

                        if (!di_list_)
                                return;

                        for (const auto& di : *di_list_)
                        {
                                const auto current_state = InputState{ di->IsEnabled(), di->GetState() };
                                auto it = known_states_.find(di->GetToken());

                                bool changed = false;
                                if (it == known_states_.end())
                                {
                                        changed = true;
                                        known_states_[di->GetToken()] = current_state;
                                }
                                else if (it->second.enabled != current_state.enabled || it->second.state != current_state.state)
                                {
                                        changed = true;
                                        it->second = current_state;
                                }

                                if (!changed)
                                        continue;

                                NotificationMessage nm;
                                nm.topic = notifications_topic_;
                                nm.utc_time = utility::datetime::system_utc_datetime();
                                nm.property_operation = "Changed";
                                nm.source_item_descriptions.push_back({"InputToken", di->GetToken()});
                                nm.data_name = "LogicalState";
                                nm.data_value = current_state.enabled && current_state.state ? "true" : "false";

                                event_signal_(nm);
                        }
                }

                DOutputEventGenerator::DOutputEventGenerator(int interval, const std::string& topic,
                        boost::asio::io_context& io_context, const ILogger& logger_)
                        : IEventGenerator(interval, topic, io_context, logger_)
                {
                }

                void DOutputEventGenerator::SetDigitalOutputsList(const DigitalOutputsList& do_list)
                {
                        do_list_ = &do_list;

                        known_states_.clear();

                        for (const auto& output : do_list)
                        {
                                known_states_[output->GetToken()] = OutputState{ output->IsEnabled(), output->GetState() };
                        }
                }

                std::deque<NotificationMessage> DOutputEventGenerator::GenerateSynchronizationEvent() const
                {
                        TRACE_LOG(logger_);

                        if (!do_list_)
                                return {};

                        std::deque<NotificationMessage> result;
                        for (const auto& output : *do_list_)
                        {
                                const auto state = OutputState{ output->IsEnabled(), output->GetState() };
                                known_states_[output->GetToken()] = state;

                                NotificationMessage nm;
                                nm.topic = notifications_topic_;
                                nm.utc_time = utility::datetime::system_utc_datetime();
                                nm.property_operation = "Initialized";
                                nm.source_item_descriptions.push_back({"RelayToken", output->GetToken()});
                                nm.data_name = "LogicalState";
                                nm.data_value = state.enabled && state.state ? "true" : "false";

                                result.push_back(nm);
                        }

                        return result;
                }

                void DOutputEventGenerator::generate_event()
                {
                        TRACE_LOG(logger_);

                        if (!do_list_)
                                return;

                        for (const auto& output : *do_list_)
                        {
                                const auto current_state = OutputState{ output->IsEnabled(), output->GetState() };
                                auto it = known_states_.find(output->GetToken());

                                bool changed = false;
                                if (it == known_states_.end())
                                {
                                        changed = true;
                                        known_states_[output->GetToken()] = current_state;
                                }
                                else if (it->second.enabled != current_state.enabled || it->second.state != current_state.state)
                                {
                                        changed = true;
                                        it->second = current_state;
                                }

                                if (!changed)
                                        continue;

                                NotificationMessage nm;
                                nm.topic = notifications_topic_;
                                nm.utc_time = utility::datetime::system_utc_datetime();
                                nm.property_operation = "Changed";
                                nm.source_item_descriptions.push_back({"RelayToken", output->GetToken()});
                                nm.data_name = "LogicalState";
                                nm.data_value = current_state.enabled && current_state.state ? "true" : "false";

                                event_signal_(nm);
                        }
                }

                MotionAlarmEventGenerator::MotionAlarmEventGenerator(const std::string& source_token,
                        int interval, const std::string& topic,
                        boost::asio::io_context& io_context, const ILogger& logger_)
                        : IEventGenerator(interval, topic, io_context, logger_),
                        source_token_(source_token)
		{
		}

		std::deque<NotificationMessage> MotionAlarmEventGenerator::GenerateSynchronizationEvent() const
		{
			TRACE_LOG(logger_);

                        NotificationMessage nm;
                        nm.topic = notifications_topic_;
                        nm.utc_time = utility::datetime::system_utc_datetime();
                        nm.property_operation = "Initialized";
                        nm.source_item_descriptions.push_back({"Source", source_token_});
                        nm.data_name = "State";

                        const auto state = GetState();
                        nm.data_value = state.effective_state ? "true" : "false";

                        return { nm };
                }

                void MotionAlarmEventGenerator::SetState(bool enabled, bool state, std::optional<std::chrono::seconds> delay)
                {
                        std::optional<NotificationMessage> notification;
                        {
                                std::lock_guard<std::mutex> lock(state_mutex_);
                                enabled_ = enabled;
                                state_ = state;

                                if (delay && delay->count() > 0 && state)
                                        expiration_ = std::chrono::steady_clock::now() + *delay;
                                else
                                        expiration_.reset();

                                notification = build_notification_locked();
                        }

                        if (notification)
                                event_signal_(*notification);
                }

                MotionAlarmEventGenerator::MotionState MotionAlarmEventGenerator::GetState() const
                {
                        std::lock_guard<std::mutex> lock(state_mutex_);

                        MotionState result{ enabled_, state_, enabled_ && state_, std::nullopt };

                        if (expiration_)
                        {
                                const auto now = std::chrono::steady_clock::now();
                                if (now < *expiration_)
                                {
                                        const auto remaining = std::chrono::duration_cast<std::chrono::seconds>(*expiration_ - now);
                                        result.remaining_delay_seconds = static_cast<int>(remaining.count());
                                }
                        }

                        return result;
                }

                void MotionAlarmEventGenerator::generate_event()
                {
                        TRACE_LOG(logger_);

                        std::optional<NotificationMessage> notification;
                        {
                                std::lock_guard<std::mutex> lock(state_mutex_);
                                notification = build_notification_locked();
                        }

                        if (notification)
                                event_signal_(*notification);
                }

                std::optional<NotificationMessage> MotionAlarmEventGenerator::build_notification_locked()
                {
                        const auto now = std::chrono::steady_clock::now();
                        if (expiration_ && now >= *expiration_)
                        {
                                expiration_.reset();
                                state_ = false;
                        }

                        const bool effective_state = enabled_ && state_;
                        if (effective_state == last_emitted_state_)
                                return std::nullopt;

                        last_emitted_state_ = effective_state;

                        NotificationMessage nm;
                        nm.topic = notifications_topic_;
                        nm.utc_time = utility::datetime::system_utc_datetime();
                        nm.property_operation = "Changed";
                        nm.source_item_descriptions.push_back({"Source", source_token_});
                        nm.data_name = "State";
                        nm.data_value = effective_state ? "true" : "false";

                        return nm;
                }

		CellMotionEventGenerator::CellMotionEventGenerator(const std::string& vsc_token, const std::string& vac_token,
			const std::string& rule,
			const std::string& din,
			int interval, const std::string& topic, boost::asio::io_context& io_context, const ILogger& logger_)
			: IEventGenerator(interval, topic, io_context, logger_)
			,video_source_configuration_token_(vsc_token)
			,video_analytics_configuration_token_(vac_token)
			,rule_(rule)
			,data_item_name_(din)
		{
		}

		std::deque<NotificationMessage> CellMotionEventGenerator::GenerateSynchronizationEvent() const
		{
			TRACE_LOG(logger_);

			NotificationMessage nm;
			nm.topic = notifications_topic_;
			nm.utc_time = utility::datetime::system_utc_datetime();
                        nm.property_operation = "Initialized";
                        nm.source_item_descriptions.push_back({"VideoSourceConfigurationToken", video_source_configuration_token_});
                        nm.source_item_descriptions.push_back({"VideoAnalyticsConfigurationToken", video_analytics_configuration_token_});
                        nm.source_item_descriptions.push_back({"Rule", rule_});
                        nm.data_name = data_item_name_;

                        const auto state = GetState();
                        nm.data_value = state.effective_state ? "true" : "false";

                        return { nm };
                }

                void CellMotionEventGenerator::SetState(bool enabled, bool state, std::optional<std::chrono::seconds> /*delay*/)
                {
                        std::optional<NotificationMessage> notification;
                        {
                                std::lock_guard<std::mutex> lock(state_mutex_);
                                enabled_ = enabled;
                                state_ = state;

                                notification = build_notification_locked();
                        }

                        if (notification)
                                event_signal_(*notification);
                }

                CellMotionEventGenerator::MotionState CellMotionEventGenerator::GetState() const
                {
                        std::lock_guard<std::mutex> lock(state_mutex_);

                        MotionState result{ enabled_, state_, enabled_ && state_, std::nullopt };

                        return result;
                }

                void CellMotionEventGenerator::generate_event()
                {
                        TRACE_LOG(logger_);

                        std::optional<NotificationMessage> notification;
                        {
                                std::lock_guard<std::mutex> lock(state_mutex_);
                                notification = build_notification_locked();
                        }

                        if (notification)
                                event_signal_(*notification);
                }

                std::optional<NotificationMessage> CellMotionEventGenerator::build_notification_locked()
                {
                        const bool effective_state = enabled_ && state_;
                        if (effective_state == last_emitted_state_)
                                return std::nullopt;

                        last_emitted_state_ = effective_state;

                        NotificationMessage nm;
                        nm.topic = notifications_topic_;
                        nm.utc_time = utility::datetime::system_utc_datetime();
                        nm.property_operation = "Changed";
                        nm.source_item_descriptions.push_back({"VideoSourceConfigurationToken", video_source_configuration_token_});
                        nm.source_item_descriptions.push_back({"VideoAnalyticsConfigurationToken", video_analytics_configuration_token_});
                        nm.source_item_descriptions.push_back({"Rule", rule_});
                        nm.data_name = data_item_name_;
                        nm.data_value = effective_state ? "true" : "false";

                        return nm;
                }

		AudioDetectectionEventGenerator::AudioDetectectionEventGenerator(const std::string& sct,
			const std::string& acf, const std::string& r, const std::string& din,
			int timeout, const std::string& topic, boost::asio::io_context& ioc, const ILogger& logger)
			: IEventGenerator(timeout, topic, ioc, logger)
			, source_configuration_token_(sct)
			, analytics_configuration_token_(acf)
			, rule_(r)
			, data_item_name_(din)
		{
		}

		std::deque<NotificationMessage> AudioDetectectionEventGenerator::GenerateSynchronizationEvent() const
		{
			TRACE_LOG(logger_);

			NotificationMessage nm;
			nm.topic = notifications_topic_;
			nm.utc_time = utility::datetime::system_utc_datetime();
			nm.property_operation = "Initialized";
			nm.source_item_descriptions.push_back({"AudioSourceConfigurationToken", source_configuration_token_});
			nm.source_item_descriptions.push_back({"AudioAnalyticsConfigurationToken", 
				analytics_configuration_token_});
			nm.source_item_descriptions.push_back({"Rule", rule_});
			nm.data_name = data_item_name_;
			nm.data_value = "false";

			return { nm };
		}

		void AudioDetectectionEventGenerator::generate_event()
		{
			TRACE_LOG(logger_);

			NotificationMessage nm;
			nm.topic = notifications_topic_;
			nm.utc_time = utility::datetime::system_utc_datetime();
			nm.property_operation = "Changed";
			nm.source_item_descriptions.push_back({"AudioSourceConfigurationToken",
				source_configuration_token_});
			nm.source_item_descriptions.push_back({"AudioAnalyticsConfigurationToken",
				analytics_configuration_token_});
			nm.source_item_descriptions.push_back({"Rule", rule_});
			nm.data_name = data_item_name_;
			// each time invert state
			nm.data_value = InvertState() ? "true" : "false";

			event_signal_(nm);
		}
}
}