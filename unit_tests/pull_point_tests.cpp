#include <boost/test/unit_test.hpp>

#include "../onvif_services/pullpoint/pull_point.h"
#include "../onvif_services/event_service_utils.h"
#include "../utility/DateTime.hpp"
#include "../utility/XmlParser.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <chrono>
#include <map>
#include <memory>
#include <stdexcept>
#include <vector>

namespace
{
namespace pt = boost::property_tree;

struct DummyLogger : ILogger
{
        DummyLogger()
                        : ILogger(ILogger::LVL_DEBUG)
        {
        }

        void Error(const std::string&) const override {}
        void Warn(const std::string&) const override {}
        void Info(const std::string&) const override {}
        void Debug(const std::string&) const override {}
        void Trace(const std::string&) const override {}
};

class TestEventGenerator : public osrv::event::IEventGenerator
{
public:
        TestEventGenerator(const std::string& topic, boost::asio::io_context& io, const ILogger& logger)
                        : osrv::event::IEventGenerator(1, topic, io, logger)
        {
        }

        std::deque<osrv::event::NotificationMessage> GenerateSynchronizationEvent() const override
        {
                return {};
        }

        void Emit(osrv::event::NotificationMessage msg)
        {
                event_signal_(std::move(msg));
        }

        size_t ConnectionCount() const
        {
                return event_signal_.num_slots();
        }

protected:
        void generate_event() override {}
};
}

/* FIX: the tested function @parse_pullmessages  was deleted, add restore these tests and generalize function
BOOST_AUTO_TEST_CASE(parse_pullmessages_func_0)
{
	const std::string request =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
			"<SOAP-ENV:Envelope"
				" xmlns:SOAP-ENV=\"http://www.w3.org/2003/05/soap-envelope\""
				" xmlns:wsa=\"http://www.w3.org/2005/08/addressing\""
				" xmlns:tet=\"http://www.onvif.org/ver10/events/wsdl\">"
				"<SOAP-ENV:Header>"
					"<wsa:Action>"
						"http://www.onvif.org/ver10/events/wsdl/PullPointSubscription/PullMessagesRequest"
					"</wsa:Action>"
					"<wsa:To>http://160.10.64.10/Subscription?Idx=0</wsa:To>"
				"</SOAP-ENV:Header>"
			"<SOAP-ENV:Body>"
				"<tet:PullMessages>"
					"<tet:Timeout>"
						"PT5S"
					"</tet:Timeout>"
					"<tet:MessageLimit>"
						"2"
					"</tet:MessageLimit>"
				"</tet:PullMessages>"
			"</SOAP-ENV:Body>"
		"</SOAP-ENV:Envelope>";

	osrv::event::PullMessagesRequest expected;
	expected.header_action = "http://www.onvif.org/ver10/events/wsdl/PullPointSubscription/PullMessagesRequest";
	expected.header_to = "http://160.10.64.10/Subscription?Idx=0";
	expected.timeout = "PT5S";
	expected.messages_limit = 2;

	auto actual = osrv::event::parse_pullmessages(request);

	BOOST_TEST(actual.header_action == expected.header_action);
	BOOST_TEST(actual.header_to == expected.header_to);
	BOOST_TEST(actual.timeout == expected.timeout);
	BOOST_TEST(actual.messages_limit == expected.messages_limit);
}

BOOST_AUTO_TEST_CASE(parse_pullmessages_func_1)
{
	const std::string request =
	"<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\""
		"xmlns:a=\"http://www.w3.org/2005/08/addressing\">"
		"<s:Header>"
				"<a:Action
s:mustUnderstand=\"1\">http://www.onvif.org/ver10/events/wsdl/PullPointSubscription/PullMessagesRequest</a:Action>"
				"<a:MessageID>urn:uuid:30cf5aa8-d867-419f-962b-b789f8d7e37e</a:MessageID>"
				"<a:ReplyTo>"
						"<a:Address>http://www.w3.org/2005/08/addressing/anonymous</a:Address>"
				"</a:ReplyTo>"
		"<Security s:mustUnderstand=\"1\""
						"xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\">"
						"<UsernameToken>"
								"<Username>admin</Username>"
								"<Password
Type=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">24UZInK62uTSnctdnp6ErMpt8LI=</Password>"
								"<Nonce
EncodingType=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary\">LYz8rNY2tUyYOTN00Hoo5r0GAAAAAA==</Nonce>"
								"<Created
xmlns=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">2020-10-27T11:10:42.103Z</Created>"
						"</UsernameToken>"
				"</Security>"
				"<a:To s:mustUnderstand=\"1\">http://192.168.43.120:8000/event_service/0</a:To>"
		"</s:Header>"
		"<s:Body xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\""
			"xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">"
			"<PullMessages xmlns=\"http://www.onvif.org/ver10/events/wsdl\">"
				"<Timeout>PT1M</Timeout>"
				"<MessageLimit>1024</MessageLimit>"
			"</PullMessages>"
		"</s:Body>"
	"</s:Envelope>";

	osrv::event::PullMessagesRequest expected;
	expected.header_action = "http://www.onvif.org/ver10/events/wsdl/PullPointSubscription/PullMessagesRequest";
	expected.header_to = "http://192.168.43.120:8000/event_service/0";
	expected.timeout = "PT1M";
	expected.messages_limit = 1024;

	auto actual = osrv::event::parse_pullmessages(request);

	BOOST_TEST(actual.header_action == expected.header_action);
	BOOST_TEST(actual.header_to == expected.header_to);
	BOOST_TEST(actual.timeout == expected.timeout);
	BOOST_TEST(actual.messages_limit == expected.messages_limit);
}
*/

