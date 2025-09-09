// pch.h: ����Ԥ�����ͷ�ļ���
// �·��г����ļ�������һ�Σ�����˽������ɵ��������ܡ�
// �⻹��Ӱ�� IntelliSense ���ܣ�����������ɺ�������������ܡ�
// ���ǣ�����˴��г����ļ��е��κ�һ��������֮���и��£�����ȫ�����������±��롣
// �����ڴ˴����ҪƵ�����µ��ļ����⽫ʹ������������Ч��
#pragma once

#ifndef PCH_H
#define PCH_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef CLIENT_APP
#define CLIENT_APP
#endif // !CLIENT_APP

#define NOMINMAX

//std libs
#include <iostream>
#include <vector>
#include <stdlib.h>
#include <stdio.h>

//Windows libs
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

//Other libs
#include <boost/uuid.hpp>
#include <json/json.h>

namespace uuid = ::boost::uuids;
using std::string;

#pragma comment(lib, "Ws2_32.lib")

#endif //PCH_H
