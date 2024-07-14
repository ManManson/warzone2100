#include "lib/netplay/tcp/tcp_connection_poll_group.h"
#include "lib/netplay/tcp/tcp_client_connection.h"
#include "lib/netplay/tcp/netsocket.h"
#include "lib/framework/wzapp.h"
#include "lib/framework/debug.h"

namespace tcp
{

TCPConnectionPollGroup::TCPConnectionPollGroup(SocketSet* sset)
	: sset_(sset)
{}

TCPConnectionPollGroup::~TCPConnectionPollGroup()
{
	if (sset_)
	{
		deleteSocketSet(sset_);
	}
}

int TCPConnectionPollGroup::checkSockets(unsigned timeout)
{
	return tcp::checkSockets(*sset_, timeout);
}

void TCPConnectionPollGroup::add(IClientConnection* conn)
{
	auto* tcpConn = dynamic_cast<TCPClientConnection*>(conn);
	ASSERT_OR_RETURN(, tcpConn != nullptr, "Expected to have TCPClientConnection instance");
	SocketSet_AddSocket(*sset_, tcpConn->socket_);
}

void TCPConnectionPollGroup::remove(IClientConnection* conn)
{
	auto tcpConn = dynamic_cast<TCPClientConnection*>(conn);
	ASSERT_OR_RETURN(, tcpConn != nullptr, "Expected to have TCPClientConnection instance");
	SocketSet_DelSocket(*sset_, tcpConn->socket_);
}

} // namespace tcp

IConnectionPollGroup* newTCPconnectionPollGroup()
{
	auto* sset = tcp::allocSocketSet();
	if (!sset)
	{
		return nullptr;
	}
	return new tcp::TCPConnectionPollGroup(sset);
}
