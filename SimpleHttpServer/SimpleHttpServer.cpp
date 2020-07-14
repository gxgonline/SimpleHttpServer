// SimpleHttpServer.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Svc.h"
//#define TEST

#ifdef TEST
#include "Test.h"
#else
#include "Core.h"
#endif

void threadSvc() {
	_tmainSvc(0, NULL);
}

int _tmain(int argc, _TCHAR* argv[])
{
#ifdef TEST
	return Test::TestAll();
#else
	// ���õ�ǰ·��..	
	wchar_t szFilePath[MAX_PATH + 1] = { 0 };
	GetModuleFileNameW(NULL, szFilePath, MAX_PATH);
	(wcsrchr(szFilePath, '\\'))[1] = 0; // ɾ���ļ�����ֻ���·���ִ�
	SetCurrentDirectory(szFilePath);//���õ�ǰ·��Ϊ����Ŀ¼���������ã����Է���ʽ����ʱ�����Ҳ����ļ����򲻿���

	std::thread *pTh = new std::thread(threadSvc);//����ϵͳ���񣨹���
	//pTh->join();//���²�������ִ��
	//_tmainSvc(argc, argv);
	Core* core = Core::getInstance();
	core->start();// ���� http server
	delete pTh;
	//MessageBox(NULL, _T("start"), _T("start"), MB_OK);
	return 0;
#endif
}