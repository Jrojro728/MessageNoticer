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
/// <returns>0=�ɹ�, 1=ʧ��</returns>
int CreateSocket(SOCKET& s, const char* port);

/// <summary>
/// ���׽�������Ϊ����״̬
/// </summary>
/// <returns>0=�ɹ�, 1=ʧ��</returns>
int ListenSocket(SOCKET& sListen);

//���շ�����

/// <summary>
/// ��������
/// </summary>
/// <param name="s">�׽���</param>
/// <param name="dataBuffer">���ݻ�����</param>
/// <returns>���յ����ֽ���</returns>
int Recv(SOCKET& s, char *& DataBuffer);

/// <summary>
/// ��������
/// </summary>
/// <param name="s">�׽���</param>
/// <param name="dataBuffer">���ݻ�����</param>
/// <returns>���͵��ֽ���</returns>
int Send(SOCKET& s, const char* DataBuffer);
