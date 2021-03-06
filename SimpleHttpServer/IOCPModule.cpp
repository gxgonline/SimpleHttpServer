#include "stdafx.h"
#include "IOCPModule.h"
#include "HttpModule.h"

//#define DEBUG

// Static variable initialization
// ------------------------------------------------------START-----------------------------------------------------------------------------------
std::atomic_int* IOCPModule::m_startNum = new std::atomic_int;
HANDLE IOCPModule::m_iocp = NULL;
SOCKET IOCPModule::m_listener = NULL;
LPFN_ACCEPTEX IOCPModule::m_acceptEx;
LPFN_DISCONNECTEX IOCPModule::m_disconnectEx;
LPFN_GETACCEPTEXSOCKADDRS IOCPModule::m_getAcceptexSockAddrs;

IOCPModule::EVENT_HANDLER IOCPModule::m_recvHandler =
[](IOCPModule* module, PER_IO_CONTEXT* ioContext, PER_SOCKET_CONTEXT* socketContext) {
#ifdef DEBUG
	std::cout << "Recv:" << ioContext->overlapped.InternalHigh << std::endl;
	for (unsigned int i = 0; i < ioContext->overlapped.InternalHigh; i++) {
		printf("%x ", ioContext->szBuffer[i]);
	}
#endif
	//std::cout << std::endl;
	HttpModule::Serve(module, ioContext, socketContext);
	/*if (!socketContext->postSend(module, EXAMPLE_RESPONSE, sizeof(EXAMPLE_RESPONSE)) && WSAGetLastError() != WSA_IO_PENDING) {
#ifdef DEBUG
		std::cout << "Error Code:" << WSAGetLastError() << std::endl;
#endif
	}*/
};

IOCPModule::EVENT_HANDLER IOCPModule::m_sendHandler =
[](IOCPModule* module, PER_IO_CONTEXT* ioContext, PER_SOCKET_CONTEXT* socketContext) {
	//socketContext->close(module); //TODO: KeepAlive
};
//-----------------------------------------------------------------------------------------------------------------------------------------------

// Context Utils
//--------------------------------------------------------START----------------------------------------------------------------------------------
bool IOCPModule::PER_IO_CONTEXT::postRecv(IOCPModule* module, IOCPModule::_PER_IO_CONTEXT* ioContext, IOCPModule::_PER_SOCKET_CONTEXT* socketContext) {
	if (opType != recv) return false;
	DWORD dwRecv = 0;
	DWORD flags = 0;
	auto ret = WSARecv(socketAccept, &wsaBuf, 1, &dwRecv, &flags, &overlapped, NULL);
	if (ret == 0) {
		//Immediately received
		ioContext->overlapped.InternalHigh = dwRecv;
		ev(module, ioContext, socketContext);
		return true;
	} 
	int errNo = WSAGetLastError();
	return SOCKET_ERROR != ret || errNo == WSA_IO_PENDING;//997
}

bool IOCPModule::PER_SOCKET_CONTEXT::insertIoContext(PER_IO_CONTEXT* ioContext) {
	return true; // for test //准备gxg-
	if (arrayLen < arrayCap) {
		arrayIoContext[arrayLen] = ioContext;
		arrayLen++;
	}
	else {
		arrayCap <<= 1;
		arrayIoContext = (PER_IO_CONTEXT**)realloc(arrayIoContext, arrayCap); // TODO: Optimize
		if (arrayIoContext)
			return insertIoContext(ioContext);
		else
			return false;
	}
	return true;
}

bool IOCPModule::PER_SOCKET_CONTEXT::postSend(IOCPModule* module, const char* buff, size_t len) {
	if (len > MAX_BUFFER_LEN) {
		LARGE_SEND_CONTEXT* ioContext = module->allocSendContext(this->socket, len, IOCPModule::m_sendHandler);
		memcpy(ioContext->wsaBuf.buf, buff, len); // TODO: optimize
		// TODO: insert to io operation list
		// TODO: fullize thought
		auto ret = WSASend(this->socket, &ioContext->wsaBuf, 1, NULL, 0, &ioContext->overlapped, NULL);
		if (ret == NULL) {
			ioContext->ev(module, (PER_IO_CONTEXT*)ioContext, this);
		}
		return (SOCKET_ERROR != ret || WSAGetLastError() == WSA_IO_PENDING);
	}

	// else
	PER_IO_CONTEXT* ioContext = module->allocIoContext(this->socket, send, IOCPModule::m_sendHandler);
	memcpy(ioContext->szBuffer, buff, len); // TODO: optimize
	ioContext->wsaBuf.len = (ULONG)len;
	if (!insertIoContext(ioContext))
	{
		module->freeIoContext(ioContext);
		return false;
	}

	auto ret = WSASend(this->socket, &ioContext->wsaBuf, 1, NULL, 0, &ioContext->overlapped, NULL);
	if (ret == NULL) {
		ioContext->ev(module, ioContext, this);
	}
	return (SOCKET_ERROR != ret || WSAGetLastError() == WSA_IO_PENDING);
}

