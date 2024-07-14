#include "lib/netplay/tcp/tcp_listen_socket.h"
#include "lib/netplay/tcp/tcp_client_connection.h"
#include "lib/netplay/tcp/netsocket.h"
#include "lib/framework/wzapp.h"
#include "lib/framework/debug.h"

namespace tcp
{

TCPListenSocket::TCPListenSocket(Socket* rawSocket)
	: listenSocket_(rawSocket)
{}

TCPListenSocket::~TCPListenSocket()
{
	if (listenSocket_)
	{
		socketClose(listenSocket_);
	}
}

IClientConnection* TCPListenSocket::accept()
{
	ASSERT(listenSocket_ != nullptr, "Internal socket handle shouldn't be null!");
	if (!listenSocket_)
	{
		return nullptr;
	}
	auto* s = socketAccept(listenSocket_);
	if (!s)
	{
		return nullptr;
	}
	return new TCPClientConnection(s);
}

IListenSocket::IPVersionsMask TCPListenSocket::supportedIpVersions() const
{
	IPVersionsMask resMask = 0;
	if (socketHasIPv4(*listenSocket_))
	{
		resMask |= static_cast<IPVersionsMask>(IPVersions::IPV4);
	}
	if (socketHasIPv6(*listenSocket_))
	{
		resMask |= static_cast<IPVersionsMask>(IPVersions::IPV6);
	}
	return resMask;
}

} // namespace tcp

IListenSocket* openTCPListenSocket(uint16_t port)
{
	tcp::Socket* s = tcp::socketListen(port);
	if (s == nullptr)
	{
		return nullptr;
	}
	return new tcp::TCPListenSocket(s);
}
