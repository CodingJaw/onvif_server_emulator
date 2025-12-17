#include "event_generators.h"
#include "pull_point.h"

#include "../utility/DateTime.hpp"


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

                        bool resolved_state = false;
                        {
                                std::lock_guard lk(state_mutex_);
                                resolved_state = enabled_ && state_;
                        }

                        NotificationMessage nm;
                        nm.topic = notifications_topic_;
                        nm.utc_time = utility::datetime::system_utc_datetime();
                        nm.property_operation = "Initialized";
                        nm.source_item_descriptions.push_back({"Source", source_token_});
                        nm.data_name = "State";
                        nm.data_value = resolved_state ? "true" : "false";

                        return { nm };
                }

                MotionAlarmEventGenerator::MotionState MotionAlarmEventGenerator::GetState() const
                {
                        std::lock_guard lk(state_mutex_);
                        return MotionState{ enabled_, state_ };
                }

                void MotionAlarmEventGenerator::UpdateState(bool enabled, bool state)
                {
                        bool should_emit = false;

                        {
                                std::lock_guard lk(state_mutex_);
                                if (enabled_ != enabled || state_ != state)
                                {
                                        enabled_ = enabled;
                                        state_ = state;
                                        state_dirty_ = true;
                                        should_emit = true;
                                }
                        }

                        if (should_emit)
                        {
                                boost::asio::post(io_context_, [this]() { generate_event(); });
                        }
                }

                void MotionAlarmEventGenerator::generate_event()
                {
                        TRACE_LOG(logger_);

                        bool resolved_state = false;

                        {
                                std::lock_guard lk(state_mutex_);

                                if (!state_dirty_)
                                        return;

                                resolved_state = enabled_ && state_;

                                if (last_reported_state_ && *last_reported_state_ == resolved_state)
                                {
                                        state_dirty_ = false;
                                        return;
                                }

                                state_dirty_ = false;
                                last_reported_state_ = resolved_state;
                        }

                        NotificationMessage nm;
                        nm.topic = notifications_topic_;
                        nm.utc_time = utility::datetime::system_utc_datetime();
                        nm.property_operation = "Changed";
                        nm.source_item_descriptions.push_back({"Source", source_token_});
                        nm.data_name = "State";
                        nm.data_value = resolved_state ? "true" : "false";

                        event_signal_(nm);
                }

                CellMotionEventGenerator::CellMotionEventGenerator(const std::string& vsc_token, const std::string& vac_token,
                        const std::string& rule,
                        const std::string& din,
                        const std::string& token,
                        int interval, const std::string& topic, boost::asio::io_context& io_context, const ILogger& logger_)
                        : IEventGenerator(interval, topic, io_context, logger_)
                        ,video_source_configuration_token_(vsc_token)
                        ,video_analytics_configuration_token_(vac_token)
                        ,rule_(rule)
                        ,data_item_name_(din)
                        ,token_(token)
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

                        bool state = false;
                        {
                                std::lock_guard lk(state_mutex_);
                                state = enabled_ && state_;
                        }

                        nm.data_value = state ? "true" : "false";

                        return { nm };
                }

                CellMotionEventGenerator::MotionState CellMotionEventGenerator::GetState() const
                {
                        std::lock_guard lk(state_mutex_);
                        return MotionState{ token_, enabled_, state_ };
                }

                void CellMotionEventGenerator::UpdateState(bool enabled, bool state)
                {
                        bool should_emit = false;

                        {
                                std::lock_guard lk(state_mutex_);
                                if (enabled_ != enabled || state_ != state)
                                {
                                        enabled_ = enabled;
                                        state_ = state;
                                        state_dirty_ = true;
                                        should_emit = true;
                                }
                        }

                        if (should_emit)
                        {
                                boost::asio::post(io_context_, [this]() { generate_event(); });
                        }
                }

                void CellMotionEventGenerator::generate_event()
                {
                        TRACE_LOG(logger_);

                        bool resolved_state = false;

                        {
                                std::lock_guard lk(state_mutex_);

                                if (!state_dirty_)
                                        return;

                                resolved_state = enabled_ && state_;

                                if (last_reported_state_ && *last_reported_state_ == resolved_state)
                                {
                                        state_dirty_ = false;
                                        return;
                                }

                                state_dirty_ = false;
                                last_reported_state_ = resolved_state;
                        }

                        NotificationMessage nm;
                        nm.topic = notifications_topic_;
                        nm.utc_time = utility::datetime::system_utc_datetime();
                        nm.property_operation = "Changed";
                        nm.source_item_descriptions.push_back({"VideoSourceConfigurationToken", video_source_configuration_token_});
                        nm.source_item_descriptions.push_back({"VideoAnalyticsConfigurationToken", video_analytics_configuration_token_});
                        nm.source_item_descriptions.push_back({"Rule", rule_});
                        nm.data_name = data_item_name_;
                        nm.data_value = resolved_state ? "true" : "false";

                        NotificationMessage digital_input_nm;
                        digital_input_nm.topic = "tns1:Device/Trigger/DigitalInput";
                        digital_input_nm.utc_time = nm.utc_time;
                        digital_input_nm.property_operation = "Changed";
                        digital_input_nm.source_item_descriptions.push_back({"InputToken", token_});
                        digital_input_nm.data_name = "LogicalState";
                        digital_input_nm.data_value = nm.data_value;

                        event_signal_(nm);
                        event_signal_(digital_input_nm);
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