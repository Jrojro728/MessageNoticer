#include "pch.h"
#include "Packet.h"

Packet::Packet(SOCKET s)
{
	PacketSize = ::Recv(s, Data);
	Packet(Data, PacketSize);
}

Packet::Packet(const char* data, unsigned int packetSize, unsigned short packetID, unsigned char packetVersion)
{
	this->Data = const_cast<char *>(data);
	this->PacketVersion = packetVersion;
	this->PacketSize = packetSize;
	this->PacketID = packetID;
	this->PacketUUID = uuid::nil_uuid(); // Initialize with nil UUID

	// If the data of header sector are not provided, it will be extracted from the data array
	if (!packetSize)
	{
		char* temp = new char[4];
		memcpy(temp, data, 4); // Copy first 4 bytes for PacketSize
		EndianSwap(temp, 0, 4); // Swap endianness for PacketSize
		PacketSize = *(unsigned int*)temp;
		delete[] temp;
	}
	
	if (!packetID)
	{
		char* temp = new char[2];
		memcpy(temp, data + 4, 2); // Copy the ID bytes
		EndianSwap(temp, 0, 2); // Swap endianness for PacketID
		PacketID = *(unsigned short*)temp;
		delete[] temp;
	}

	if (!packetVersion)
	{
		char* temp = new char[1];
		memcpy(temp, data + 6, 1); // Copy the version byte
		EndianSwap(temp, 0, 1); // Swap endianness for PacketVersion
		PacketVersion = *(unsigned char*)temp;
		delete[] temp;
	}
}

void Packet::AddData(const char* data, unsigned int size)
{
	if (Data == nullptr)
	{
		char* newData = new char[size + 7]; // 7 bytes for header (PacketSize, PacketID, PacketVersion)
		PacketSize = size + 7 + 4; // Set new PacketSize
		newData[0] = (PacketSize >> 24) & 0xFF; // Copy PacketSize
		newData[1] = (PacketSize >> 16) & 0xFF;
		newData[2] = (PacketSize >> 8) & 0xFF;
		newData[3] = PacketSize & 0xFF;
		newData[4] = (PacketID >> 8) & 0xFF; // Copy PacketID
		newData[5] = PacketID & 0xFF;
		newData[6] = PacketVersion & 0xFF; // Copy PacketVersion
		newData[7] = (size >> 24) & 0xFF; // Copy size
		newData[8] = (size >> 16) & 0xFF;
		newData[9] = (size >> 8) & 0xFF;
		newData[10] = size & 0xFF;
		memcpy(newData + 11, data, size); // Copy data
		delete[] Data; // Delete old data
		Data = newData; // Assign new data
	}
	else
	{
		char* newData = new char[PacketSize + size];
		PacketSize += size; // Set new PacketSize
		newData[0] = (PacketSize >> 24) & 0xFF; // Copy PacketSize
		newData[1] = (PacketSize >> 16) & 0xFF;
		newData[2] = (PacketSize >> 8) & 0xFF;
		newData[3] = PacketSize & 0xFF;
		memcpy(newData + 4, Data, PacketSize); // Copy old data
		memcpy(newData + PacketSize, data, size); //Copy new data
		delete[] Data; // Delete old data
		Data = newData; // Assign new data
		PacketSize += size;
	}
}

const char* Packet::GetData(size_t offset) const
{
	if (Data == nullptr || PacketSize < 7 + 5 + offset)
		throw std::runtime_error("Packet data is not valid or too small to retrieve data.");
	char* result = new char[PacketSize - 7];
	char* temp = new char[5];
	memset(result, 0, PacketSize - 7);
	memset(temp, 0, 5);
	memcpy(temp, Data + offset + 7, 4); // Copy size from the packet
	EndianSwap(temp, 0, 4); // Swap endianness for size
	unsigned int size = *(unsigned int*)temp;
	if (size + 7 + 4 > PacketSize)
		throw std::runtime_error("Packet data is not valid or too small to retrieve data.");
	memcpy(result, Data + offset + 7 + 4, size); // Copy data from the packet
	return result;
}
