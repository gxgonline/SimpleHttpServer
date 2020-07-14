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
	// 设置当前路径..	
	wchar_t szFilePath[MAX_PATH + 1] = { 0 };
	GetModuleFileNameW(NULL, szFilePath, MAX_PATH);
	(wcsrchr(szFilePath, '\\'))[1] = 0; // 删除文件名，只获得路径字串
	SetCurrentDirectory(szFilePath);//设置当前路径为程序目录，若不设置，当以服务方式启动时，则找不到文件，打不开。

	std::thread *pTh = new std::thread(threadSvc);//启动系统服务（管理）
	//pTh->join();//导致不能往下执行
	//_tmainSvc(argc, argv);
	Core* core = Core::getInstance();
	core->start();// 开启 http server
	delete pTh;
	//MessageBox(NULL, _T("start"), _T("start"), MB_OK);
	return 0;
#endif
}