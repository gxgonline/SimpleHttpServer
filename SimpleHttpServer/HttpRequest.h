#pragma once

#include "StateMachine.h"
#include <string>
#include <map>
#include <functional>
#include <mutex>

class HttpRequest
{
public:
	HttpRequest(char* buff, size_t len);
	~HttpRequest();
	bool parse(char* buff, size_t len);
	std::string getMethod();
	std::string getURL();
	std::string getBody();
	std::string getVersion();
	bool isFinished();
	bool m_responsed = false;
	std::map<std::string, std::string>& getHeader();
private:
	bool m_finished = false;
	std::ostringstream m_method;
	std::ostringstream m_url;
	std::ostringstream m_version;
	std::map<std::string, std::string> m_header;
	std::ostringstream m_bodyStream;
	std::string m_currentKey;
	std::string m_currentValue;
	std::mutex m_mutex;
	int m_contentLength = 0;
	int m_contentReceived = 0;
	bool m_headerPrepared = false;
	enum httpRequestState {requestMethod, requestURL, requestVersion, headerKey, headerValue, body, finish};
	StateMachine<httpRequestState> m_httpRequestStateM;

	void prepareHeader();
};

