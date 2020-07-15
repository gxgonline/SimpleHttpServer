#include "stdafx.h"
#include "Core.h"

//#define DEBUG


Core* Core::instance = nullptr;

Core::Core()
{
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	processorsNum = si.dwNumberOfProcessors;

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
	if (instance) return instance;
	return instance = new Core();
}

void Core::start() {
	processorsNum = 1; //for debug

	std::thread** t = new std::thread*[processorsNum];
	for (auto i = 0; i < processorsNum; i++) {
		t[i] = new std::thread(Core::threadproc);
	}
	// 第一个join导致堵塞，执行不到后面的join
	for (auto i = 0; i < processorsNum; i++) {
		t[i]->join();// 阻止
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