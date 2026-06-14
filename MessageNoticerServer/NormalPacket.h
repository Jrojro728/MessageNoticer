// NormalPacket.h ¡ªPacket types used after handshake is complete.
#pragma once
#include "pch.h"
#include "Network.h"
#include "Packet.h"
#include "Client.h"
#include "Message.h"

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
	SendAMessagePacket(Message msg, unsigned char PacketVersion = 1)
		: Packet(PacketType::SendAMessage, PacketVersion)
	{
		this->AddData(msg.operator std::string());
	};

	std::string GetType() const override
	{
		return "SendAMessagePacket";
	}
};

class BroadcastMessagePacket : public Packet
{
public:
	BroadcastMessagePacket(Message msg, unsigned char PacketVersion = 1)
		: Packet(PacketType::BroadcastMessage, PacketVersion)
	{
		this->AddData(msg.operator std::string());
	};
	std::string GetType() const override
	{
		return "BroadcastMessagePacket";
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

class UnifiedSyncPacket : public Packet
{
public:
	UnifiedSyncPacket(const char* syncInfo, unsigned char PacketVersion = 1)
		: Packet(PacketType::UnifiedSync, PacketVersion)
	{
		this->AddData(syncInfo, strlen(syncInfo));
		this->AddData(
			static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::system_clock::now().time_since_epoch()).count()));// time of the UnifiedSync, used for clients to determine if they need to update their data or not. The time is in seconds since epoch (Unix time).
	};

	std::string GetType() const override
	{
		return "UnifiedSyncPacket";
	}
};

class GetClientListPacket : public Packet
{
public:
	GetClientListPacket(uint8_t level, uint32_t number, unsigned char PacketVersion = 1)
		: Packet(PacketType::GetClientList, PacketVersion)
	{
		this->AddData(level);
		this->AddData(number);
	};

	GetClientListPacket(MessagePriority level, uint32_t number, unsigned char PacketVersion = 1)
		: Packet(PacketType::GetClientList, PacketVersion)
	{
		this->AddData(level);
		this->AddData(number);
	};

	std::string GetType() const override
	{
		return "GetClientListPacket";
	}
};

class SendClientListResponsePacket : public Packet
{
public:
	SendClientListResponsePacket(const std::vector<Client>& ClientList, unsigned char PacketVersion = 1)
		: Packet(PacketType::SendClientListResponse, PacketVersion)
	{
		Json::FastWriter Writer;
		Json::Value Root;

		Json::Value ClientArray(Json::arrayValue);
		for (const auto& client : ClientList)
		{
			Json::Value ClientJson;
			ClientJson["uuid"] = to_string(client.GetClientID());
			ClientJson["name"] = client.GetReadableClientName();
			ClientJson["id"] = client.GetSocket();
			ClientJson["minMessageLevel"] = client.GetMinMessageLevel();
			ClientJson["status"] = client.GetClientStatus();
			ClientArray.append(ClientJson);
		}
		Root["clients"] = ClientArray;
		Root["number"] = ClientList.size();

		std::string clientListJson = Writer.write(Root);
		this->AddData(clientListJson.c_str(), clientListJson.size());
	};

	std::string GetType() const override
	{
		return "SendClientListResponsePacket";
	}
};

class WhoAmIPacket : public Packet
{
public:
	WhoAmIPacket(string reason, unsigned char PacketVersion = 1)
		: Packet(PacketType::WhoAmI, PacketVersion)
	{
		this->AddData(reason);
	};
	std::string GetType() const override
	{
		return "WhoAmIPacket";
	}
};

class WhoAmIResponsePacket : public Packet
{
public:
	WhoAmIResponsePacket(Client client, unsigned char PacketVersion = 1)
		: Packet(PacketType::WhoAmIResponse, PacketVersion)
	{
		Json::FastWriter Writer;
		Json::Value Root;
		Root = client.operator Json::Value();
		std::string identityJson = Writer.write(Root);
		this->AddData(identityJson);
	};
	std::string GetType() const override
	{
		return "WhoAmIResponsePacket";
	}
};