// ClientProcess.cpp ¡ª Handshake logic + select-based packet processing.
#include "pch.h"
#include "ClientProcess.h"
#include "Logger.h"
#include "HandshakePacket.h"
#include "NormalPacket.h"
#include "Message.h"
#include "Colors.h"

// Global running flag
extern volatile std::sig_atomic_t gRunning;
extern volatile std::sig_atomic_t gDisconnected;

Client LocalClient = Client(INVALID_SOCKET); //The Client itself

// ©¤©¤ HandshakeProcess ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤

/// <summary>
/// Perform the handshake with the server.
///   1. Send HandshakeRequest.
///   2. Receive HandshakeInfo, check max-users.
///   3. Send HandshakeAck.
///   4. Receive HandshakeSuccess and get client itself.
/// Logs the server name / version / protocol on success.
/// </summary>
/// <param name="sServer">Connected server socket.</param>
/// <returns>0 on success, 1 on failure.</returns>
int HandshakeProcess(SOCKET& sServer, string ClientName)
{
	Logger logger = GetLogger(LOG4CPLUS_TEXT("HandshakeProcess"));
	Json::Reader Reader;
	Json::Value Root;

	HandshakePacket(ClientName.c_str(), 1).Send(sServer);

	Packet Received = Packet::PacketFromNetworkRecv(sServer);
	if (Received.GetPacketID() == PacketType::HandshakeError)
	{
		LOG_FATAL(logger, "Handshake error from server: " << Received.GetRawData());
		return 1;
	}

	if (!Reader.parse(Received.GetData(), Root, false))
	{
		LOG_FATAL(logger, "Invalid handshake response from server.");
		return 1;
	}

	// If the server is full, send a "No" ack so it knows we're leaving
	if (Root["info"]["maxuser"].asInt() - Root["info"]["useronline"].asInt() < 1)
	{
		HandshakeAckPacket(ClientName.c_str(), No).Send(sServer);
		return 1;
	}

	HandshakeAckPacket(ClientName.c_str(), Ok).Send(sServer);

	Received = Packet::PacketFromNetworkRecv(sServer);
	if (Received.GetPacketID() != PacketType::HandshakeSuccess)
	{
		LOG_ERROR(logger, "Failed to get HandshakeSuccess flag!");
		return 1;
	}

	// Parse the client info from the handshake success packet
	Json::Value ClientInfo;
	if (!Reader.parse(Received.GetData(sizeof(uint8_t)), ClientInfo, false))
	{
		LOG_ERROR(logger, "Invalid HandshakeSuccess packet received!");
		return 1;
	}
	LocalClient = Client(ClientInfo["id"].asUInt64(), uuid::uuid_from_string(ClientInfo["uuid"].asString()), ClientInfo["name"].asString(), ClientInfo["status"].asUInt());

	LOG_INFO(logger, CLR_BOLD "Connected to server: "
		<< Root["info"]["name"].asString()
		<< " Version: " << Root["info"]["version"].asString()
		<< " Protocol: " << Root["info"]["protocol"].asUInt() << CLR_RESET);
	LOG_INFO(logger, "Server message: " << Root["fastmessage"]["text"].asString());
	LOG_INFO(logger, CLR_BOLD  CLR_YELLOW "Client ID: " << LocalClient.GetClientID() << " Socket(server): " << LocalClient.GetSocket() << CLR_RESET);

	return 0;
}


// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T
//  NormalProcess ¡ª select-based packet handler (called from main loop)
// ¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T¨T

