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
	SendClientList = 12,	//Send the list of client to the other side
};

// Packet base class, a self-buffered packet with automatic memory management.
// The buffer layout is: [PacketSize(4B)][PacketID(2B)][PacketVersion(1B)][payload...]
// All sizes in header are in big-endian (network byte order).
class Packet
{
public:
	static uuid::random_generator UUIDGenerator; // Random UUID generator 
	Packet() = default;
	Packet(unsigned short packetID, unsigned char packetVersion = 0) : PacketID(packetID), PacketVersion(packetVersion) {};

	/// Construct from a received network buffer (moves ownership).
	explicit Packet(std::vector<char> buffer);

	/// Factory: receive a complete packet from a socket.
	inline static Packet PacketFromNetworkRecv(SOCKET s) {
		std::vector<char> buf;
		int len = Recv(s, buf);
		if (buf.empty() || len <= 0)
			throw std::runtime_error("PacketFromNetworkRecv: no data received");
		return Packet(std::move(buf));
	}

	/// <summary>
	/// default destructor for Packet class
	/// </summary>
	virtual ~Packet() = default;

	void SetUUID(uuid::uuid UUID) { PacketUUID = UUID; }
	virtual std::string GetType() const { return "Packet"; }
	size_t GetPacketSize() const { return Data.size(); }
	unsigned short GetPacketID() const { return PacketID; }
	char GetPacketVersion() const { return PacketVersion; }
	uuid::uuid GetPacketUUID() const { return PacketUUID; }
	const char* GetRawData() const { return Data.data(); }

	/// <summary>
	/// default equality operator for Packet class
	/// </summary>
	virtual bool operator==(const Packet& other) const
	{
		return this->GetType() == other.GetType();
	}

	// Get a pointer to the raw buffer (including header).
	operator const char* () const { return Data.data(); }

	/// Append a POD value to the payload.
	template<typename T>
	void AddData(const T& data)
	{
		size_t oldSize = Data.size();
		if (oldSize == 0)
		{
			Data.resize(7 + sizeof(T));
			WriteHeader();
			std::memcpy(Data.data() + 7, &data, sizeof(T));
		}
		else
		{
			Data.resize(oldSize + sizeof(T));
			std::memcpy(Data.data() + oldSize, &data, sizeof(T));
			WriteHeader();
		}
	}

	/// Append a length-prefixed data block to the payload.
	void AddData(const char* data, unsigned int size);
	void AddData(const std::string& str) { AddData(str.data(), (unsigned int)str.size()); }

	/// Extract a POD value from the payload at the given offset.
	template<typename T>
	T GetData(size_t offset = 0) const
	{
		if (Data.size() < 7 + sizeof(T) + offset)
			throw std::runtime_error("Packet data is not valid or too small to retrieve data.");
		T result{};
		std::memcpy(&result, Data.data() + 7 + offset, sizeof(T));
		return result;
	}

	/// Get a pointer to the payload sub-block at offset.
	/// The returned pointer is valid as long as the Packet object is alive.
	string GetData(size_t offset = 0) const;

	/// Send the complete packet (including header) to a socket.
	int Send(SOCKET s) const
	{
		if (Data.empty())
			return 0;
		return ::Send(s, Data.data(), (int)Data.size());
	}

private:
	/// Write the 7-byte header at the front of Data, using big-endian byte order.
	void WriteHeader();

	unsigned short PacketID = 0;
	char PacketVersion = 1;
	std::vector<char> Data;
	uuid::uuid PacketUUID;
};
