#pragma once
#include "Packet.h"

enum status : uint8_t
{
	Online = 0x00, // server is online
	Ok = 0x00, 
	Error = 0x01,
	No = 0x01,
	Offline = 0x02, // maybe the background is offline
	Logoff = 0x02, //client
	Full = 0x03, // server is full
	Buzy = 0x04, // try again later
	Maintenance = 0x05, // server is under maintenance
	Other = 0xff
};

class HandshakePacket : public Packet
{
public:
	HandshakePacket(unsigned char ProtocalVersion = 1, const char* ClientName = "Unknown") : Packet(PacketType::HandshakeRequest, ProtocalVersion) {
		Json::FastWriter Writer;
		Json::Value Root;
		Root["fastmessage"] = "Hello from client!";
		Root["name"] = ClientName;
		string request = Writer.write(Root);
		this->AddData(request.c_str(), request.size());
	};

	std::string GetType() const override {
		return "HandshakeRequestPacket";
	}
};

class HandshakeInfoPacket : public Packet
{
public:
	HandshakeInfoPacket(string ServerName, string Version, int maxUser, int UserNow, unsigned char ProtocalVersion = 1, uint8_t status = Online) : Packet(PacketType::HandshakeInfo, ProtocalVersion) 
	{
		Json::FastWriter Writer;
		Json::Value Root;

		Json::Value Info;
		Info["name"] = ServerName;
		Info["version"] = Version;
		Info["protocol"] = ProtocalVersion;
		Info["maxuser"] = maxUser;
		Info["useronline"] = UserNow;

		Json::Value FastMessage;
		FastMessage["text"] = "Hello from server!";

		Root["info"] = Info;
		Root["fastmessage"] = FastMessage;
		Root["status"] = status;
		string description = Writer.write(Root);
		this->AddData(description.c_str(), description.size());
	};

	std::string GetType() const override {
		return "HandshakeInfoPacket";
	}
};

class HandshakeAckPacket : public Packet
{
	public:
	HandshakeAckPacket(const char* ClientName = "Unknown", uint8_t status = 0, unsigned char ProtocalVersion = 1) : Packet(PacketType::HandshakeAck, ProtocalVersion)
	{
		Json::FastWriter Writer;
		Json::Value Root;
		Root["status"] = status;
		Root["name"] = ClientName;
		string ack = Writer.write(Root);
		this->AddData(ack.c_str(), ack.size());
	};

	std::string GetType() const override {
		return "HandshakeAckPacket";
	}
};

class HandshakeErrorPacket : public Packet
{
public:
	HandshakeErrorPacket(const char* reason = NULL) : Packet(PacketType::HandshakeError) 
	{
		this->AddData(reason ? reason : "Handshake error occurred!", strlen(reason));
	};

	std::string GetType() const override 
	{
		return "HandshakeErrorPacket";
	}
};

class HandshakeSuccessPacket : public Packet
{
	public:
	HandshakeSuccessPacket() : Packet(PacketType::HandshakeSuccess) {
		this->AddData(Ok);
	};

	std::string GetType() const override {
		return "HandshakeSuccessPacket";
	}
};