bool IOCPModule::PER_SOCKET_CONTEXT::close(IOCPModule* module) {
	// TODO: Cancel all IO requests
	module->postDisconnect(socket);
	return true;
}



// IOCPModule functions

IOCPModule::IOCPModule(Mempool* mempoll)
{
	this->m_mempool = mempoll;
	if (m_listener == NULL) return;
	PER_IO_CONTEXT *ioContext = allocIoContext();
	if (ioContext == NULL) return;
	if (ioContext->socketAccept == INVALID_SOCKET) return;
	if (m_acceptEx(m_listener, ioContext->socketAccept, ioContext->wsaBuf.buf, ioContext->wsaBuf.len - 2 * (sizeof(sockaddr_in) + 16),
		sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, 0, (LPOVERLAPPED)&ioContext->overlapped) == FALSE &&
		WSAGetLastError() != WSA_IO_PENDING && WSAGetLastError() != 0) {
#ifdef DEBUG
		std::cout << "Error code:" << WSAGetLastError() << std::endl;
#endif
		closesocket(ioContext->socketAccept);
		freeIoContext(ioContext);
		return;
	}
	m_startNum++;
}


IOCPModule::~IOCPModule()
{
	if (m_startNum->load() == 0)
		WSACleanup();
}

bool IOCPModule::initialize() {
	m_iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0); // Num of CPU
	WSADATA wsaData;
	auto ret = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (ret != NULL) return false;

	struct sockaddr_in serverAddress;
	m_listener = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	if (m_listener == INVALID_SOCKET) return false;

	ZeroMemory((char*)&serverAddress, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.S_un.S_addr = inet_addr("0.0.0.0");
	serverAddress.sin_port = htons(LISTEN_PORT);

	if (SOCKET_ERROR == bind(m_listener, (struct sockaddr*)&serverAddress, sizeof(serverAddress)))
		return false;
	if (SOCKET_ERROR == listen(m_listener, SOMAXCONN))
		return false;
	if(CreateIoCompletionPort((HANDLE)m_listener, m_iocp, 0, 0) == NULL) return false;

	// get AcceptEx process pointer
	GUID guidAcceptEx = WSAID_ACCEPTEX;
	DWORD dwBytes = 0;
	WSAIoctl(m_listener, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidAcceptEx, sizeof(guidAcceptEx),
		&m_acceptEx, sizeof(m_acceptEx), &dwBytes, NULL, NULL);

	// get GetAcceptExSockAddrs process pointer
	GUID guidGetAcceptExSockAddrs = WSAID_GETACCEPTEXSOCKADDRS;
	WSAIoctl(m_listener, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidGetAcceptExSockAddrs, sizeof(guidGetAcceptExSockAddrs),
		&m_getAcceptexSockAddrs, sizeof(m_getAcceptexSockAddrs), &dwBytes, NULL, NULL);

	// get DisconnectEx process pointer
	GUID guidDisconnectEx = WSAID_DISCONNECTEX;
	WSAIoctl(m_listener, SIO_GET_EXTENSION_FUNCTION_POINTER, &guidDisconnectEx, sizeof(guidDisconnectEx),
		&m_disconnectEx, sizeof(m_disconnectEx), &dwBytes, NULL, NULL);

	m_startNum->store(0);
	return true;
}

// allocate a io context for accpetex
// this function first create a socket for future
IOCPModule::PER_IO_CONTEXT* IOCPModule::allocIoContext() {
	SOCKET acceptSocket = getReusedSocket();
	if (acceptSocket == INVALID_SOCKET) return NULL;

	return allocIoContext(acceptSocket, accept, NULL);
}

// allocate a io context for wsarecv/wsasend
// this function dosn't create new socket
IOCPModule::PER_IO_CONTEXT* IOCPModule::allocIoContext(SOCKET acceptSocket, operation opType = recv, EVENT_HANDLER ev = m_recvHandler) {
	PER_IO_CONTEXT *ioContext = (PER_IO_CONTEXT*)malloc(sizeof(PER_IO_CONTEXT));
	ZeroMemory(ioContext, sizeof(PER_IO_CONTEXT));
	ioContext->opType = opType;
	ioContext->socketAccept = acceptSocket;
	ioContext->wsaBuf.len = MAX_BUFFER_LEN;
	ioContext->wsaBuf.buf = ioContext->szBuffer;
	ioContext->ev = ev ? ev : m_recvHandler;
	return ioContext;
}

