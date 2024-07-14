#include "lib/netplay/tcp/tcp_client_connection.h"
#include "lib/netplay/tcp/netsocket.h"
#include "lib/framework/wzapp.h"
#include "lib/framework/debug.h"
#include "lib/framework/string_ext.h"

namespace tcp
{

TCPClientConnection::TCPClientConnection(Socket* rawSocket)
	: socket_(rawSocket)
{
	ASSERT(socket_ != nullptr, "Null socket passed to TCPClientConnection ctor");
}

TCPClientConnection::~TCPClientConnection()
{
	if (socket_)
	{
		socketClose(socket_);
	}
}

ssize_t TCPClientConnection::readAll(void* buf, size_t size, unsigned timeout)
{
	return tcp::readAll(*socket_, buf, size, timeout);
}
ssize_t TCPClientConnection::readNoInt(void* buf, size_t maxSize, size_t* rawByteCount)
{
	return tcp::readNoInt(*socket_, buf, maxSize, rawByteCount);
}

ssize_t TCPClientConnection::writeAll(const void* buf, size_t size, size_t* rawByteCount)
{
	return tcp::writeAll(*socket_, buf, size, rawByteCount);
}

bool TCPClientConnection::readReady() const
{
	return socketReadReady(*socket_);
}

void TCPClientConnection::flush(size_t* rawByteCount)
{
	socketFlush(*socket_, std::numeric_limits<uint8_t>::max()/*unused*/, rawByteCount);
}

bool TCPClientConnection::readDisconnected() const
{
	return socketReadDisconnected(*socket_);
}

void TCPClientConnection::enableCompression()
{
	socketBeginCompression(*socket_);
}

void TCPClientConnection::useNagleAlgorithm(bool enable)
{
	socketSetTCPNoDelay(*socket_, !enable);
}

std::string TCPClientConnection::textAddress() const
{
	return getSocketTextAddress(*socket_);
}

} // namespace tcp

IClientConnection* TCPconnectionOpenAny(const SocketAddress* addr, unsigned timeout)
{
	auto* s = tcp::socketOpenAny(addr, timeout);
	if (!s)
	{
		return nullptr;
	}
	return new tcp::TCPClientConnection(s);
}

static OpenConnectionResult socketOpenTCPConnectionSync(const char* host, uint32_t port)
{
	SocketAddress* hosts = tcp::resolveHost(host, port);
	if (hosts == nullptr)
	{
		int sErr = tcp::getSockErr();
		return OpenConnectionResult((sErr != 0) ? sErr : -1, astringf("Cannot resolve host \"%s\": [%d]: %s", host, sErr, tcp::strSockError(sErr)));
	}

	IClientConnection* client_transient_socket = TCPconnectionOpenAny(hosts, 15000);
	int sockOpenErr = tcp::getSockErr();
	tcp::deleteSocketAddress(hosts);

	if (client_transient_socket == nullptr)
	{
		return OpenConnectionResult((sockOpenErr != 0) ? sockOpenErr : -1, astringf("Cannot connect to [%s]:%d, [%d]:%s", host, port, sockOpenErr, tcp::strSockError(sockOpenErr)));
	}

	return OpenConnectionResult(client_transient_socket);
}

struct OpenConnectionRequest
{
	std::string host;
	uint32_t port = 0;
	OpenConnectionToHostResultCallback callback;
};

static int openDirectTCPConnectionAsyncImpl(void* data)
{
	OpenConnectionRequest* pRequestInfo = (OpenConnectionRequest*)data;
	if (!pRequestInfo)
	{
		return 1;
	}

	pRequestInfo->callback(socketOpenTCPConnectionSync(pRequestInfo->host.c_str(), pRequestInfo->port));
	delete pRequestInfo;
	return 0;
}

bool socketOpenTCPConnectionAsync(const std::string& host, uint32_t port, OpenConnectionToHostResultCallback callback)
{
	// spawn background thread to handle this
	auto pRequest = new OpenConnectionRequest();
	pRequest->host = host;
	pRequest->port = port;
	pRequest->callback = callback;

	WZ_THREAD* pOpenConnectionThread = wzThreadCreate(openDirectTCPConnectionAsyncImpl, pRequest);
	if (pOpenConnectionThread == nullptr)
	{
		debug(LOG_ERROR, "Failed to create thread for opening connection");
		delete pRequest;
		return false;
	}

	wzThreadDetach(pOpenConnectionThread);
	// the thread handles deleting pRequest
	pOpenConnectionThread = nullptr;

	return true;
}
