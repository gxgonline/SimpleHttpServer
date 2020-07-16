#include "stdafx.h"
#include "Core.h"

//#define DEBUG


Core* Core::m_pInstance = nullptr;

Core::Core()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	m_nProcessorsNum = si.dwNumberOfProcessors;

	if (!IOCPModule::initialize()) {
#ifdef DEBUG
		std::cout << "IOCP Module initialize failed!" << std::endl;
#endif
		exit(-1);
	}
}


Core::~Core()
{
}

Core* Core::getInstance() {
	if (m_pInstance) return m_pInstance;
	return m_pInstance = new Core();
}

void Core::start() {
	m_nProcessorsNum = 2; //for debug

	std::thread** t = new std::thread*[m_nProcessorsNum];
	for (auto i = 0; i < m_nProcessorsNum; i++) {
		t[i] = new std::thread(Core::threadproc);
	}
	// ��һ��join���¶�����ִ�в��������join
	for (auto i = 0; i < m_nProcessorsNum; i++) {
		t[i]->join();// ��ֹ
	}
}

void Core::threadproc() {
#ifdef DEBUG
	std::cout << "Worker thread start..." << std::endl;
#endif
	IOCPModule *iocpModule = new IOCPModule(new Mempool());
	for (; ;) {
		if (!iocpModule->eventLoop()) {
			std::cout << "debug" << std::endl;
		}
	}
	delete iocpModule;
#ifdef DEBUG
	std::cout << "I'm exploding..." << std::endl;
#endif
}