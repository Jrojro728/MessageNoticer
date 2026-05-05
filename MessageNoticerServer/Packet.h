#pragma once
#include "pch.h"
#include "Network.h"

enum PacketType : uint16_t
{
	Null = 0,				//NULL packet
	HandshakeRequest = 1,	//Start Handshake request
	HandshakeInfo = 2,		//Handshake info, such as server name, version, max users, etc.
	HandshakeAck = 3,		//Handshake acknowledge, client acknowledges the handshake info
	HandshakeResponse = 4,	//Response to Handshake request, can be 5 or 6, not implemented
	HandshakeError = 5,		//Error during Handshake process, such as timeout or invalid request
	HandshakeSuccess = 6,	//Handshake success, server and client can start to communicate
	SendAMessage = 7,		//Post a message to the server
	WaitingMessage = 8, 	//Tell other side to send more messages
	RegisterChildServer = 9,//Register a child server to the main server
	UnifiedSync = 10,		//Force synchronize data between all client
	GetClientList = 11,		//Get the list of client at the specified level
};

// Packet base class, a packet with a PacketSize, PacketID, data, version, and UUID.
class Packet
{
public:
	static uuid::random_generator UUIDGenerator; // Random UUID generator 
	Packet() = default;
	Packet(unsigned short packetID, unsigned char packetVersion = 0) : PacketID(packetID), PacketVersion(packetVersion) {};

	inline static Packet PacketFromNetworkRecv(SOCKET s) {
		char* cstr = nullptr;
		int len = Recv(s, cstr);
		if (cstr == nullptr || len <= 0)
			throw std::runtime_error("PacketFromNetworkRecv: no data received");
		return Packet(cstr);
	}

	/// <summary>
	/// recommended constructor for Packet class
	/// </summary>
	/// <param name="packetID">Packet's ID</param>
	/// <param name="data">Packet's raw data</param>
	/// <param name="packetVersion">Packet format version</param>
	/// <param name="packetUUID">UUID</param>
	/// <param name="packetSize">Data size with head sector</param>
	Packet(const char* data, unsigned int packetSize = 0, unsigned short packetID = 0, unsigned char packetVersion = 0);

	/// <summary>
	/// default destructor for Packet class
	/// </summary>
	virtual ~Packet() = default;

	// Set the UUID of the packet
	void SetUUID(uuid::uuid UUID) { PacketUUID = UUID; }; 
	// Return the type of the packet
	virtual std::string GetType() const { return "Packet"; }; 
	// Get the size of the packet data
	unsigned int GetPacketSize() const { return PacketSize; };
	// Get the ID of the packet
	unsigned short GetPacketID() const { return PacketID; };
	// Get the version of the packet format
	char GetPacketVersion() const { return PacketVersion; };
	// Get the UUID of the packet
	uuid::uuid GetPacketUUID() const { return PacketUUID; };
	// Get the raw data of the packet
	const char* GetRawData() const { return Data; };

	/// <summary>
	/// default equality operator for Packet class
	/// </summary>
	virtual bool operator==(const Packet& other) const
	{
		return this->GetType() == other.GetType();
	}
	// Return the raw data
	virtual operator char* () const { return Data; }

	template<typename T>
	void AddData(const T& data)
	{
		if (Data == nullptr)
		{
			char* newData = new char[sizeof(T) + 7]; // 7 bytes for header (PacketSize, PacketID, PacketVersion)
			PacketSize = sizeof(T) + 7; // Set new PacketSize

			newData[0] = (PacketSize >> 24) & 0xFF; // Copy PacketSize
			newData[1] = (PacketSize >> 16) & 0xFF;
			newData[2] = (PacketSize >> 8) & 0xFF;
			newData[3] = PacketSize & 0xFF;
			newData[4] = (PacketID >> 8) & 0xFF; // Copy PacketID
			newData[5] = PacketID & 0xFF;
			newData[6] = PacketVersion & 0xFF; // Copy PacketVersion
			memcpy(newData + 7, &data, sizeof(T)); // Copy data
			delete[] Data; // Delete old data
			Data = newData; // Assign new data
		}
		else
		{
			char* newData = new char[PacketSize + sizeof(T)];
			memcpy(newData, Data, PacketSize); // Copy old data
			memcpy(newData + PacketSize, &data, sizeof(T)); //Copy new data
			Data = newData; // Assign new data
			PacketSize += sizeof(T);
			// ����ͷ����PacketSize�ֶ�
			newData[0] = (PacketSize >> 24) & 0xFF;
			newData[1] = (PacketSize >> 16) & 0xFF;
			newData[2] = (PacketSize >> 8) & 0xFF;
			newData[3] = PacketSize & 0xFF;
		}
	}

	void AddData(const char* data, unsigned int size);
	void AddData(string str) { AddData(str.c_str(), str.size()); }

	/// <summary>
	/// Get data from Packet
	/// </summary>
	/// <typeparam name="T">any type</typeparam>
	/// <param name="offset">where you want to start</param>
	/// <returns>T type value</returns>
	template<typename T>
	T GetData(size_t offset = 0) const
	{
		if (Data == nullptr || PacketSize < sizeof(T) + 7)
			throw std::runtime_error("Packet data is not valid or too small to retrieve data.");
		T result{};
		memcpy(&result, Data + offset + 7, sizeof(T)); // Copy data from the packet
		return result;
	}

	string GetString(size_t offset = 0) const { return std::string(GetData(offset)); };
	const char* GetData(size_t offset = 0) const;

	int Send(SOCKET s) const
	{
		if (Data == nullptr || PacketSize == 0)
			return 0; // No data to send
		return ::Send(s, Data, PacketSize); // Send the packet data
	}

private:
	unsigned short PacketID = 0; //Packet ID for packet's format
	char* Data = nullptr; //Pointer to the data of the packet
	unsigned int PacketSize = 0; //Size of the packet data
	char PacketVersion = 1; //Version of the packet format, 1 for now
	uuid::uuid PacketUUID; //UUID for the packet, if exist
};