/// <summary>
/// Handle one round of network I/O on |sServer|:
///   1. select() with a 100?ms timeout.
///   2. If the socket is readable, receive and dispatch the packet.
/// Returns when either:
///   - A full packet was processed (return 0), or
///   - The socket closed or an error occurred (return 1 / exception).
///
/// The caller (main event loop) is responsible for:
///   - Polling the command queue and calling ProcessCommand().
///   - Checking gRunning and cleaning up on disconnect.
/// </summary>
/// <param name="sServer">Connected server socket.</param>
/// <returns>0 on normal return, 1 on disconnect or select error.</returns>
int NormalProcess(SOCKET& sServer)
{
	Logger logger = GetLogger(LOG4CPLUS_TEXT("NormalProcess"));
	Json::Reader reader;

	// select() with a 100?ms timeout
	fd_set readset;
	FD_ZERO(&readset);
	FD_SET(sServer, &readset);
	struct timeval tv = { 0, 100000 };

	int ret = select((int)(sServer + 1), &readset, NULL, NULL, &tv);

	if (ret == SOCKET_ERROR) {
		int selErr = GetSocketError();
#ifdef _WIN32
		if (selErr == WSAEINTR) return 0;   // signal ¡ª try again
#else
		if (selErr == EINTR) return 0;
#endif
		LOG_ERROR(logger, "select() failed: " << GetSocketError());
		return 1;
	}

	if (ret == 0) return 0;  // timeout ¡ª nothing to read

	// ©¤©¤ Socket is readable ¡ª receive a packet ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤
	// ©¤©¤ Explicitly check for disconnect before reading a full packet ©¤©¤
	// Client-side Recv() returns 0/error on EOF instead of throwing
	// SocketClosedException, so PacketFromNetworkRecv would turn
	// it into a generic runtime_error. We peek first to detect the FIN.
	char peekBuf;
	int peekRet = recv(sServer, &peekBuf, 1, MSG_PEEK);
	if (peekRet == 0)
	{
		LOG_FATAL(logger, "Server disconnected.");
		gDisconnected = 1;
		return -1;
	}
	if (peekRet == SOCKET_ERROR)
	{
#ifdef _WIN32
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK) return 0;
#else
		int err = errno;
		if (err == EAGAIN || err == EWOULDBLOCK) return 0;
#endif
		LOG_ERROR(logger, "Socket error: " << err);
		if (err == 10054)
		{
			LOG_ERROR(logger, CLR_RED "Connection reset by peer. Server may have closed the connection." CLR_RESET);
			gDisconnected = 1;
			return -1;
		}
		gRunning = 0;
		return 1;
	}

	try
	{
		Packet pkt = Packet::PacketFromNetworkRecv(sServer);

		switch (pkt.GetPacketID())
		{
		case PacketType::BroadcastMessage:
		{
			Message msg(pkt);
			LOG_INFO(logger, CLR_YELLOW "[From " << msg.GetSender().GetSocket()
				<< "]" CLR_RESET " " CLR_CYAN << msg.GetContentJson() << CLR_RESET);
			break;
		}
		case PacketType::SendClientListResponse:
		{
			Json::Value root;
			if (reader.parse(pkt.GetData(), root, false))
			{
				LOG_INFO(logger, CLR_BOLD "Online clients (" << root["number"].asUInt() << "):" CLR_RESET);
				for (auto& cl : root["clients"])
				{
					LOG_INFO(logger, "  ID:" CLR_CYAN << cl["id"].asInt() << CLR_RESET
						<< "  Name:\"" CLR_GREEN << cl["name"].asString() << CLR_RESET
						<< "\"  Level:" << cl["minMessageLevel"].asUInt()
						<< "  Status:" << cl["status"].asUInt());
				}
			}
			break;
		}
		case PacketType::SendAMessage:
		{
			Message msg(pkt);
			Json::Value contentJson;
			if (!reader.parse(pkt.GetData(), contentJson, false))
			{
				LOG_ERROR(logger, "Invalid message content");
				return 1;
			}
			LOG_INFO(logger, CLR_YELLOW << msg.GetSender().GetReadableClientName()
				<< ":" CLR_RESET " " CLR_CYAN << contentJson["content"]["content"].asString() << CLR_RESET);
			break;
		}
		case PacketType::WhoAmIResponse:
		{
			// Read the client info from the packet and update LocalClient
			Json::Value ClientInfo;
			if (!reader.parse(pkt.GetData(), ClientInfo, false))
			{
				LOG_ERROR(logger, "Invalid WhoAmIResponse packet data");
				return 1;
			}
			LocalClient = Client(ClientInfo["id"].asUInt64(), uuid::uuid_from_string(ClientInfo["uuid"].asString()), ClientInfo["name"].asString(), ClientInfo["status"].asUInt());
			LOG_INFO(logger,  CLR_BOLD "You are Client ID: " CLR_YELLOW << LocalClient.GetClientID()
				<< CLR_RESET CLR_BOLD CLR_GREEN " Name: \"" CLR_RESET CLR_BOLD CLR_YELLOW << LocalClient.GetReadableClientName() << CLR_RESET CLR_BOLD CLR_GREEN "\" Socket(server): " CLR_BOLD CLR_YELLOW << LocalClient.GetSocket() << CLR_RESET);
			break;
		}
		default:
			LOG_DEBUG(logger, "Received packet ID: " << pkt.GetPacketID());
			break;
		}
	}
	catch (SocketClosedException&)
	{
		LOG_FATAL(logger, "Server disconnected.");
		gDisconnected = 1;
		return -1;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(logger, "Error handling packet: " << e.what());
	}

	return 0;
}