BOOST_AUTO_TEST_CASE(compare_subscription_references_func)
{
        using namespace osrv::event;

	const std::string full_ref = "http://127.0.0.1:8080/onvif/event_service/s0";
	const std::string test_subscription_ref = "onvif/event_service/s0";
	const std::string test_subscription_ref2 = "onvif/event_service/s1";

        BOOST_TEST(true == compare_subscription_references(full_ref, test_subscription_ref));
        BOOST_TEST(false == compare_subscription_references(full_ref, test_subscription_ref2));
}

BOOST_AUTO_TEST_CASE(find_pullpoint_requires_delimited_suffix_match)
{
        using namespace osrv::event;

        boost::asio::io_context io;
        DummyLogger logger;
        PullPoints_t pullpoints;

        auto subscription_one = std::make_shared<PullPoint>("onvif/event_service/s1", io, logger);
        auto subscription_two = std::make_shared<PullPoint>("onvif/event_service/s10", io, logger);

        pullpoints.push_back(subscription_one);
        pullpoints.push_back(subscription_two);

        auto found_one = find_pullpoint(pullpoints, "http://127.0.0.1:8080/onvif/event_service/s1");
        BOOST_TEST(found_one != pullpoints.end());
        BOOST_TEST(*found_one == subscription_one);

        auto found_two = find_pullpoint(pullpoints, "http://127.0.0.1:8080/onvif/event_service/s10");
        BOOST_TEST(found_two != pullpoints.end());
        BOOST_TEST(*found_two == subscription_two);

        auto missing = find_pullpoint(pullpoints, "http://127.0.0.1:8080/onvif/event_service/s1_extra");
        BOOST_TEST(missing == pullpoints.end());
}

BOOST_AUTO_TEST_CASE(validates_message_limit_with_defaults)
{
        using namespace osrv::event;

        const std::string request =
                        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">"
                        "<s:Body>"
                        "<PullMessages xmlns=\"http://www.onvif.org/ver10/events/wsdl\">"
                        "<Timeout>PT1S</Timeout>"
                        "</PullMessages>"
                        "</s:Body>"
                        "</s:Envelope>";

        const auto ptree_request = exns::to_ptree(request);
        const auto result = validate_message_limit(ptree_request, /*default_message_limit*/ 7, /*max_message_limit*/ 10);

        BOOST_TEST(result.valid);
        BOOST_TEST(result.message_limit == 7);
}

BOOST_AUTO_TEST_CASE(rejects_non_positive_message_limits)
{
        using namespace osrv::event;

        const std::string request =
                        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">"
                        "<s:Body>"
                        "<PullMessages xmlns=\"http://www.onvif.org/ver10/events/wsdl\">"
                        "<Timeout>PT1S</Timeout>"
                        "<MessageLimit>-5</MessageLimit>"
                        "</PullMessages>"
                        "</s:Body>"
                        "</s:Envelope>";

        const auto ptree_request = exns::to_ptree(request);
        const auto result = validate_message_limit(ptree_request, /*default_message_limit*/ 5, /*max_message_limit*/ 10);

        BOOST_TEST(!result.valid);
        BOOST_TEST(result.reason == "MessageLimit must be between 1 and 10");
}

