#include "stdafx.h"
#include "HttpModule.h"

#define SUCCESS_RESPONSE "<html><body>Success</body></html>"
#define FAIL_RESPONSE "<html><body>Fail</body></html>"

HttpModule::HttpModule()
{
}


HttpModule::~HttpModule()
{
}

void HttpModule::Serve(IOCPModule* module, IOCPModule::PER_IO_CONTEXT* ioContext, IOCPModule::PER_SOCKET_CONTEXT* socketContext) {
	HttpRequest *request;
	if (socketContext->request) {

		request = socketContext->request;
		if (socketContext->request->responsed) {
			delete request;
			request = new HttpRequest(ioContext->wsaBuf.buf, ioContext->overlapped.InternalHigh);
			socketContext->request = request;
		} else 
			request->parse(ioContext->wsaBuf.buf, ioContext->overlapped.InternalHigh);
	}
	else {
		request = new HttpRequest(ioContext->wsaBuf.buf, ioContext->overlapped.InternalHigh);
		socketContext->request = request;
	}
	if (!request->isFinished()) return;
	std::string method = request->getMethod();

	if (method == "GET") {
		auto url = request->getURL();
		if (url == "/" || url == "/index.html") {
			//SendFile(module, ioContext, socketContext, request, "./www/index.html", "text/html");
			SendFile(module, ioContext, socketContext, request, "./www/index.html", "text/html");
		}
		else if (url == "/plain.txt") {
			SendFile(module, ioContext, socketContext, request, "./www/plain.txt", "text/plain");
		}
		else if (url == "/img.html") {
			SendFile(module, ioContext, socketContext, request, "./www/img.html", "text/html");
		}
		else if (url == "/1.png") {
			SendFile(module, ioContext, socketContext, request, "./www/1.png", "image/png");
		}
		else {
			if (!SendFile(module, ioContext, socketContext, request, "./www" + url, "text/html"))
			{
				SendFile(module, ioContext, socketContext, request, "./www/404.html", "text/html");
			}			
			//std::string notFound = request->getVersion() + " " + "404 Not Found\r\n";
			//socketContext->postSend(module, notFound.c_str(), notFound.length());
		}
	}
	else if (method == "POST") {
		std::map<std::string, std::vector<std::string>> formData;
		FormDataDecode(formData, request->getBody());
		std::ostringstream out;
		out << request->getVersion() << " 200 OK\r\n" << "Content-Type: text/html\r\n" << "Server: InsHttpBeta 17.01\r\n";

		auto url = request->getURL();
		if (url == "/post") {
			if (formData["login"].size() > 0 && formData["login"][0] == "19784420" &&
				formData["pass"].size() > 0 && formData["pass"][0] == "4420") {
				out << "Content-Length:" << sizeof(SUCCESS_RESPONSE) - 1 << "\r\n\r\n" << SUCCESS_RESPONSE;
				SendFile(module, ioContext, socketContext, request, "./www/hello.html", "text/html");
			}
			else {
				out << "Content-Length:" << sizeof(FAIL_RESPONSE) - 1 << "\r\n\r\n" << FAIL_RESPONSE;
				auto outStr = out.str();
				socketContext->postSend(module, outStr.c_str(), outStr.length());
			}
		}
		else
		{
			out << "Content-Length:" << sizeof(FAIL_RESPONSE) - 1 << "\r\n\r\n" << FAIL_RESPONSE;
			auto outStr = out.str();
			socketContext->postSend(module, outStr.c_str(), outStr.length());
		}
	}
	socketContext->request->responsed = true;
}

