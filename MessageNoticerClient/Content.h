#pragma once
#include "pch.h"
class Payload
{
public:
	// default constructor (init payload as text content)
	Payload() : Content(""), Type(Text) {}

	Payload(string str, uint8_t type) : Content(str), Type(type) {}

	enum ContentType : uint8_t
	{
		Text = 0,		// Plain text message
		JSON = 1,		// JSON-formatted message
		XML = 2,		// XML-formatted message
		Binary = 3,		// Binary data message
		Attachment = 4,	// Message with file attachment (1.metadata 2.content) <TODO>
		Image = 5,		// Image message (1.image metadata 2.binary data) <TODO>
	};

	virtual operator std::string() const
	{
		Json::FastWriter Writer;
		return Writer.write(operator Json::Value());
	}

	virtual operator Json::Value() const
	{
		Json::Value Root;
		Root["type"] = Type;
		Root["content"] = Content;
		return Root;
	}

protected:
	uint8_t Type;	// Content type
	string Content; // Actual content
};

//Simple text content payload
class TextContent : public Payload
{
public:
	TextContent(string str) : Payload(str, Text) {};
};