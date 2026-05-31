#include "pch.h"
#include "ServerProcess.h"
#include "Message.h"

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
	if (strcmp(Root["fastmessage"].asCString(), "Hello from client!") != 0)
	{
		LOG_ERROR(logger, "Request parse failed! The format is bad");
		HandshakeErrorPacket("Invalid handshake request.").Send(sSelected);
		return 1;
	}

	// Send info to client
	HandshakeInfoPacket("blabla", "b1", 64, ClientList.size(), 1, Online).Send(sSelected);
	// Ack from client
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

	//Send Handshake success
	HandshakeSuccessPacket().Send(sSelected);
	ClientList.erase(std::find(ClientList.begin(), ClientList.end(), Client(sSelected)));
	ClientList.push_back(Client(sSelected, UUIDGenerator(), Root["name"].asString(), ClientStatus::Ready));
	return 0;
}

int NormalProcess(SOCKET& sSelected, std::vector<Client>& ClientList)
{
	Logger logger = GetLogger(LOG4CPLUS_TEXT("NormalProcess"));
	Packet temp = Packet::PacketFromNetworkRecv(sSelected);
	int16_t PacketID = temp.GetPacketID();
	Message Temp{""};
	
	switch (PacketID)
	{
		case PacketType::WaitingMessage: // SetMinMessageLevelPacket
		{
			auto it = std::find(ClientList.begin(), ClientList.end(), Client(sSelected));
			if (it != ClientList.end())
			{
				uint8_t MinMessageLevel = temp.GetData<uint8_t>();
				it->SetMinMessageLevel(MinMessageLevel);
				LOG_DEBUG(logger, "Set min message level as: " << MinMessageLevel << " for client " << sSelected);
				it->SetClientStatus(ClientStatus::Waiting);
			}
			break;
		}
		case PacketType::SendAMessage: // SendMessagePacket
		{
			//<TODO> 让message类变成json格式且在SendAMessagePacket中使用
			LOG_DEBUG(logger, "Received SendMessagePacket from client " << sSelected);
			/*Json::Reader Reader;
			Json::Value Root;*/
			Temp.SetUUID(temp.GetData<uuid::uuid>()); // Get the message's ID
			LOG_DEBUG(logger, "Message UUID: " << Temp.GetMessageUUID()); //为什么log会输出两次？ <TODO>解决这个问题
			LOG_DEBUG(logger, "Message content: " << temp.GetData(sizeof(uuid::uuid)));
			/*if (!Reader.parse(temp.GetData(sizeof(uuid::uuid)), Root, false))
			{
				LOG_ERROR(logger, "MessagePacket parse failed!");
				break;
			}
			LOG_DEBUG(logger, "Message content: " << Root["content"].asString()); */
			break;
		}
		default:
		{ 
			LOG_WARN(logger, "Received unknown packetID: " << PacketID << " from client " << sSelected << ". Did the client send error packet?");
			break;
		}
	}
	
	return 0;
}