BOOST_AUTO_TEST_CASE(rejects_oversized_message_limits)
{
        using namespace osrv::event;

        const std::string request =
                        "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\">"
                        "<s:Body>"
                        "<PullMessages xmlns=\"http://www.onvif.org/ver10/events/wsdl\">"
                        "<Timeout>PT1S</Timeout>"
                        "<MessageLimit>25</MessageLimit>"
                        "</PullMessages>"
                        "</s:Body>"
                        "</s:Envelope>";

        const auto ptree_request = exns::to_ptree(request);
        const auto result = validate_message_limit(ptree_request, /*default_message_limit*/ 5, /*max_message_limit*/ 10);

        BOOST_TEST(!result.valid);
        BOOST_TEST(result.reason == "MessageLimit must be between 1 and 10");
}

BOOST_AUTO_TEST_CASE(serialize_notification_messages_func0)
{
        // empty queue
        using namespace osrv::event;

        std::deque<NotificationMessage> msgs;
        boost::asio::io_context io;
        DummyLogger logger;
        auto pullpoint = std::make_shared<PullPoint>("onvif/event_service/s0", io, logger);
        auto now = boost::posix_time::microsec_clock::universal_time();
        pullpoint->SetSubscriptionTimes(now, now, now + boost::posix_time::seconds(60));
        boost::property_tree::ptree res = serialize_notification_messages(msgs, *pullpoint);

	auto ctime = exns::find_hierarchy("CurrentTime", res);
	auto ttime = exns::find_hierarchy("TerminationTime", res);

	BOOST_TEST(!ctime.empty());
	BOOST_TEST(!ttime.empty());
}

BOOST_AUTO_TEST_CASE(serialize_notification_messages_func1)
{
        using namespace osrv::event;

        std::deque<NotificationMessage> msgs;

        NotificationMessage test_msg;
        test_msg.source_item_descriptions.push_back({"ItemName", "ItemValue"});

        msgs.push_back(test_msg);

        boost::asio::io_context io;
        DummyLogger logger;
        auto pullpoint = std::make_shared<PullPoint>("onvif/event_service/s0", io, logger);
        const std::string subscription_address = "http://127.0.0.1:8080/onvif/event_service/s0";
        const std::string producer_endpoint = "http://127.0.0.1:8080/onvif/event_service";
        pullpoint->SetSubscriptionAddress(subscription_address);
        pullpoint->SetServiceEndpoint(producer_endpoint);
        auto now = boost::posix_time::microsec_clock::universal_time();
        pullpoint->SetSubscriptionTimes(now, now, now + boost::posix_time::seconds(60));
        boost::property_tree::ptree res = serialize_notification_messages(msgs, *pullpoint);

        auto sub_ref = res.get<std::string>("wsnt:NotificationMessage.wsnt:SubscriptionReference.wsa:Address");
        BOOST_TEST(sub_ref == subscription_address);

        auto producer_ref = res.get<std::string>("wsnt:NotificationMessage.wsnt:ProducerReference.wsa:Address");
        BOOST_TEST(producer_ref == producer_endpoint);

        auto name =
                        res.get<std::string>("wsnt:NotificationMessage.wsnt:Message.tt:Message.tt:Source.tt:SimpleItem.<xmlattr>.Name");
        BOOST_TEST(name == "ItemName");

        auto value =
                        res.get<std::string>("wsnt:NotificationMessage.wsnt:Message.tt:Message.tt:Source.tt:SimpleItem.<xmlattr>.Value");
        BOOST_TEST(value == "ItemValue");
}

BOOST_AUTO_TEST_CASE(rejects_rapid_renewals)
{
        using namespace osrv::event;
        boost::asio::io_context io;
        DummyLogger logger;
        auto pullpoint = std::make_shared<PullPoint>("onvif/event_service/s0", io, logger);

        auto created = boost::posix_time::second_clock::universal_time();
        auto lease = boost::posix_time::seconds(300);
        auto min_interval = boost::posix_time::seconds(2);
        pullpoint->SetSubscriptionTimes(created, created, created + lease);

        auto first_attempt = created + boost::posix_time::seconds(1);
        BOOST_TEST(false == pullpoint->TryRenew(first_attempt, lease, min_interval));
        BOOST_TEST(pullpoint->GetTerminationTimePoint() == created + lease);

        auto allowed_attempt = created + boost::posix_time::seconds(3);
        BOOST_TEST(true == pullpoint->TryRenew(allowed_attempt, lease, min_interval));
        BOOST_TEST(pullpoint->GetTerminationTimePoint() == allowed_attempt + lease);
}

