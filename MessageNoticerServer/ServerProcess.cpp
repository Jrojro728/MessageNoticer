#include "pch.h"
#include "ServerProcess.h"

int HandshakeProcess(SOCKET& sSelected, std::vector<Client>& ClientList)
{
	Logger logger = GetLogger(LOG4CPLUS_TEXT("HandshakeProcess"));
	Json::Reader Reader;
	Json::Value Root;
	uuid::random_generator UUIDGenerator;

	// 接收客户端的数据
	// 客户端握手请求
	if (!Reader.parse(Packet::PacketFromNetworkRecv(sSelected).GetData(), Root, false))
	{
		LOG_ERROR(logger, "Request parse failed!");
		HandshakeErrorPacket("Invalid handshake request.").Send(sSelected);
		return 1;
	}
	if (strcmp(Root["fastmessage"].asCString(), "Hello from client!"))
	{
		LOG_ERROR(logger, "Request parse failed! The format is bad");
		HandshakeErrorPacket("Invalid handshake request.").Send(sSelected);
		return 1;
	}

	// 给客户端发送信息
	HandshakeInfoPacket("blabla", "b1", 64, ClientList.size(), 1, Online).Send(sSelected);
	// 客户端确认
	if (!Reader.parse(Packet::PacketFromNetworkRecv(sSelected).GetData(), Root, false))
	{
		LOG_ERROR(logger, "Ack parse failed!");
		HandshakeErrorPacket("Invalid handshake ack.").Send(sSelected);
		return 1;
	}
	if (!(Root["status"].asUInt() == Ok))
	{
		LOG_ERROR(logger, "Clientside error!");
		HandshakeErrorPacket().Send(sSelected);
		return 1;
	}

	//发送握手成功包
	HandshakeSuccessPacket().Send(sSelected);
	ClientList.push_back(Client(sSelected, UUIDGenerator(), Root["name"].asString()));
	return 0;
}

int NormalProcess(SOCKET& sSelected, std::vector<Client>& ClientList)
{
	Logger logger = GetLogger(LOG4CPLUS_TEXT("NormalProcess"));
	Packet temp = Packet::PacketFromNetworkRecv(sSelected);
	if (temp.GetPacketID() != PacketType::WaitingMessage)
	{
		LOG_ERROR(logger, "Clientside error.");
		return 1;
	}

	auto it = std::find(ClientList.begin(), ClientList.end(), Client(sSelected));
	if (it != ClientList.end())
		it->SetMinMessageLevel(temp.GetData<uint8_t>());
	
	return 0;
}