// allocate a io context for wsasend with large buffer
// this function dosn't create new socket
IOCPModule::LARGE_SEND_CONTEXT* IOCPModule::allocSendContext(SOCKET acceptSocket, size_t size, EVENT_HANDLER ev = m_sendHandler) {
	LARGE_SEND_CONTEXT *ioContext = (LARGE_SEND_CONTEXT*)m_mempool->alloc(sizeof(LARGE_SEND_CONTEXT));
	static_assert(sizeof(LARGE_SEND_CONTEXT) < 512, "largeContext is too large");
	ioContext->opType = send;
	ioContext->socketAccept = acceptSocket;
	ioContext->wsaBuf.len = (ULONG)size;
	ioContext->wsaBuf.buf = (char*)m_mempool->allocPages(size);
	ioContext->ev = ev ? ev : m_sendHandler;
	return ioContext;
}

bool IOCPModule::freeSendContext(LARGE_SEND_CONTEXT* context) {
	m_mempool->freePages(context->wsaBuf.buf);
	m_mempool->free(context);
	return true;
}

IOCPModule::PER_SOCKET_CONTEXT* IOCPModule::allocSocketContext() {
	auto ret = (PER_SOCKET_CONTEXT*)m_mempool->alloc(sizeof(PER_SOCKET_CONTEXT));
	if (ret == nullptr) {
		// Only for debug
		std::cout << "DEBUG";
	}
	ZeroMemory(ret, sizeof(PER_SOCKET_CONTEXT));
	ret->arrayLen = 0;
	ret->arrayCap = 8;
	ret->arrayIoContext = (PER_IO_CONTEXT**)malloc(sizeof(PER_IO_CONTEXT) * ret->arrayCap);
	ret->request = nullptr;
	return ret;
}

bool IOCPModule::freeSocketContext(PER_SOCKET_CONTEXT* context) {
	if (context->request) {
		delete context->request;
	}
	free(context->arrayIoContext);
	m_mempool->free(context);
	return true;
}

bool IOCPModule::freeIoContext(PER_IO_CONTEXT* context) {
	free(context);
	return true;
}

bool IOCPModule::eventLoop() {
	
	DWORD nBytes = 0;
	PER_SOCKET_CONTEXT* socketContext = nullptr;
	LPOVERLAPPED lpoverlapped;
	if (GetQueuedCompletionStatus(m_iocp, &nBytes, (PULONG_PTR)&socketContext, &lpoverlapped, INFINITE) == FALSE) {
		return true;
	};
	PER_IO_CONTEXT* context = CONTAINING_RECORD(lpoverlapped, PER_IO_CONTEXT, overlapped);
	if (nBytes == 0 || nBytes == -1)//gxg+
	{
		if (recv == context->opType || send == context->opType)//仅对recv和send进行返回处理，accept时若返回则再也无法连接请求了。
		{
			// 释放掉对应的资源
			if (context->wsaBuf.len > MAX_BUFFER_LEN)
				freeSendContext((LARGE_SEND_CONTEXT*)context);
			else
				freeIoContext(context);		
			context = nullptr;
			return true;
		}
	}
	DISCONNECT_CONTEXT* disconnectContex;
	switch (context->opType) {
	case accept:
		return doAcceptEx(context);
	case recv:
		if (socketContext == NULL) { 
#ifdef DEBUG
			std::cout << "why socketContext == NULL? some recv requests error?" << std::endl;
#endif
			return false;
		}
		return doRecv(context, socketContext);
	case send:
		if (socketContext == NULL) {
#ifdef DEBUG
		std::cout << "why socketContext == NULL? some send requests error?" << std::endl;
#endif
		return false;
	}
#ifdef DEBUG
		std::cout << "sended" << std::endl;
#endif
		// TODO: optimize
		context->ev(this, context, socketContext);
		if (context->wsaBuf.len > MAX_BUFFER_LEN)
			freeSendContext((LARGE_SEND_CONTEXT*) context);
		else
			freeIoContext(context);
		return true;
	case disconnect:
#ifdef DEBUG
		std::cout << "disconnected" << std::endl;
#endif
		disconnectContex = (DISCONNECT_CONTEXT*)context;
		if (!doDisconnect(disconnectContex->socket)) {
			//TODO
		}
		m_mempool->free(disconnectContex);
		return true;
	default:
		return false;
	}
	
	return true;
}

