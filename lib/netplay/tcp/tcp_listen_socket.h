#pragma once

#include <stdint.h>

#include "lib/netplay/listen_socket.h"

namespace tcp
{

struct Socket;

class TCPListenSocket : public IListenSocket
{
public:

	TCPListenSocket(tcp::Socket* rawSocket);
	virtual ~TCPListenSocket() override;

	virtual IClientConnection* accept() override;
	virtual IPVersionsMask supportedIpVersions() const override;

private:

	tcp::Socket* listenSocket_;
};

} // namespace tcp
