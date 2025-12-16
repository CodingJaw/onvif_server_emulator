#pragma once

#include "../Logger.h"
#include "../onvif_services/physical_components/IDigitalInput.h"
#include "../onvif_services/physical_components/IDigitalOutput.h"
#include "notification_message.h"

#include <functional>
#include <deque>
#include <unordered_map>
#include <chrono>
#include <mutex>
#include <optional>

#include <boost/signals2.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>

namespace osrv
{
	namespace event
	{
                class IEventGenerator
                {
		public:
			IEventGenerator(int interval, const std::string& topic, boost::asio::io_context& io_context, const ILogger& logger)
				:
				event_interval_(interval)
				,notifications_topic_(topic)
				,io_context_(io_context)
				,alarm_timer_(io_context_)
				,logger_(logger)
			{
			}

			virtual ~IEventGenerator() {}

			//Before calling this method, no any events should be generated
			void Run()
			{
				schedule_next_alarm();
			}

			void Stop()
			{
				alarm_timer_.cancel();
			}

			//This will be connected
			boost::signals2::connection Connect(std::function<void(NotificationMessage)> f)
			{
				return event_signal_.connect(f);
			}

			// returns a NotificationMessage with 'PropertyOperation' equals "Initialized"
			virtual std::deque<NotificationMessage> GenerateSynchronizationEvent() const = 0;

		protected:
			// This method is should be overrided by implementors.
			// Implementors should fill a NotificationMessage and emit the signal with it
			virtual void generate_event()
			{
				//	NotificationMessage event_description;
				// /*do somehow filling event_description...*/
				// event_signal_(NotificationMessage);
			}

			void schedule_next_alarm()
			{
				alarm_timer_.expires_after(std::chrono::seconds(event_interval_));
				alarm_timer_.async_wait([this](const boost::system::error_code& error) {
							if(error == boost::asio::error::operation_aborted)
								return;

							generate_event();

							schedule_next_alarm();
						}
					);
			}

		protected:
			boost::signals2::signal<void(NotificationMessage)> event_signal_;

		protected:
			const int event_interval_;
			const std::string notifications_topic_;

			boost::asio::io_context& io_context_;
			boost::asio::steady_timer alarm_timer_;

			const ILogger& logger_;
		};

                class DInputEventGenerator : public IEventGenerator
                {
                public:
                        DInputEventGenerator(int /*interval*/, const std::string& /*topic*/,
                                boost::asio::io_context& /*io_context*/, const ILogger& /*logger_*/);

			// If the member DigitalInputsList is not initialized, events will not be generated
			void SetDigitalInputsList(const DigitalInputsList& /*di_list*/);

                        // Inherited via IEventGenerator
                        std::deque<NotificationMessage> GenerateSynchronizationEvent() const override;

                protected:
                        void generate_event() override;

                private:
                        struct InputState
                        {
                                bool enabled;
                                bool state;
                        };

                        mutable std::unordered_map<std::string, InputState> known_states_;
                        const DigitalInputsList* di_list_ = nullptr;
                };

                class DOutputEventGenerator : public IEventGenerator
                {
                public:
                        DOutputEventGenerator(int /*interval*/, const std::string& /*topic*/,
                                boost::asio::io_context& /*io_context*/, const ILogger& /*logger_*/);

                        void SetDigitalOutputsList(const DigitalOutputsList& /*do_list*/);

                        // Inherited via IEventGenerator
                        std::deque<NotificationMessage> GenerateSynchronizationEvent() const override;

                protected:
                        void generate_event() override;

                private:
                        struct OutputState
                        {
                                bool enabled;
                                bool state;
                        };

                        mutable std::unordered_map<std::string, OutputState> known_states_;
                        const DigitalOutputsList* do_list_ = nullptr;
                };
		
                class MotionAlarmEventGenerator : public IEventGenerator
                {
                public:
                        MotionAlarmEventGenerator(const std::string& /*source_token*/, int /*interval*/, const std::string& /*topic*/,
                                boost::asio::io_context& /*io_context*/, const ILogger& /*logger_*/);

                        struct MotionState
                        {
                                bool enabled;
                                bool state;
                                bool effective_state;
                                std::optional<int> remaining_delay_seconds;
                        };

                        void SetState(bool enabled, bool state, std::optional<std::chrono::seconds> delay);

                        MotionState GetState() const;

                        // Inherited via IEventGenerator
                        std::deque<NotificationMessage> GenerateSynchronizationEvent() const override;

                protected:
			void generate_event() override;

                private:
                        std::optional<NotificationMessage> build_notification_locked();

                private:
                        bool enabled_ = true;
                        bool state_ = false;
                        bool last_emitted_state_ = false;
                        std::optional<std::chrono::steady_clock::time_point> expiration_;
                        mutable std::mutex state_mutex_;
                        std::string source_token_;
                };

                class CellMotionEventGenerator : public IEventGenerator
                {
                public:
			CellMotionEventGenerator(const std::string& /*vsc_token*/, const std::string& /*vac_token*/,
				const std::string& /*rule*/,
                                const std::string& /*data_item_name*/,
                                int /*interval*/, const std::string& /*topic*/,
                                boost::asio::io_context& /*io_context*/, const ILogger& /*logger_*/);

                        struct MotionState
                        {
                                bool enabled;
                                bool state;
                                bool effective_state;
                                std::optional<int> remaining_delay_seconds;
                        };

                        void SetState(bool enabled, bool state, std::optional<std::chrono::seconds> delay);

                        MotionState GetState() const;

                        // Inherited via IEventGenerator
                        std::deque<NotificationMessage> GenerateSynchronizationEvent() const override;

                protected:
			void generate_event() override;

                private:
                        std::optional<NotificationMessage> build_notification_locked();

                private:
                        bool enabled_ = true;
                        bool state_ = false;
                        bool last_emitted_state_ = false;
                        std::optional<std::chrono::steady_clock::time_point> expiration_;
                        mutable std::mutex state_mutex_;
                        std::string video_source_configuration_token_;
                        std::string video_analytics_configuration_token_;
                        std::string rule_;
			std::string data_item_name_;
		};

		class AudioDetectectionEventGenerator : public IEventGenerator
		{
		public:
			AudioDetectectionEventGenerator(const std::string& /*asc_token*/, const std::string& /*ac_token*/,
				const std::string& /*rule*/,
				const std::string& /*data_item_name*/,
				int /*interval*/, const std::string& /*topic*/,
				boost::asio::io_context& /*io_context*/, const ILogger& /*logger_*/);

			// Inherited via IEventGenerator
			std::deque<NotificationMessage> GenerateSynchronizationEvent() const override;

		protected:
			void generate_event() override;

		private:
			bool InvertState()
			{
				return state_ = !state_;
			}

		private:
			bool state_ = false;
			std::string source_configuration_token_;
			std::string analytics_configuration_token_;
			std::string rule_;
			std::string data_item_name_;
		};
	}
}