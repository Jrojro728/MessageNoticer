#include "pch.h"
#include "ClientProcess.h"
#include "HandshakePacket.h"
#include "NormalPacket.h"

int HandshakeProcess(SOCKET& sServer)
{
	Logger logger = GetLogger(LOG4CPLUS_TEXT("HandshakeProcess"));
	Json::Reader Reader;
	Json::Value Root;
	
	char randStr[8];
	snprintf(randStr, sizeof(randStr), "%d", rand() % 100000);//Random number as Client's name for temp

	//Send Handshake request
	HandshakePacket("TestClient", 1).Send(sServer);

	Packet Received = Packet::PacketFromNetworkRecv(sServer);
	if (Received.GetPacketID() == PacketType::HandshakeError)	//Check PacketType
		LOG_FATAL(logger, "Handshake error from server: " << Received.GetRawData());

	//Parse 
	if (!(Reader.parse(Received.GetData(), Root, false)))
		LOG_FATAL(logger, "Invalid handshake response from server.");

	if (Root["info"]["maxuser"].asInt() - Root["info"]["useronline"].asInt() < 1)
		HandshakeAckPacket(randStr, No).Send(sServer);

	//Send ack to end handshake
	HandshakeAckPacket(randStr, Ok).Send(sServer);

	//Check Handshake successed
	if (!(Packet::PacketFromNetworkRecv(sServer).GetPacketID() == HandshakeSuccess))
	{
		LOG_ERROR(logger, "Failed to get HandshakeSuccess flag");
	}
	LOG_INFO(logger, "Connected to server: "
		<< Root["info"]["name"].asString()
		<< " Version: " << Root["info"]["version"].asString()
		<< " Protocol: " << Root["info"]["protocol"].asUInt());

    return 0;
}

int NormalProcess(SOCKET& sServer)
{
	Logger logger = GetLogger(LOG4CPLUS_TEXT("NormalProcess"));
	WaitingMessagePacket(0).Send(sServer);
	SendAMessagePacket("Hello, world!\0", uuid::random_generator()(), 1).Send(sServer);

	return 0;
}