// post a acceptex request to iocp
// this function will create a new socket for future
bool IOCPModule::postAcceptEx(PER_IO_CONTEXT* context) {
	SOCKET recvSocket = getReusedSocket();
	if (recvSocket == INVALID_SOCKET) return false;
	context->opType = accept;
	context->socketAccept = recvSocket;

	if (m_acceptEx(m_listener, context->socketAccept, context->wsaBuf.buf, context->wsaBuf.len - 2 * (sizeof(sockaddr_in) + 16),
		sizeof(sockaddr_in) + 16, sizeof(sockaddr_in) + 16, 0, (LPOVERLAPPED)&context->overlapped) == FALSE &&
		WSAGetLastError() != WSA_IO_PENDING && WSAGetLastError() != 0) {
		closesocket(context->socketAccept); // Tag
		return false;
	}	
	return true;
}

bool IOCPModule::doAcceptEx(PER_IO_CONTEXT* context) {
	// get client addr
	SOCKADDR_IN *clientAddr = NULL, *localAddr = NULL;
	int remoteLen  = sizeof(SOCKADDR_IN);
	int localLen = remoteLen;
	m_getAcceptexSockAddrs(context->wsaBuf.buf, context->wsaBuf.len - ((sizeof(SOCKADDR_IN) + 16) * 2),
		sizeof(SOCKADDR_IN) + 16, sizeof(SOCKADDR_IN) + 16, (LPSOCKADDR*)&localAddr, &localLen, (LPSOCKADDR*)&clientAddr, &remoteLen);
#ifdef DEBUG
	std::cout << "Client:  " << inet_ntoa(clientAddr->sin_addr) << ":" << ntohs(clientAddr->sin_port) << "   has been accepted." << std::endl;
#endif
	SOCKET recvSocket = context->socketAccept;

	
	// allocate new ioContext and socketContext
	PER_IO_CONTEXT* recvIoContext = allocIoContext(recvSocket);
	if (recvIoContext == NULL) return false;
	PER_SOCKET_CONTEXT* recvSocketContext = allocSocketContext();
	if (recvSocketContext == NULL) {
		freeIoContext(recvIoContext);
		return false;
	}
	

	memcpy(&recvSocketContext->clientAddr, clientAddr, sizeof(SOCKADDR_IN));
	recvSocketContext->socket = recvSocket;

	
	// bind new request to iocp
	if (CreateIoCompletionPort((HANDLE)recvIoContext->socketAccept, IOCPModule::m_iocp, (ULONG_PTR)recvSocketContext, 0) == NULL
		&& WSAGetLastError() != ERROR_INVALID_PARAMETER) {
		// About the 87 error param error:
		// Binding a reused socket to iocp may cause 87 error
		// TODO: fix it?
		closesocket(recvIoContext->socketAccept);
		freeIoContext(recvIoContext);
		freeSocketContext(recvSocketContext);
		return false;
	}

	// call recv handler
	recvIoContext->ev(this, context, recvSocketContext);

	// post a recv request to iocp
	if (postRecv(recvIoContext, recvSocketContext) == false) {
		freeIoContext(recvIoContext);
		freeSocketContext(recvSocketContext);
		return false;
	}


	// post next accept request to listener
	if (postAcceptEx(context) == false) {
		freeIoContext(context);
		return false;
	}

	return true;
}

bool IOCPModule::doRecv(PER_IO_CONTEXT* ioContext, PER_SOCKET_CONTEXT* socketContext) {
	ioContext->ev(this, ioContext, socketContext); 

	// post a recv request to iocp
	if (postRecv(ioContext, socketContext) == false) {
		freeIoContext(ioContext);
		freeSocketContext(socketContext);
		return false;
	}

	return true;
}

bool IOCPModule::postRecv(PER_IO_CONTEXT* ioContext, PER_SOCKET_CONTEXT* socketContext) {
	return ioContext->postRecv(this, ioContext, socketContext);
}

SOCKET IOCPModule::getReusedSocket() {
	if (m_reusedSocketQueue.empty()) {
		return socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	}
	SOCKET ret = m_reusedSocketQueue.front();
	m_reusedSocketQueue.pop();
	return ret;
}

bool IOCPModule::freeReusedSocket(SOCKET s) {
	m_reusedSocketQueue.push(s);
	return true;
}

bool IOCPModule::postDisconnect(SOCKET s) {
	DISCONNECT_CONTEXT* context = (DISCONNECT_CONTEXT*)m_mempool->alloc(sizeof(DISCONNECT_CONTEXT));
	context->socket = s;
	context->opType = disconnect;
	if (m_disconnectEx(s, &context->overlapped, TF_REUSE_SOCKET, NULL) == FALSE && WSAGetLastError() != WSA_IO_PENDING)
		return false;
	return true;
}

bool IOCPModule::doDisconnect(SOCKET s) {
	return freeReusedSocket(s);
}