BOOST_AUTO_TEST_CASE(expiration_detection)
{
        using namespace osrv::event;
        boost::asio::io_context io;
        DummyLogger logger;
        auto pullpoint = std::make_shared<PullPoint>("onvif/event_service/s0", io, logger);

        auto created = boost::posix_time::second_clock::universal_time();
        auto lease = boost::posix_time::seconds(10);
        pullpoint->SetSubscriptionTimes(created, created, created + lease);

        BOOST_TEST(false == pullpoint->IsExpired(created + boost::posix_time::seconds(5)));
        BOOST_TEST(true == pullpoint->IsExpired(created + boost::posix_time::seconds(15)));
}

BOOST_AUTO_TEST_CASE(filters_match_requested_topics)
{
        using namespace osrv::event;
        DummyLogger logger;
        std::map<std::string, std::string> namespaces;
        NotificationsManager manager(logger, namespaces, 300, 1, 60);

        auto& io = manager.GetIoContext();
        auto matching_gen = std::make_shared<TestEventGenerator>("tns1:RuleEngine/CellMotionDetector/Motion", io, logger);
        auto other_gen = std::make_shared<TestEventGenerator>("tns1:Device/Trigger/DigitalInput", io, logger);

        manager.AddGenerator(matching_gen);
        manager.AddGenerator(other_gen);

        std::vector<TopicExpression> topic_filters{{"http://www.onvif.org/ver10/tev/topicExpression/ConcreteSet",
                "tns1:RuleEngine/CellMotionDetector"}};

        auto pullpoint = manager.CreatePullPoint(topic_filters);
        (void)pullpoint;

        BOOST_TEST(matching_gen->ConnectionCount() == 1u);
        BOOST_TEST(other_gen->ConnectionCount() == 0u);
}

