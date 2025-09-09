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
/// <param name="s">套接字</param>
/// <param name="port">端口号</param>
/// <param name="address">连接或绑定的地址(没有则传入NULL)</param>
/// <returns>0=成功, 1=失败</returns>
int CreateSocket(SOCKET& s, const char* port, const char* address);

//简单收发数据

/// <summary>
/// 接收数据
/// </summary>
/// <param name="s">套接字</param>
/// <param name="dataBuffer">数据缓冲区</param>
/// <returns>接收到的字节数</returns>
int Recv(SOCKET& s, char*& DataBuffer);

/// <summary>
/// 发送数据
/// </summary>
/// <param name="s">套接字</param>
/// <param name="DataBuffer">数据缓冲区</param>
/// <param name="Size">数据大小</param>
/// <returns>发送的字节数</returns>
int Send(SOCKET& s, const char* DataBuffer, int Size);

/// <summary>
/// 发送数据
/// </summary>
/// <param name="s">套接字</param>
/// <param name="DataBuffer">数据缓冲区</param>
/// <returns>发送的字节数</returns>
int Send(SOCKET& s, const char* DataBuffer);

void EndianSwap(char* pData, unsigned int startIndex, unsigned int length);