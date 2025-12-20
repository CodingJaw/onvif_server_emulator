#pragma once

#include "DateTime.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <mutex>
#include <regex>
#include <string>
#include <sys/time.h>

namespace osrv
{
class SystemClock
{
public:
        struct State
        {
                std::string datetime_type{"NTP"};
                bool daylight_savings{false};
                std::string timezone{utility::datetime::current_timezone_tz_string()};
                boost::posix_time::ptime utc_time{utility::datetime::system_utc_now()};
                boost::posix_time::ptime local_time{utility::datetime::system_local_now()};
                bool ntp_enabled{true};
        };

        SystemClock() = default;

        void Initialize(const State& initial_state)
        {
                std::lock_guard<std::mutex> lk(mutex_);
                if (initialized_)
                        return;

                state_ = normalize_state(initial_state);
                initialized_ = true;
        }

        void Update(const State& new_state)
        {
                std::lock_guard<std::mutex> lk(mutex_);

                state_ = normalize_state(merge_state(new_state));
                apply_host_state(state_);
        }

        State Snapshot() const
        {
                std::lock_guard<std::mutex> lk(mutex_);
                return state_;
        }

private:
        static boost::posix_time::time_duration parse_timezone_offset(const std::string& tz)
        {
                static const std::regex tz_regex(R"(([+-])(\d{2}):(\d{2}))");
                std::smatch matches;
                if (!std::regex_match(tz, matches, tz_regex))
                {
                        return boost::posix_time::time_duration(0, 0, 0);
                }

                const auto hours = std::stoi(matches[2]);
                const auto minutes = std::stoi(matches[3]);
                const auto duration = boost::posix_time::hours(hours) + boost::posix_time::minutes(minutes);
                return matches[1] == "-" ? -duration : duration;
        }

        static boost::posix_time::ptime utc_from_local(const boost::posix_time::ptime& local,
                                                                                              const boost::posix_time::time_duration& offset)
        {
                return local - offset;
        }

        static timeval to_timeval(const boost::posix_time::ptime& utc_time)
        {
                static const auto epoch = boost::posix_time::ptime(boost::gregorian::date(1970, 1, 1));
                const auto diff = utc_time - epoch;

                timeval tv{};
                tv.tv_sec = static_cast<time_t>(diff.total_seconds());
                tv.tv_usec = static_cast<suseconds_t>(diff.fractional_seconds());
                return tv;
        }

        State normalize_state(State state) const
        {
                const auto tz = state.timezone.empty() ? utility::datetime::current_timezone_tz_string() : state.timezone;
                state.timezone = tz;
                const auto offset = parse_timezone_offset(tz);

                state.ntp_enabled = boost::iequals(state.datetime_type, "NTP");

                const bool utc_valid = !state.utc_time.is_not_a_date_time();
                const bool local_valid = !state.local_time.is_not_a_date_time();

                if (!utc_valid && local_valid)
                {
                        state.utc_time = utc_from_local(state.local_time, offset);
                }
                else if (utc_valid && !local_valid)
                {
                        state.local_time = state.utc_time + offset;
                }

                return state;
        }

        State merge_state(State update) const
        {
                State merged = state_;

                merged.datetime_type = update.datetime_type.empty() ? merged.datetime_type : update.datetime_type;
                merged.daylight_savings = update.daylight_savings;
                merged.timezone = update.timezone.empty() ? merged.timezone : update.timezone;
                merged.utc_time = !update.utc_time.is_not_a_date_time() ? update.utc_time : merged.utc_time;
                merged.local_time = !update.local_time.is_not_a_date_time() ? update.local_time : merged.local_time;

                return merged;
        }

        void apply_host_state(const State& state)
        {
#if defined(_WIN32)
                (void)state;
                return;
#else
                if (state.ntp_enabled)
                {
                        return;
                }

                if (state.utc_time.is_not_a_date_time())
                {
                        return;
                }

                const auto tv = to_timeval(state.utc_time);
                settimeofday(&tv, nullptr);
#endif
        }

        mutable std::mutex mutex_{};
        State state_{};
        bool initialized_ = false;
};
} // namespace osrv