BOOST_AUTO_TEST_CASE(rejects_unknown_topic_expression_dialect)
{
        using namespace osrv::event;
        DummyLogger logger;
        std::map<std::string, std::string> namespaces;
        NotificationsManager manager(logger, namespaces, 300, 1, 60);

        std::vector<TopicExpression> topic_filters{{"http://example.com/unsupported", "tns1:RuleEngine/CellMotionDetector"}};

        BOOST_CHECK_THROW(manager.CreatePullPoint(topic_filters), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(allows_concurrent_subscriptions_to_receive_events)
{
        using namespace osrv::event;
        DummyLogger logger;
        std::map<std::string, std::string> namespaces;
        NotificationsManager manager(logger, namespaces, 300, 1, 60);

        auto& io = manager.GetIoContext();
        auto generator = std::make_shared<TestEventGenerator>("tns1:RuleEngine/CellMotionDetector/Motion", io, logger);
        manager.AddGenerator(generator);

        auto subscription_one = manager.CreatePullPoint({});
        auto subscription_two = manager.CreatePullPoint({});

        std::deque<NotificationMessage> received_one;
        std::deque<NotificationMessage> received_two;

        subscription_one->PullMessages([&received_one](std::shared_ptr<PullPoint>, std::deque<NotificationMessage>&& events,
                        std::shared_ptr<HttpServer::Response>) {
                received_one = std::move(events);
        }, nullptr, 1, 10);

        subscription_two->PullMessages([&received_two](std::shared_ptr<PullPoint>, std::deque<NotificationMessage>&& events,
                        std::shared_ptr<HttpServer::Response>) {
                received_two = std::move(events);
        }, nullptr, 1, 10);

        NotificationMessage message;
        message.topic = generator->Topic();
        message.utc_time = utility::datetime::system_utc_datetime();
        message.property_operation = "Changed";
        message.data_name = "State";
        message.data_value = "true";

        generator->Emit(message);

        BOOST_TEST(received_one.size() == 1u);
        BOOST_TEST(received_two.size() == 1u);
        BOOST_TEST(received_one.front().topic == generator->Topic());
        BOOST_TEST(received_two.front().topic == generator->Topic());
}

BOOST_AUTO_TEST_CASE(caps_backlog_for_idle_subscriptions)
{
        using namespace osrv::event;
        boost::asio::io_context io;
        DummyLogger logger;
        auto pullpoint = std::make_shared<PullPoint>("onvif/event_service/s0", io, logger);
        pullpoint->SetMaxMessages(3);

        for (int i = 0; i < 5; ++i)
        {
                NotificationMessage message;
                message.topic = "tns1:RuleEngine/CellMotionDetector";
                message.utc_time = utility::datetime::system_utc_datetime();
                message.property_operation = "Changed";
                message.data_name = "State";
                message.data_value = std::to_string(i);

                pullpoint->Notify(std::move(message));
        }

        std::deque<NotificationMessage> received;
        pullpoint->PullMessages([&received](std::shared_ptr<PullPoint>, std::deque<NotificationMessage>&& events,
                        std::shared_ptr<HttpServer::Response>) {
                received = std::move(events);
        }, nullptr, 1, 10);

        BOOST_TEST(received.size() == 3u);
        BOOST_TEST(received.front().data_value == "2");
        BOOST_TEST(received.back().data_value == "4");
}

BOOST_AUTO_TEST_CASE(responds_immediately_when_timeout_zero_and_no_events)
{
        using namespace osrv::event;
        boost::asio::io_context io;
        DummyLogger logger;
        auto pullpoint = std::make_shared<PullPoint>("onvif/event_service/s0", io, logger);

        bool handler_called = false;
        std::deque<NotificationMessage> received;
        pullpoint->PullMessages([
                        &handler_called, &received](std::shared_ptr<PullPoint>, std::deque<NotificationMessage>&& events,
                        std::shared_ptr<HttpServer::Response>) {
                handler_called = true;
                received = std::move(events);
        }, nullptr, 0, 10);

        BOOST_TEST(handler_called);
        BOOST_TEST(received.empty());
}

BOOST_AUTO_TEST_CASE(expired_subscriptions_do_not_affect_active_ones)
{
        using namespace osrv::event;
        DummyLogger logger;
        std::map<std::string, std::string> namespaces;
        NotificationsManager manager(logger, namespaces, 1, 0, 60);

        auto& io = manager.GetIoContext();
        auto generator = std::make_shared<TestEventGenerator>("tns1:RuleEngine/CellMotionDetector/Motion", io, logger);
        manager.AddGenerator(generator);

        auto expiring = manager.CreatePullPoint({});
        (void)expiring;
        auto extended = manager.CreatePullPoint({});

        const auto now = boost::posix_time::microsec_clock::universal_time();
        extended->SetSubscriptionTimes(now, now, now + boost::posix_time::seconds(5));
        extended->RefreshTerminationTimer(std::chrono::seconds(5), [&manager](const std::string& ref) {
                manager.Unsubscribe(ref);
        });

        io.run_for(std::chrono::seconds(2));

        BOOST_TEST(generator->ConnectionCount() == 1u);

        io.restart();
        io.run_for(std::chrono::seconds(4));

        BOOST_TEST(generator->ConnectionCount() == 0u);
}

BOOST_AUTO_TEST_CASE(renewed_subscription_outlives_peers)
{
        using namespace osrv::event;
        DummyLogger logger;
        std::map<std::string, std::string> namespaces;
        NotificationsManager manager(logger, namespaces, 1, 0, 60);

        auto& io = manager.GetIoContext();
        auto generator = std::make_shared<TestEventGenerator>("tns1:RuleEngine/CellMotionDetector/Motion", io, logger);
        manager.AddGenerator(generator);

        auto renewed = manager.CreatePullPoint({});
        auto short_lived = manager.CreatePullPoint({});
        (void)short_lived;

        const auto renew_time = boost::posix_time::microsec_clock::universal_time();
        BOOST_TEST(renewed->TryRenew(renew_time, boost::posix_time::seconds(3), boost::posix_time::seconds(0)));
        renewed->RefreshTerminationTimer(std::chrono::seconds(3), [&manager](const std::string& ref) {
                manager.Unsubscribe(ref);
        });

        io.run_for(std::chrono::seconds(2));

        BOOST_TEST(generator->ConnectionCount() == 1u);

        io.restart();
        io.run_for(std::chrono::seconds(2));

        BOOST_TEST(generator->ConnectionCount() == 0u);
}
