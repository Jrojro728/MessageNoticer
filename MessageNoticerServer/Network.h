#pragma once
#include "pch.h"

//初始化及设置

/// <summary>
/// 初始化网络库
/// </summary>
/// <returns>0=成功, 1=失败</returns>
int InitNetwork();

/// <summary>
/// 创建套接字
/// </summary>
/// <returns>0=成功, 1=失败</returns>
int CreateSocket(SOCKET& s, const char* port);

/// <summary>
/// 将套接字设置为监听状态
/// </summary>
/// <returns>0=成功, 1=失败</returns>
int ListenSocket(SOCKET& sListen);

//简单收发数据

/// <summary>
/// 接收数据
/// </summary>
/// <param name="s">套接字</param>
/// <param name="dataBuffer">数据缓冲区</param>
/// <returns>接收到的字节数</returns>
int Recv(SOCKET& s, char *& DataBuffer);

/// <summary>
/// 发送数据
/// </summary>
/// <param name="s">套接字</param>
/// <param name="dataBuffer">数据缓冲区</param>
/// <returns>发送的字节数</returns>
int Send(SOCKET& s, const char* DataBuffer);
