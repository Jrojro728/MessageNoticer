#include "pch.h"
#include "Message.h"

Message::Message(Packet pkt) : Sender(INVALID_SOCKET), Receiver(INVALID_SOCKET)
{
	Logger logger = GetLogger(LOG4CPLUS_TEXT("Message"));
	Json::Reader Reader;
	Json::Value Root;
	if (!Reader.parse(pkt.GetData(), Root, false))
		throw std::runtime_error("Message constructor: failed to parse packet data as JSON.");

	this->Title = Root["title"].asString();
	switch (Root["content"]["type"].asUInt())
	{
	case Payload::Text:
		this->Content = Payload(Root["content"]["content"].asString(), Root["content"]["type"].asUInt());
		break;
	default:
		LOG_WARN(logger, "unrecognized content type " << Root["content"]["type"].asUInt() << ", defaulting to text.");
		this->Content = Payload(Root["content"]["content"].asString(), Payload::Text);
		break;
	}

	this->MessageUUID = uuid::uuid_from_string(Root["uuid"].asString());
	this->Priority = static_cast<MessagePriority>(Root["priority"].asUInt());
	this->Sender = Client(Root["sender"].as<SOCKET>());
	this->Receiver = Client(Root["receiver"].as<SOCKET>());
	this->SendTime = std::chrono::system_clock::from_time_t(Root["timestamp"].asInt64());
}

Message::operator std::string() const
{
	Json::FastWriter Writer;
	return Writer.write(operator Json::Value());
}

Message::operator Json::Value() const
{
	Json::Value Root;
	Root["title"] = Title;
	Root["priority"] = static_cast<uint8_t>(Priority);

	Json::Value Content = this->Content;

	Root["content"] = Content;
	Root["sender"] = Sender.GetSocket();
	Root["receiver"] = Receiver.GetSocket();
	Root["timestamp"] = GetSendTimeEpoch();
	Root["uuid"] = to_string(MessageUUID);
	return Root;
}