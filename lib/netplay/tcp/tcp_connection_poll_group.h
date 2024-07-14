#pragma once

#include "lib/netplay/connection_poll_group.h"

namespace tcp
{

struct SocketSet;

class TCPConnectionPollGroup : public IConnectionPollGroup
{
public:

	TCPConnectionPollGroup(SocketSet* sset);
	virtual ~TCPConnectionPollGroup() override;

	virtual int checkSockets(unsigned timeout) override;
	virtual void add(IClientConnection* conn) override;
	virtual void remove(IClientConnection* conn) override;

private:

	SocketSet* sset_;
};

} // namespace tcp