bool HttpModule::SendFile(IOCPModule* module, IOCPModule::PER_IO_CONTEXT* ioContext, IOCPModule::PER_SOCKET_CONTEXT* socketContext,
	HttpRequest* request, std::string filePath, std::string contentType) {

	HANDLE fp = CreateFileA(filePath.c_str(), GENERIC_READ | GENERIC_WRITE,	FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);

	//if (fp == NULL)
	if (fp == INVALID_HANDLE_VALUE)
	{
		std::string notFound = request->getVersion() + " " + "404 Not Found.We Found love 1.\r\n";
		socketContext->postSend(module, notFound.c_str(), notFound.length());
		return false;
	}
	LARGE_INTEGER lpFileSize;
	if (!GetFileSizeEx(fp, &lpFileSize))
	{
		std::string notFound = request->getVersion() + " " + "404 Not Found.We Found love 2.\r\n";
		socketContext->postSend(module, notFound.c_str(), notFound.length());
		CloseHandle(fp);
		return false;
	}
	DWORD dwBytesInBlock = lpFileSize.LowPart;//GetFileSize(fp, NULL);


	//HANDLE hFileMapping = CreateFileMapping(fp, NULL, PAGE_READWRITE, (DWORD)(dwBytesInBlock >> 16), (DWORD)(dwBytesInBlock & 0x0000FFFF), NULL);
	//HANDLE hFileMapping = CreateFileMapping(fp, NULL, PAGE_READWRITE, (DWORD)(dwBytesInBlock >> 32), (DWORD)(dwBytesInBlock & 0xFFFFFFFF), NULL);
	HANDLE hFileMapping = CreateFileMapping(fp, NULL, PAGE_READWRITE, lpFileSize.HighPart, lpFileSize.LowPart, NULL);
	
	int dwError = GetLastError();
	CloseHandle(fp);

	if (hFileMapping == NULL)
	{
		std::string notFound = request->getVersion() + " " + "404 Not Found.We Found love 3.\r\n";
		socketContext->postSend(module, notFound.c_str(), notFound.length());
		return false;
	}

	__int64 qwFileOffset = 0;

	LPVOID pbFile = MapViewOfFile(hFileMapping, FILE_MAP_ALL_ACCESS, (DWORD)(qwFileOffset >> 32), (DWORD)(qwFileOffset & 0xFFFFFFFF), dwBytesInBlock);

	if (pbFile == NULL)
	{
		std::string notFound = request->getVersion() + " " + "404 Not Found.We Found love 4.\r\n";
		socketContext->postSend(module, notFound.c_str(), notFound.length());
		return false;
	}

	std::ostringstream out;
	out << request->getVersion() << " 200 OK\r\nContent-Length:" << dwBytesInBlock << "\r\n"
		<< "Server:" << "InsHttpBeta 17.01\r\n"
		<< "Content-Type:" << contentType << "\r\n" << "\r\n";
	auto outStr = out.str();
	size_t nOutStrLen = outStr.length();
	char* buff = (char*)malloc(nOutStrLen + dwBytesInBlock + 1); //TODO: Optimize
	memcpy(buff, outStr.c_str(), nOutStrLen);
	memcpy(buff + nOutStrLen, pbFile, dwBytesInBlock);
	buff[nOutStrLen + dwBytesInBlock] = '\0';

	//FILE *fp2 = NULL;
	//fopen_s(&fp2, "K:\\Webs\\SimpleHttpServer\\SimpleHttpServer\\www\\indexTest.html", "wb");
	//fwrite(buff, 1, nOutStrLen + dwBytesInBlock, fp2);
	//fclose(fp2);

	UnmapViewOfFile(pbFile);
	CloseHandle(hFileMapping);

	if (!socketContext->postSend(module, buff, nOutStrLen + dwBytesInBlock))
	{
		//MessageBox(NULL, _T("postSend"), _T("postSend fail"), MB_OK);
	}
	free(buff);
	return true;
}

// TODO: support url-encode/url-decode
void HttpModule::FormDataDecode(std::map<std::string, std::vector<std::string>>& formData, std::string source) {
	std::istringstream in(source);
	std::string currentKey;
	std::string currentValue;
	bool parsingKey = true;
	char c = in.get();
	for (; !in.eof(); c = in.get()) {
		if (parsingKey) {
			if (c == '=') {
				parsingKey = false;
				currentValue = "";
				continue;
			}
			currentKey += c;
		}
		else {
			if (c == '&') {
				if (formData[currentKey].size() == 0) {
					formData[currentKey] = std::vector < std::string > {currentValue};
				}
				else {
					auto v = formData[currentKey];
					v.push_back(currentValue);
					formData[currentKey] = v;
				}
				currentKey = "";
				parsingKey = true;
				continue;
			}
			currentValue += c;
		}
	}
	if (formData[currentKey].size() == 0) {
		formData[currentKey] = std::vector < std::string > {currentValue};
	}
	else {
		auto v = formData[currentKey];
		v.push_back(currentValue);
		formData[currentKey] = v;
	}
}