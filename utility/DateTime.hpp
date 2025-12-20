#pragma once

#include <cstdlib>
#include <iomanip>
#include <sstream>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/property_tree/ptree.hpp>

namespace utility
{
        namespace datetime
        {
                inline boost::posix_time::ptime system_utc_now()
                {
                        namespace pt = boost::posix_time;

                        return pt::microsec_clock::universal_time();
                }

                inline boost::posix_time::ptime system_local_now()
                {
                        namespace pt = boost::posix_time;

                        return pt::microsec_clock::local_time();
                }

                inline std::string format_timezone_from_offset(const boost::posix_time::time_duration& timezone_offset)
                {
                        const auto total_seconds = timezone_offset.total_seconds();
                        const bool is_negative = total_seconds < 0;
                        const auto abs_seconds = std::abs(total_seconds);
                        const auto offset_hours = abs_seconds / 3600;
                        const auto offset_minutes = (abs_seconds % 3600) / 60;

                        std::ostringstream tz_stream;
                        tz_stream << (is_negative ? '-' : '+') << std::setw(2) << std::setfill('0') << offset_hours << ":"
                                  << std::setw(2) << std::setfill('0') << offset_minutes;

                        return tz_stream.str();
                }

                inline std::string posix_datetime_to_utc(boost::posix_time::ptime tm)
                {
                        std::stringstream ss;

                        namespace pt = boost::posix_time;

                        // date format example: 2020-10-27T11:20:42Z
                        // pt::time_facet* tf = new pt::time_facet("%Y-%m-%dT%H:M%:%S:%FZ");
                        pt::time_facet* tf = new pt::time_facet("%Y-%m-%dT%H:%M:%S.%fZ");
                        ss.imbue(std::locale(ss.getloc(), tf));

                        ss << tm;

                        return ss.str();
                }

                inline std::string posix_time_to_utc(boost::posix_time::ptime tm)
                {
                        std::stringstream ss;

                        namespace pt = boost::posix_time;

                        pt::time_facet* tf = new pt::time_facet("%H:%M:%S.%fZ");
                        ss.imbue(std::locale(ss.getloc(), tf));

                        ss << tm;

                        return ss.str();
                }

                inline std::string system_utc_datetime()
                {
                        return posix_datetime_to_utc(system_utc_now());
                }

                inline std::string system_utc_time()
                {
                        return posix_time_to_utc(system_utc_now());
                }

                inline boost::property_tree::ptree make_onvif_datetime_node(const boost::posix_time::ptime& time)
                {
                        namespace pt = boost::posix_time;

                        boost::property_tree::ptree datetime_node;
                        const auto date = time.date();
                        const auto tod = time.time_of_day();

                        datetime_node.put("tt:Time.tt:Hour", tod.hours());
                        datetime_node.put("tt:Time.tt:Minute", tod.minutes());
                        datetime_node.put("tt:Time.tt:Second", tod.seconds());

                        datetime_node.put("tt:Date.tt:Year", static_cast<int>(date.year()));
                        datetime_node.put("tt:Date.tt:Month", static_cast<int>(date.month()));
                        datetime_node.put("tt:Date.tt:Day", static_cast<int>(date.day()));

                        return datetime_node;
                }

                inline std::string current_timezone_tz_string()
                {
                        const auto timezone_offset = system_local_now() - system_utc_now();

                        return format_timezone_from_offset(timezone_offset);
                }
        }
}
