#pragma once
#include "pch.h"
#include "Network.h"
#include "Packet.h"

//¿Í»§¶Ë
class Client
{
public:
	Client() = delete;
	Client(SOCKET s) : ClientSocket(s){};
	Client(SOCKET& s, uuid::uuid uuid) : ClientSocket(s), ClientID(uuid) {};
	Client(SOCKET& s, uuid::uuid uuid, string name) : ClientSocket(s), ClientID(uuid), ReadableClientName(name) {};
	Client(SOCKET& s, uuid::uuid uuid, const char* name) : ClientSocket(s), ClientID(uuid), ReadableClientName(name) {};
	
	int Recv(char*& DataBuffer) { return ::Recv(ClientSocket, DataBuffer); };
	int Send(const char* DataBuffer) { return ::Send(ClientSocket, DataBuffer); };
	int Send(Packet& packet) { return packet.Send(ClientSocket); };
	SOCKET GetSocket() const { return ClientSocket; };
	uuid::uuid GetClientID() const { return ClientID; };
	string GetReadableClientName() const { return ReadableClientName; };
	uint8_t GetMinMessageLevel() const { return MinMessageLevel; };
	void SetMinMessageLevel(uint8_t level) { MinMessageLevel = level; };

	bool operator==(const Client& other) const
	{
		return this->ClientSocket == other.ClientSocket;
	};

	operator SOCKET() const { return ClientSocket; };
private:
	SOCKET ClientSocket; //Socket of the client
	string ReadableClientName; //If exist
	uuid::uuid ClientID; //UUID of the client, used to identify the client
	uint8_t MinMessageLevel = 0; //The minimum message level that the client wants to receive, 0 for all messages, none or 255 for no messages
};


