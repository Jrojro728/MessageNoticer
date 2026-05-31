// Message.h ¡ªRepresents a message with sender, receiver, title, content, UUID,
//              timestamp (via std::chrono), and importance level.
#pragma once
#include "pch.h"
#include "Logger.h"
#include "Client.h"

// Importance level for a message.
enum class MessagePriority : uint8_t
{
	Low = 0,  // Informational, no action needed
	Normal = 1,  // Default ¡ªstandard communication
	High = 2,	// Requires attention
	Urgent = 3 ,  // Time-sensitive, needs immediate handling
	None = 255 // Special value to indicate no messages
};

class Message
{
public:
	// Default constructor is deleted because Client has no default constructor.
	Message() = delete;

	// Full constructor.
	// Pass INVALID_SOCKET as a sentinel to indicate "no specific sender/receiver".
	Message(string title = "", string content = "", Client Sender = INVALID_SOCKET,
		Client Receiver = INVALID_SOCKET,
		uuid::uuid messageUUID = uuid::random_generator()(),
		MessagePriority priority = MessagePriority::Normal)
		: Title(std::move(title)), Content(std::move(content)),
		MessageUUID(messageUUID), Priority(priority),
		SendTime(std::chrono::system_clock::now()),
		Sender(Sender), Receiver(Receiver)
	{
	};
	~Message() = default;

	// ---------- Getters ----------

	std::string GetTitle() const { return Title; };
	std::string GetContent() const { return Content; };
	uuid::uuid GetMessageUUID() const { return MessageUUID; };
	MessagePriority GetPriority() const { return Priority; };
	Client GetSender() const { return Sender; };
	Client GetReceiver() const { return Receiver; };
	std::chrono::system_clock::time_point GetSendTime() const { return SendTime; };

	// Returns send time as a Unix timestamp (seconds since epoch).
	std::time_t GetSendTimeEpoch() const {
		return std::chrono::system_clock::to_time_t(SendTime);
	};

	// Returns a formatted UTC time string like "2026-05-05 11:30:45".
	// Converts chrono time_point 'time_t' to std::tm for formatting.
	// This avoids C++20 chrono floor / year_month_day / hh_mm_ss pitfalls
	// while still storing the timestamp as std::chrono::system_clock::time_point.
	std::string GetFormattedSendTime() const {
		char buf[100]{};
		std::time_t tt = std::chrono::system_clock::to_time_t(SendTime);
		std::tm tm{};
#if defined(_WIN32) || defined(_WIN64)
		if (gmtime_s(&tm, &tt) == 0) {
			std::strftime(buf, sizeof buf, "%F %T", &tm);
			return std::string(buf);
		}
#else
		// POSIX: gmtime_r
		if (gmtime_r(&tt, &tm)) {
			std::strftime(buf, sizeof buf, "%F %T", &tm);
			return std::string(buf);
		}
#endif
		return {};
	}

	// Returns the priority as a human-readable string.
	std::string GetPriorityString() const {
		switch (Priority) {
		case MessagePriority::Low:    return "Low";
		case MessagePriority::Normal: return "Normal";
		case MessagePriority::High:   return "High";
		case MessagePriority::Urgent: return "Urgent";
		default:                      return "Unknown";
		}
	}

	// ---------- Setters ----------

	void SetTitle(const char* title) { Title = std::string(title); };
	void SetTitle(string title) { Title = std::move(title); };
	void SetContent(const char* content) { Content = std::string(content); };
	void SetContent(string content) { Content = std::move(content); };
	void SetPriority(MessagePriority priority) { Priority = priority; };
	void SetReceiver(Client receiver) { Receiver = receiver; };
	void SetUUID(uuid::uuid uuid) { MessageUUID = uuid; };

private:
	std::string Title;                           // Message title / subject
	std::string Content;                         // Message body
	uuid::uuid MessageUUID;                      // Unique identifier for this message
	MessagePriority Priority;                    // Importance level
	std::chrono::system_clock::time_point SendTime; // Creation timestamp
	Client Sender;                               // Originating client
	Client Receiver;                             // Target client
};

