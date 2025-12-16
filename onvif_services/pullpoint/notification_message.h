#pragma once

#include <string>
#include <vector>

namespace osrv
{
        namespace event
        {
                using StringPairsList_t = std::vector<std::pair<std::string, std::string>>;

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
        }
}
