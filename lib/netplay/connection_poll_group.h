#pragma once

class IClientConnection;

class IConnectionPollGroup
{
public:

	virtual ~IConnectionPollGroup() = default;

	virtual int checkSockets(unsigned timeout) = 0;
	virtual void add(IClientConnection* conn) = 0;
	virtual void remove(IClientConnection* conn) = 0;
};

IConnectionPollGroup* newTCPconnectionPollGroup();
