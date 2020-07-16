#pragma once
#include <thread>
#include "IOCPModule.h"

#ifdef DEBUG
#include <iostream>
#endif
class Core
{
public:
	static Core* getInstance();
	Core();
	~Core();
	void start();
private:
	static Core* m_pInstance;
	static void threadproc();

	//int m_nStop = 0;
	int m_nProcessorsNum = 0;
};

