#pragma once
#include "pch.h"

//��ʼ��������

/// <summary>
/// ��ʼ�������
/// </summary>
/// <returns>0=�ɹ�, 1=ʧ��</returns>
int InitNetwork();

/// <summary>
/// �����׽���
/// </summary>
/// <param name="s">�׽���</param>
/// <param name="port">�˿ں�</param>
/// <param name="address">���ӻ�󶨵ĵ�ַ(û������NULL)</param>
/// <returns>0=�ɹ�, 1=ʧ��</returns>
int CreateSocket(SOCKET& s, const char* port, const char* address);

//���շ�����

/// <summary>
/// ��������
/// </summary>
/// <param name="s">�׽���</param>
/// <param name="dataBuffer">���ݻ�����</param>
/// <returns>���յ����ֽ���</returns>
int Recv(SOCKET& s, char*& DataBuffer);

/// <summary>
/// ��������
/// </summary>
/// <param name="s">�׽���</param>
/// <param name="DataBuffer">���ݻ�����</param>
/// <param name="Size">���ݴ�С</param>
/// <returns>���͵��ֽ���</returns>
int Send(SOCKET& s, const char* DataBuffer, int Size);

/// <summary>
/// ��������
/// </summary>
/// <param name="s">�׽���</param>
/// <param name="DataBuffer">���ݻ�����</param>
/// <returns>���͵��ֽ���</returns>
int Send(SOCKET& s, const char* DataBuffer);

void EndianSwap(char* pData, unsigned int startIndex, unsigned int length);