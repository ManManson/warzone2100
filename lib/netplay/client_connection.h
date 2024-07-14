#pragma once

#include <string>
#include <stddef.h>

#include "lib/framework/wzglobal.h"
#include "lib/framework/types.h"
#include "lib/netplay/open_connection_result.h"

#ifdef WZ_OS_WIN
# include <ws2def.h>
#else
# include <netdb.h>
#endif

typedef struct addrinfo SocketAddress;

class IClientConnection
{
public:

	virtual ~IClientConnection() = default;

	virtual ssize_t readAll(void* buf, size_t size, unsigned timeout) = 0;
	virtual ssize_t readNoInt(void* buf, size_t max_size, size_t* rawByteCount) = 0;
	virtual ssize_t writeAll(const void* buf, size_t size, size_t* rawByteCount) = 0;
	virtual bool readReady() const = 0;
	virtual void flush(size_t* rawByteCount) = 0;
	virtual bool readDisconnected() const = 0;
	virtual void enableCompression() = 0;
	virtual void useNagleAlgorithm(bool enable) = 0;
	virtual std::string textAddress() const = 0;
};

IClientConnection* TCPconnectionOpenAny(const SocketAddress* addr, unsigned timeout);
bool socketOpenTCPConnectionAsync(const std::string& host, uint32_t port, OpenConnectionToHostResultCallback callback);
