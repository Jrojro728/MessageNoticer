#include "pch.h"
#include "ServerProcess.h"
#include "Message.h"

int broadcastMessage(const Message& msg, const std::vector<Client>& ClientList)
{
	Logger logger = GetLogger(LOG4CPLUS_TEXT("BroadcastMessage"));
	LOG_DEBUG(logger, CLR_BOLD CLR_GREEN "Broadcasting message: " << msg.GetTitle() << " with content: " << msg.GetContentJson() << CLR_RESET);
	if (msg.GetReceiver() != BroadcastClient)
	{
		try
		{
			LOG_DEBUG(logger, CLR_BOLD CLR_GREEN "Sending message to client " << msg.GetReceiver() << CLR_RESET);
			SendAMessagePacket(msg).Send(msg.GetReceiver());
			return 0;
		}
		catch (const std::exception& e)
		{
			LOG_ERROR(logger, CLR_BOLD CLR_RED_BG "Failed to send message to client " << msg.GetReceiver() << ": " << e.what() << CLR_RESET);
			return 1;
		}
	}
	for (const auto& client : ClientList)
	{
		if (client.GetClientStatus() == ClientStatus::Waiting && client.GetMinMessageLevel() <= static_cast<uint8_t>(msg.GetPriority()) && client != msg.GetSender())
		{
			LOG_DEBUG(logger, CLR_BOLD CLR_GREEN "Sending message to client " << client.GetSocket() << CLR_RESET);
			try
			{
				BroadcastMessagePacket(msg).Send(client);
			}
			catch (const std::exception& e)
			{
				LOG_ERROR(logger, CLR_BOLD CLR_RED_BG "Failed to broadcast message to client " << client.GetSocket() << ": " << e.what() << CLR_RESET);
			}
		}
	}
	return 0;
}

int HandshakeProcess(SOCKET& sSelected, std::vector<Client>& ClientList, string ServerName, string Version)
{
	Logger logger = GetLogger(LOG4CPLUS_TEXT("HandshakeProcess"));
	Json::Reader Reader;
	Json::Value Root;
	uuid::random_generator UUIDGenerator;

	// Receive Data from Client and
	// Parse Client's handshake request
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
	HandshakeInfoPacket(ServerName, Version, 64, ClientList.size(), 1, Online).Send(sSelected);
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
	ClientList.erase(std::find(ClientList.begin(), ClientList.end(), Client(sSelected)));
	Client NewClient(sSelected, UUIDGenerator(), Root["name"].asString(), ClientStatus::Ready);
	ClientList.push_back(NewClient);
	HandshakeSuccessPacket(NewClient).Send(sSelected);
	return 0;
}

int NormalProcess(SOCKET& sSelected, std::vector<Client>& ClientList)
{
	Logger logger = GetLogger(LOG4CPLUS_TEXT("NormalProcess"));
	Packet temp = Packet::PacketFromNetworkRecv(sSelected);
	int16_t PacketID = temp.GetPacketID();
	
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
			LOG_DEBUG(logger, "Received SendMessagePacket from client " << sSelected);
			try
			{
				Message Temp = Message(temp);
				LOG_DEBUG(logger, "Message UUID: "		<< Temp.GetMessageUUID());
				LOG_DEBUG(logger, "Message content: "	<< Temp.GetContentJson());
				LOG_DEBUG(logger, "Message priority: "	<< static_cast<int>(Temp.GetPriority()));
				LOG_DEBUG(logger, "Message sender: "	<< Temp.GetSender().GetSocket());
				LOG_DEBUG(logger, "Message receiver: " << ((Temp.GetReceiver() == BroadcastClient) ? "(Broadcast)" : std::to_string(Temp.GetReceiver().GetSocket())));
				LOG_DEBUG(logger, "Message send time: " << Temp.GetFormattedSendTime());
				broadcastMessage(Temp, ClientList);
			}
			catch (const std::exception& e)
			{
				LOG_ERROR(logger, e.what());
				return 1;
			}
			break;
		}
		case PacketType::GetClientList: // GetClientListPacket
		{
			LOG_DEBUG(logger, "Received GetClientListPacket from client " << sSelected);
			std::vector<Client> QualifiedClientList;
			uint8_t RequestMinMsgLevel = temp.GetData<uint8_t>();
			for(auto & client : ClientList)
			{
				if ((client.GetClientStatus() == ClientStatus::Ready || client.GetClientStatus() == ClientStatus::Waiting) && client.GetMinMessageLevel() >= RequestMinMsgLevel)
					QualifiedClientList.push_back(client);
			}
			SendClientListResponsePacket(QualifiedClientList).Send(sSelected);
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
