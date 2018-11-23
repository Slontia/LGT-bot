// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>



// TODO: reference additional headers your program requires here
#include "stdint.h"
#include "string"
#include <atomic>
#include <vector>
#include <map>
#include <functional>
#include <Windows.h>
#include <time.h>

/* Mysql Connector */
#include <mysql_connection.h>
#include <mysql_driver.h>

using std::string;
using std::vector;
using std::map;


/*
������Ҫ���ǵ����飺
1. ���쳣�Ĵ�����Ϸ�е��쳣Ӧ��������Ϸ����ֹ���������򲻷�������
2. Time Up�Ĵ���ĿǰAtomStateû��TimerHandle�������ˣ�����TimerHandle����ΪӦ��������ֹ��״̬����������������
3. ��ȡ��״̬��Ϣ��ʱ���޷�����ת�������Կ���ʹ��boost�������ˣ�
*/


/*
1. ��Container��������׶ν����ĺ���
���ֽ׶Σ�һ��Ϊģ�壬����Ҫ�������׶��г�Ա��һ��Ϊ��ͨ�ࡣֻ����ͨ����Գ�Ϊ���׶Σ���Ҫ��Container�а�ID��

�����������ͷŽ׶�ʱ���̰߳�ȫ��
������Ϸǰ�����Ϸ�Ϸ��ԣ��Ƿ�������׶�
*/