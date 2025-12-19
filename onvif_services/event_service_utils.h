#pragma once

#include <boost/property_tree/ptree.hpp>

#include <string>

namespace osrv
{
namespace event
{
struct MessageLimitValidationResult
{
        bool valid = true;
        int message_limit = 0;
        std::string reason;
};

MessageLimitValidationResult validate_message_limit(const boost::property_tree::ptree& request_tree,
        int default_message_limit, int max_message_limit);
}
} // namespace osrv

