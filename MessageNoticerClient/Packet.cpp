#include "pch.h"
#include "Packet.h"

Packet::Packet(std::vector<char> buffer)
	: Data(std::move(buffer))
{
	this->PacketUUID = uuid::nil_uuid();

	// Parse header from the buffer.
	// The first 4 bytes are PacketSize (big-endian), but we already have that from Data.size().
	if (Data.size() < 7)
		throw std::runtime_error("Packet: buffer too small for header.");

	PacketID = ((unsigned char)Data[4] << 8) | (unsigned char)Data[5];
	PacketVersion = Data[6];
}

void Packet::WriteHeader()
{
	uint32_t netSize = (uint32_t)Data.size();
	// Big-endian: MSB first
	Data[0] = (char)(netSize >> 24);
	Data[1] = (char)(netSize >> 16);
	Data[2] = (char)(netSize >> 8);
	Data[3] = (char)(netSize);
	Data[4] = (char)(PacketID >> 8);
	Data[5] = (char)(PacketID);
	Data[6] = PacketVersion;
}

void Packet::AddData(const char* data, unsigned int size)
{
	size_t oldSize = Data.size();
	size_t entrySize = 4 + size; // 4-byte size prefix + actual data

	if (oldSize == 0)
	{
		Data.resize(7 + entrySize);
		WriteHeader();
		Data[7] = (char)(size >> 24);
		Data[8] = (char)(size >> 16);
		Data[9] = (char)(size >> 8);
		Data[10] = (char)(size);
		std::memcpy(Data.data() + 11, data, size);
	}
	else
	{
		Data.resize(oldSize + entrySize);
		Data[oldSize + 0] = (char)(size >> 24);
		Data[oldSize + 1] = (char)(size >> 16);
		Data[oldSize + 2] = (char)(size >> 8);
		Data[oldSize + 3] = (char)(size);
		std::memcpy(Data.data() + oldSize + 4, data, size);
		WriteHeader();
	}
}

string Packet::GetData(size_t offset) const
{
	// The payload at Data[7 + offset] starts with a 4-byte length (big-endian)
	// followed by that many bytes of string data
	if (Data.size() < 7 + 4 + offset)
		throw std::runtime_error("Packet data is invalid or too small to retrieve data.");

	uint32_t subSize =
		((unsigned char)Data[7 + offset] << 24) |
		((unsigned char)Data[8 + offset] << 16) |
		((unsigned char)Data[9 + offset] << 8) |
		(unsigned char)Data[10 + offset];

	if (7 + 4 + subSize + offset > Data.size())
		throw std::runtime_error("Packet data is invalid or too small to retrieve data.");

	string result(Data.data() + 7 + 4 + offset, subSize);
	return result;
}
