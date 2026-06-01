#include "pch.h"
#include "Logger.h"

Logger GetLogger(tstring name)
{
	Logger logger = log4cplus::Logger::getInstance(name);

	// Only add a ConsoleAppender once per logger (skip if already configured).
	// 防止多次添加Appender导致日志重复输出
	if (logger.getAllAppenders().size() == 0)
	{
		// Create a ConsoleAppender and set its layout
		SharedAppenderPtr appender(new log4cplus::ConsoleAppender());
		appender->setName(LOG4CPLUS_TEXT("console"));

		//set pattern: date/time, thread id, log level, logger name, message, source file and line number
		tstring pattern = LOG4CPLUS_TEXT("%D{%y/%m/%d %H:%M:%S.%q} [%t] %-5p %c - %m [%F:%L]%n");
		appender->setLayout(std::unique_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(pattern)));

#ifdef _DEBUG
		logger.setLogLevel(log4cplus::DEBUG_LOG_LEVEL);
#else
		logger.setLogLevel(log4cplus::INFO_LOG_LEVEL);
#endif
		logger.addAppender(appender);
	}

	return logger;
}

std::string strToHexString(const char* data, size_t len) {
	std::stringstream ss;
	ss << std::hex << std::setfill('0'); // 16进制，不足2位补0
	for (size_t i = 0; i < len; ++i) {
		// 转换为unsigned char避免符号位问题，再转为整数输出2位16进制
		ss << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(data[i]));
		if (i != len - 1) {
			ss << " "; // 字节间用空格分隔
		}
	}
	return ss.str(); // 返回拼接好的16进制字符串
}

