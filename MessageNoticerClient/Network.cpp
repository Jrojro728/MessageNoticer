#include "pch.h"
#include "Network.h"
#include "Logger.h"

Logger NetworkLogger = GetLogger("Network");

// Helper: loop-read until we get exactly `size` bytes or the connection dies.
// Returns the number of bytes actually read; on error returns -1.
// On graceful close returns a value < size (caller checks).
static int RecvAll(SOCKET s, char* buf, int size)
{
	int total = 0;
	while (total < size)
	{
		int n = recv(s, buf + total, size - total, 0);
		if (n == 0)
			return total; // graceful close
		if (n == SOCKET_ERROR)
		{
#ifdef _WIN32
			if (WSAGetLastError() == WSAEINTR) continue;
#else
			if (errno == EINTR) continue;
#endif
			return -1; // real error
		}
		total += n;
	}
	return total;
}

int InitNetwork()
{
#ifdef _WIN32
    WSADATA wd;
    if (WSAStartup(MAKEWORD(2, 2), &wd) == SOCKET_ERROR)
    {
        LOG_FATAL(NetworkLogger, "WSAStartup error: " << GetSocketError());
        return 1;
    }
#else
    // SIGPIPE would kill the process when writing to a closed socket.
    // Ignore it and handle EPIPE via send() return value instead.
    signal(SIGPIPE, SIG_IGN);
#endif
    return 0;
}

int CreateSocket(SOCKET& s, const char* port, const char* address)
{
    struct addrinfo* result = NULL, * ptr = NULL, hints;
    memset(&hints, 0, sizeof(hints));
    int connectErrno = 0;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(address, port, &hints, &result))
    {
        LOG_FATAL(NetworkLogger, "getaddrinfo failed: " << GetSocketError());
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    for (ptr = result; ptr != NULL; ptr = ptr->ai_next)
    {
        s = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
        if (s == INVALID_SOCKET)
        {
            LOG_FATAL(NetworkLogger, "socket error: " << GetSocketError());
#ifdef _WIN32
            WSACleanup();
#endif
            continue;
        }

#ifdef CLIENT_APP
        if (connect(s, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
            int err = errno;  // save before CloseSocket/WSACleanup clear it
            LOG_FATAL(NetworkLogger, "connect error: " << err);
            CloseSocket(s);
#ifdef _WIN32
            WSACleanup();
#endif
            connectErrno = err;
            s = INVALID_SOCKET;
            continue;
        }
        break;
#endif // CLIENT_APP
    }

#ifdef SERVER_APP
    // Allow reusing the address immediately after restart
    int optval = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&optval, sizeof(optval));

    if (bind(s, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR)
    {
        LOG_FATAL(NetworkLogger, "bind error: " << GetSocketError());
        freeaddrinfo(result);
        CloseSocket(s);
#ifdef _WIN32
        WSACleanup();
#endif
        s = INVALID_SOCKET;
        return 1;
    }

    if (listen(s, SOMAXCONN) == SOCKET_ERROR)
    {
        LOG_FATAL(NetworkLogger, "listen error: " << GetSocketError());
        CloseSocket(s);
#ifdef _WIN32
        WSACleanup();
#endif
        s = INVALID_SOCKET;
        return 1;
    }
#endif // SERVER_APP

    int createErr = (s == INVALID_SOCKET) ? (connectErrno != 0 ? connectErrno : errno) : 0;
    freeaddrinfo(result);
    if (s == INVALID_SOCKET) {
        LOG_FATAL(NetworkLogger, "Unable to create socket! " << createErr);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }
    return 0;
}

int Recv(SOCKET& s, std::vector<char>& DataBuffer)
{
	// ---- Step 1: Read the 4-byte size header (big-endian) ----
	char sizeBuf[4];
	int n = RecvAll(s, sizeBuf, 4);
	if (n != 4)
	{
		DataBuffer.clear();
#ifdef SERVER_APP
		throw ClientSocketClosedException();
#else
		return (n == 0) ? 0 : SOCKET_ERROR;
#endif
	}

	// Parse big-endian uint32_t
	uint32_t packetSize =
		((unsigned char)sizeBuf[0] << 24) |
		((unsigned char)sizeBuf[1] << 16) |
		((unsigned char)sizeBuf[2] << 8)  |
		(unsigned char)sizeBuf[3];

	// Sanity check: min header is 7 bytes, max is MAX_PACKET_SIZE
	if (packetSize < 7 || packetSize > MAX_PACKET_SIZE)
	{
		LOG_ERROR(NetworkLogger, "Invalid packet size: " << packetSize << " from socket " << s);
		DataBuffer.clear();
#ifdef SERVER_APP
		throw ClientSocketClosedException();
#else
		return SOCKET_ERROR;
#endif
	}

	// ---- Step 2: Read the rest of the packet ----
	DataBuffer.resize(packetSize);
	memcpy(DataBuffer.data(), sizeBuf, 4);

	n = RecvAll(s, DataBuffer.data() + 4, packetSize - 4);
	if (n != (int)(packetSize - 4))
	{
		DataBuffer.clear();
#ifdef SERVER_APP
		throw ClientSocketClosedException();
#else
		return (n >= 0) ? 0 : SOCKET_ERROR;
#endif
	}

#ifdef _DEBUG
	LOG_DEBUG(NetworkLogger, s << " recv: " << strToHexString(DataBuffer.data(), packetSize));
#endif

	return (int)packetSize;
}

int Send(SOCKET& s, const char* DataBuffer, int Size)
{
    send(s, DataBuffer, Size, 0);
#ifdef _DEBUG
    LOG_DEBUG(NetworkLogger, s << "recv: " << strToHexString(DataBuffer, Size));
#endif
    return 0;
}

int Send(SOCKET& s, const char* DataBuffer)
{
    send(s, DataBuffer, strlen(DataBuffer), 0);
#ifdef _DEBUG
    LOG_DEBUG(NetworkLogger, s << "recv: " << strToHexString(DataBuffer, strlen(DataBuffer)));
#endif
    return 0;
}

void EndianSwap(char* pData, unsigned int startIndex, unsigned int length)
{
    unsigned int cnt = length / 2;
    unsigned int start = startIndex;
    unsigned int end = startIndex + length - 1;
    for (unsigned int i = 0; i < cnt; i++)
    {
        char tmp = pData[start + i];
        pData[start + i] = pData[end - i];
        pData[end - i] = tmp;
    }
}


