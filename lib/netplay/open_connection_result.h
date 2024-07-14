#pragma once

#include <string>
#include <memory>
#include <functional>

class IClientConnection;

// Higher-level functions for opening a connection / socket
struct OpenConnectionResult
{
public:
	OpenConnectionResult(int error, std::string errorString)
		: error(error)
		, errorString(errorString)
	{ }

	OpenConnectionResult(IClientConnection* open_socket)
		: open_socket(open_socket)
	{ }
public:
	bool hasError() const { return error != 0; }
public:
	OpenConnectionResult(const OpenConnectionResult& other) = delete; // non construction-copyable
	OpenConnectionResult& operator=(const OpenConnectionResult&) = delete; // non copyable
	OpenConnectionResult(OpenConnectionResult&&) = default;
	OpenConnectionResult& operator=(OpenConnectionResult&&) = default;
public:

	std::unique_ptr<IClientConnection> open_socket;
	int error = 0;
	std::string errorString;
};
typedef std::function<void(OpenConnectionResult&& result)> OpenConnectionToHostResultCallback;
