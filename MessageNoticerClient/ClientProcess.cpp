// ClientProcess.cpp — Handshake logic + select-based packet processing.
#include "pch.h"
#include "ClientProcess.h"
#include "Logger.h"
#include "HandshakePacket.h"
#include "NormalPacket.h"
#include "Message.h"
#include "Colors.h"
#include <functional>
#include <deque>

Client LocalClient = Client(INVALID_SOCKET); //The Client itself

// ── Shared between ConsoleThread and the main loop ──────────────────
static std::mutex gCmdMutex;
static std::deque<std::string> gCmdQueue;

extern volatile std::sig_atomic_t gRunning;

// ── HandshakeProcess ─────────────────────────────────────────────────

/// <summary>
/// Perform the handshake with the server.
///   1. Send HandshakeRequest (with a random client name).
///   2. Receive HandshakeInfo, check max-users.
///   3. Send HandshakeAck.
///   4. Receive HandshakeSuccess and get client itself.
/// Logs the server name / version / protocol on success.
/// </summary>
/// <param name="sServer">Connected server socket.</param>
/// <returns>0 on success, 1 on failure.</returns>
int HandshakeProcess(SOCKET& sServer)
{
	Logger logger = GetLogger(LOG4CPLUS_TEXT("HandshakeProcess"));
	Json::Reader Reader;
	Json::Value Root;

	char randStr[8];
	srand((unsigned int)time(NULL));
	snprintf(randStr, sizeof(randStr), "%d", rand() % 100000);

	HandshakePacket(randStr, 1).Send(sServer);

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
		HandshakeAckPacket(randStr, No).Send(sServer);

	HandshakeAckPacket(randStr, Ok).Send(sServer);

	Received = Packet::PacketFromNetworkRecv(sServer);
	if (Received.GetPacketID() != PacketType::HandshakeSuccess)
	{
		LOG_ERROR(logger, "Failed to get HandshakeSuccess flag");
		return 1;
	}

	// Parse the client info from the handshake success packet
	Json::Value ClientInfo;
	if (!Reader.parse(Received.GetData(sizeof(uint8_t)), ClientInfo, false))
	{
		LOG_ERROR(logger, "Invalid HandshakeSuccess packet data");
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


// ═══════════════════════════════════════════════════════════════════════
//  NormalProcess — select-based packet handler (called from main loop)
// ═══════════════════════════════════════════════════════════════════════

/// <summary>
/// Handle one round of network I/O on |sServer|:
///   1. select() with a 100 ms timeout.
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

	// select() with a 100 ms timeout
	fd_set readset;
	FD_ZERO(&readset);
	FD_SET(sServer, &readset);
	struct timeval tv = { 0, 100000 };

	int ret = select((int)(sServer + 1), &readset, NULL, NULL, &tv);

	if (ret == SOCKET_ERROR) {
		int selErr = GetSocketError();
#ifdef _WIN32
		if (selErr == WSAEINTR) return 0;   // signal — try again
#else
		if (selErr == EINTR) return 0;
#endif
		LOG_ERROR(logger, "select() failed: " << GetSocketError());
		return 1;
	}

	if (ret == 0) return 0;  // timeout — nothing to read

	// ── Socket is readable — receive a packet ───────────────────
	// ── Explicitly check for disconnect before reading a full packet ──
	// Client-side Recv() returns 0/error on EOF instead of throwing
	// ClientSocketClosedException, so PacketFromNetworkRecv would turn
	// it into a generic runtime_error. We peek first to detect the FIN.
	char peekBuf;
	int peekRet = recv(sServer, &peekBuf, 1, MSG_PEEK);
	if (peekRet == 0)
	{
		LOG_FATAL(logger, "Server disconnected.");
		gRunning = 0;
		return 1;
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
			LOG_INFO(logger, CLR_YELLOW "[From " << msg.GetSender().GetSocket()
				<< "]" CLR_RESET " " CLR_CYAN << msg.GetContentJson() << CLR_RESET);
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
	catch (ClientSocketClosedException&)
	{
		LOG_FATAL(logger, "Server disconnected.");
		gRunning = 0;
		return 1;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(logger, "Error handling packet: " << e.what());
	}

	return 0;
}

// ═══════════════════════════════════════════════════════════════════════
//  Console input (used by main's ConsoleThread)
// ═══════════════════════════════════════════════════════════════════════

/// <summary>
/// Background thread: reads stdin via ReadLine() and enqueues lines.
/// Exits when gRunning becomes 0 or stdin closes.
/// </summary>
void ConsoleThread()
{
	while (gRunning)
	{
		char buf[4096];
		int ret = ReadLine(buf, sizeof(buf));
		if (ret <= 0) break;

		size_t len = strlen(buf);
		if (len == 0) continue;

		std::lock_guard<std::mutex> lock(gCmdMutex);
		gCmdQueue.push_back(buf);
	}
}

/// <summary>
/// Non-blocking poll: return the next queued command, or an empty string.
/// </summary>
/// <returns>The next command line, or empty if none.</returns>
std::string PollCommand()
{
	std::lock_guard<std::mutex> lock(gCmdMutex);
	if (gCmdQueue.empty()) return {};
	std::string line = std::move(gCmdQueue.front());
	gCmdQueue.pop_front();
	return line;
}

/// <summary>
/// Tokenise |cmd| and dispatch via a lookup table.
/// </summary>
/// <param name="line">The command line to process (e.g. "/msg 123 Hello")</param>
/// <param name="sServer">Connected server socket, used to send command packets.</param>
void ProcessCommand(const std::string& line, SOCKET& sServer)
{
	Logger logger = GetLogger(LOG4CPLUS_TEXT("Command"));

	std::istringstream iss(line);
	std::vector<std::string> t;
	std::string tok;
	while (iss >> tok) t.push_back(tok);
	if (t.empty()) return;

	const std::string& cmd = t[0];

	// /help
	auto cmdHelp = [&]() {
		LOG_INFO(logger, CLR_CYAN "=== Commands ===" CLR_RESET);
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/help | /h" CLR_RESET "           Show this help");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/msg <id> <text>" CLR_RESET "     Send message to client <id>");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/broadcast <text>" CLR_RESET "    Send message to all");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/list" CLR_RESET "                List online clients");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/level <0-255>" CLR_RESET "       Set min message level");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/exit | /quit" CLR_RESET "        Disconnect and exit");
		LOG_INFO(logger, "  " CLR_BOLD CLR_CYAN "/whoami" CLR_RESET "			   Show your identity");	
	};

	// /msg
	auto cmdMsg = [&]() {
		if (t.size() < 3) {
			LOG_WARN(logger, "Usage: /msg <receiver_id> <text>");
			return;
		}
		SOCKET receiverId;
		try { receiverId = (SOCKET)std::stoi(t[1]); }
		catch (...) { LOG_WARN(logger, "Invalid receiver ID: " << t[1]); return; }

		std::string text;
		for (size_t i = 2; i < t.size(); ++i) {
			if (i > 2) text += " ";
			text += t[i];
		}
		Message msg("", TextContent(text), LocalClient, Client(receiverId));
		SendAMessagePacket(msg, 1).Send(sServer);
		LOG_INFO(logger, CLR_GREEN "Message sent to [" << receiverId << "]" CLR_RESET);
	};

	// /broadcast
	auto cmdBroadcast = [&]() {
		if (t.size() < 2) {
			LOG_WARN(logger, "Usage: /broadcast <text>");
			return;
		}
		std::string text;
		for (size_t i = 1; i < t.size(); ++i) {
			if (i > 1) text += " ";
			text += t[i];
		}
		Message msg("", TextContent(text), LocalClient, BroadcastClient);
		SendAMessagePacket(msg, 1).Send(sServer);
		LOG_INFO(logger, CLR_GREEN "Broadcast sent." CLR_RESET);
	};

	// /list
	auto cmdList = [&]() {
		GetClientListPacket(MessagePriority::Low, 0).Send(sServer);
	};

	// /level
	auto cmdLevel = [&]() {
		uint8_t level = 0;
		if (t.size() >= 2) {
			try {
				int l = std::stoi(t[1]);
				if (l < 0) l = 0;
				if (l > 255) l = 255;
				level = (uint8_t)l;
			} catch (...) {
				LOG_WARN(logger, "Invalid level: " << t[1] << ". Using 0.");
			}
		}
		WaitingMessagePacket(level).Send(sServer);
		LOG_INFO(logger, "Set message level to " CLR_BOLD << (int)level << CLR_RESET);
	};

	auto cmdWhoami = [&]() {
		WhoAmIPacket("whoami?").Send(sServer);
	};

	// /exit | /quit
	auto cmdExit = [&]() {
		LOG_INFO(logger, "Exiting...");
		gRunning = 0;
	};

	// ── Dispatch table ───────────────────────────────────────────
	struct Cmd { const char* name; std::function<void()> fn; };
	const Cmd dispatch[] = {
		{ "/help",      cmdHelp },
		{ "/h",         cmdHelp },
		{ "/msg",       cmdMsg },
		{ "/broadcast", cmdBroadcast },
		{ "/list",      cmdList },
		{ "/level",     cmdLevel },
		{ "/exit",      cmdExit },
		{ "/quit",      cmdExit },
		{ "/whoami",    cmdWhoami },
	};

	for (auto& entry : dispatch) {
		if (cmd == entry.name) {
			entry.fn();
			return;
		}
	}

	LOG_WARN(logger, "Unknown command: \"" << cmd << "\". Type /help");
}
