#include "stdafx.h"
#include "HttpRequest.h"


HttpRequest::HttpRequest(char* buf, size_t len) // TODO: maybe bug
{
	auto requestMethodHandler = [this](httpRequestState start, std::stringstream& currentStream) -> httpRequestState {
		char c = currentStream.get();
		while (!currentStream.eof()) {
			if (c == ' ') return requestURL;
			m_method.put(c);
			c = currentStream.get();
		}
		return requestMethod;
	};

	m_httpRequestStateM.addStep(requestMethod, requestMethodHandler);

	m_httpRequestStateM.addStep(requestURL, [this](httpRequestState start, std::stringstream& currentStream) -> httpRequestState {
		char c = currentStream.get();
		while (!currentStream.eof()) {
			if (c == ' ') return requestVersion;
			m_url.put(c);
			c = currentStream.get();
		}
		return requestURL;
	});

	m_httpRequestStateM.addStep(requestVersion, [this](httpRequestState start, std::stringstream& currentStream) -> httpRequestState {
		char c = currentStream.get();
		while (!currentStream.eof()) {
			if (c == '\r') {
				c = currentStream.get();
				if (currentStream.eof()) {
					currentStream.putback('\r');
					return requestVersion;
				}
				if (c == '\n')
					return headerKey;
				else
				{
					currentStream.putback(c);
					c = '\r';
				}
			}
			m_version.put(c);
			c = currentStream.get();
		}
		return requestVersion;
	});

	m_httpRequestStateM.addStep(headerKey, [this](httpRequestState start, std::stringstream& currentStream) -> httpRequestState {
		char c = currentStream.get();
		while (!currentStream.eof()) {
			if (c == ':') {
				m_currentValue = "";
				return headerValue;
			}
			m_currentKey += c;
			c = currentStream.get();
		}
		return headerKey;
	});

	m_httpRequestStateM.addStep(headerValue, [this](httpRequestState start, std::stringstream& currentStream) -> httpRequestState {
		char c = currentStream.get();
		while (!currentStream.eof()) {
			if (c == '\r') {
				c = currentStream.get();
				if (currentStream.eof()) {
					currentStream.putback('\r');
					return headerValue;
				}
				if (c == '\n') {
					c = currentStream.get();
					if (currentStream.eof()) {
						m_header[m_currentKey] = m_currentValue;
						return headerValue;
					}
					if (c == '\r') {
						c = currentStream.get();
						if (currentStream.eof()) {
							currentStream.putback('\r');
							return headerValue;
						}
						if (c == '\n') {
							m_header[m_currentKey] = m_currentValue;
							return body;
						}
						currentStream.putback(c);
						currentStream.putback('\r');
						m_currentKey == "";
						return headerKey;
					}
					currentStream.putback(c);
					m_header[m_currentKey] = m_currentValue;
					m_currentKey = "";
					return headerKey;
				}
				else
				{
					currentStream.putback(c);
					c = '\r';
				}
			}
			m_currentValue += c;
			c = currentStream.get();
		}
		return headerValue;
	});

	m_httpRequestStateM.addStep(body, [this](httpRequestState start, std::stringstream& currentStream) -> httpRequestState {
		prepareHeader();
		char c = currentStream.get();
		while (!currentStream.eof() && c != 0) { // c == 0 -> wait for next request(body)
			m_bodyStream.put(c);
			m_contentReceived++;
			if (m_contentReceived >= m_contentLength && m_contentLength != 0) {
				m_finished = true;
				return finish;
			}
			c = currentStream.get();
		}
		if (m_contentLength != 0)
			return body;
		else
		{
			m_finished = true;
			return finish;
		}
	});

	m_httpRequestStateM.addStep(finish, [this](httpRequestState start, std::stringstream& currentStream) -> httpRequestState {
		return finish;
	});

	m_finished = parse(buf, len);
}


HttpRequest::~HttpRequest()
{
}

bool HttpRequest::parse(char* buf, size_t len) {
	std::lock_guard<std::mutex> lock(m_mutex);
	if (m_finished) return true;
	std::string buffer(buf, len);
	m_httpRequestStateM.produce(buffer);
	return m_finished;
}

void HttpRequest::prepareHeader() {
	if (m_headerPrepared) return;
	for (auto& h : m_header) {
		/*
		std::string upper;
		for (auto c : h.first) {
			if (c >= 'a' && c <= 'z')
				upper += c + 'A' - 'a';
			else
				upper += c;
		}
		header[upper] = h.second;
		*/
		if (h.first == "Content-Length") {
			m_contentLength = std::atoi(h.second.c_str());
		}
	}
	m_headerPrepared = true;
}

std::string HttpRequest::getMethod() {
	return m_method.str();
}

std::string HttpRequest::getURL() {
	return m_url.str();
}

std::string HttpRequest::getBody() {
	return m_bodyStream.str();
}

std::string HttpRequest::getVersion() {
	return m_version.str();
}

bool HttpRequest::isFinished() {
	return m_finished;
}

std::map<std::string, std::string>& HttpRequest::getHeader() {
	return m_header;
}