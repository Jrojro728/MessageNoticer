// NormalPacket.h ¡ªPacket types used after handshake is complete.
#pragma once
#include "pch.h"
#include "Network.h"
#include "Client.h"
#include "Packet.h"

// Sent by a client to tell the server what minimum message level it wants to receive.
// Payload: 1 byte (uint8_t) = MinMessageLevel (0 = all, 255 = none).
class WaitingMessagePacket : public Packet
{
public:
	WaitingMessagePacket(unsigned char PacketVersion, uint8_t minPacketLevel = 0)
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
	SendAMessagePacket(unsigned char PacketVersion, const char* message, uuid::uuid messageUUID)
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

