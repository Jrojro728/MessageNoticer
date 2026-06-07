#pragma once
#include "pch.h"
#include "Network.h"
#include "Packet.h"


enum ClientStatus : uint8_t
{
	Unknown = 255,		//The server has not received any status update from the client yet. The server should treat the client as if it is in the Handshaking state, and may choose to disconnect it if it stays in this state for too long.
	Ready = 0,			//Everything is fine
	Handshaking = 1,	//The client just connected and is handshaking
	Error = 2,			//The client has encountered an error and cannot continue normal operation.
	Reattempting = 3,	//The client is trying to re-establish connection or retry something.
	Busy = 4,			//The client is busy processing something and cannot receive messages from the server.
	Waiting = 5,		//The client is waiting for something(etc. waiting for a message or a response or a flag change)
	Panic = 6,			//The client is in a panicked state, which means it has encountered a critical error and cannot continue normal operation, but it is trying to recover.
};

/// <summary>
/// Base class for connected clients. Contains the client's socket, UUID, and other info. Provides methods for sending and receiving data.
/// </summary>
class Client
{
public:
	Client() = delete;
	Client(SOCKET s, uint8_t status = ClientStatus::Unknown) : ClientSocket(s), ClientStatus(status) {};
	Client(SOCKET& s, uuid::uuid uuid, string name = "", uint8_t status = ClientStatus::Unknown) : ClientSocket(s), ClientID(uuid), ReadableClientName(name), ClientStatus(status) {};
	Client(SOCKET& s, uuid::uuid uuid, const char* name = "", uint8_t status = ClientStatus::Unknown) : ClientSocket(s), ClientID(uuid), ReadableClientName(name), ClientStatus(status) {};
	
	int Recv(std::vector<char>& DataBuffer) { return ::Recv(ClientSocket, DataBuffer); };
	int Send(const char* DataBuffer) { return ::Send(ClientSocket, DataBuffer); };
	int Send(Packet& packet) { return packet.Send(ClientSocket); };

	//Getter & Setter
	SOCKET GetSocket() const { return ClientSocket; };
	uuid::uuid GetClientID() const { return ClientID; };
	string GetReadableClientName() const { return ReadableClientName; };
	uint8_t GetMinMessageLevel() const { return MinMessageLevel; };
	uint8_t GetClientStatus() const { return ClientStatus; };
	void SetMinMessageLevel(uint8_t level) { MinMessageLevel = level; };
	void SetClientStatus(uint8_t status) { ClientStatus = status; };

	bool operator==(const Client& other) const
	{
		return this->ClientSocket == other.ClientSocket;
	};

	bool operator!=(const Client& other) const
	{
		return this->ClientSocket != other.ClientSocket;
	};

	operator SOCKET() const { return ClientSocket; };
	operator Json::Value() const
	{
		Json::Value Root;
		Root["uuid"] = to_string(ClientID);
		Root["name"] = ReadableClientName;
		Root["minMessageLevel"] = MinMessageLevel;
		Root["status"] = ClientStatus;
		return Root;
	};
private:
	SOCKET ClientSocket;						//Socket of the client
	string ReadableClientName =  "";			//If exist
	uuid::uuid ClientID = uuid::nil_uuid();		//UUID of the client, used to identify the client
	uint8_t MinMessageLevel = 0;				//The minimum message level that the client wants to receive, 0 for all messages, none or 255 for no messages
	uint8_t ClientStatus = 0;					//The status of the client, see ClientStatus Enum
};


