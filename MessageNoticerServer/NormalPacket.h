// NormalPacket.h ¡ªPacket types used after handshake is complete.
#pragma once
#include "pch.h"
#include "Network.h"
#include "Packet.h"

// Sent by a client to tell the server what minimum message level it wants to receive.
// Payload: 1 byte (uint8_t) = MinMessageLevel (0 = all, 255 = none).
class WaitingMessagePacket : public Packet
{
public:
	WaitingMessagePacket(uint8_t minPacketLevel = 0, unsigned char PacketVersion = 1)
		: Packet(PacketType::WaitingMessage, PacketVersion)
	{
		this->AddData(minPacketLevel);
	};

	std::string GetType() const override
	{
		return "WaitingMessagePacket";
	}
};

// Sent by a client to broadcast a message to other connected clients.
// Payload: 16 bytes (UUID) + 4 bytes (message length) + N bytes (message text).
class SendAMessagePacket : public Packet
{
public:
	SendAMessagePacket(const char* message, uuid::uuid messageUUID, unsigned char PacketVersion = 1)
		: Packet(PacketType::SendAMessage, PacketVersion)
	{
		this->AddData(messageUUID);
		this->AddData(message, strlen(message));
	};

	std::string GetType() const override
	{
		return "SendAMessagePacket";
	}
};

class RegisterChildServerPacket : public Packet
{
public:
	RegisterChildServerPacket(const char* ServerName, const char* ServerAddress, int port = 12306, int maxUser = 64, unsigned char PacketVersion = 1)
		: Packet(PacketType::RegisterChildServer, PacketVersion)
	{
		Json::FastWriter Writer;
		Json::Value Root;
		Root["name"] = ServerName;
		Root["address"] = ServerAddress;
		Root["port"] = port;
		Root["maxUser"] = maxUser;
		std::string description = Writer.write(Root);
		this->AddData(description.c_str(), description.size());
	};

	std::string GetType() const override
	{
		return "RegisterChildServerPacket";
	}
};

class UnifiedSync : public Packet
{
public:
	UnifiedSync(const char* syncInfo, unsigned char PacketVersion = 1)
		: Packet(PacketType::UnifiedSync, PacketVersion)
	{
		Json::FastWriter Writer;
		Json::Value Root;
		Root["info"] = syncInfo;
		Root["timestamp"] = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
		string description = Writer.write(Root);
		this->AddData(description.c_str(), description.size());
	};

};