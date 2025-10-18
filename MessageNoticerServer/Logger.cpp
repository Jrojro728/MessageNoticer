#include "pch.h"
#include "Logger.h"

Logger GetLogger(tstring name)
{
	//第1步：建立ConsoleAppender
	SharedAppenderPtr appender(new log4cplus::ConsoleAppender());

	//第2步：设置Appender的名称和输出格式（SimpleLayout）
	appender->setName(LOG4CPLUS_TEXT("console"));

	tstring pattern = LOG4CPLUS_TEXT("%D{%y/%m/%d %H:%M:%S.%q} [%t] %-5p %c - %m [%F:%L]%n");
	appender->setLayout(std::unique_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(pattern)));

	//第3步：得到一个Logger实例，并设置其日志输出等级阈值
	Logger logger = log4cplus::Logger::getInstance(name);
#ifdef _DEBUG
	logger.setLogLevel(log4cplus::DEBUG_LOG_LEVEL);
#else
	logger.setLogLevel(log4cplus::INFO_LOG_LEVEL);
#endif // DEBUG
	//第4步：为Logger实例添加ConsoleAppender
	logger.addAppender(appender);
	return logger;
}