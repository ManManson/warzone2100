#pragma once

#include "lib/netplay/client_connection.h"
#include "lib/netplay/open_connection_result.h"

namespace tcp
{

struct Socket;

class TCPClientConnection : public IClientConnection
{
public:

	TCPClientConnection(Socket* rawSocket);
	virtual ~TCPClientConnection() override;

	virtual ssize_t readAll(void* buf, size_t size, unsigned timeout) override;
	virtual ssize_t readNoInt(void* buf, size_t maxSize, size_t* rawByteCount) override;
	virtual ssize_t writeAll(const void* buf, size_t size, size_t* rawByteCount) override;
	virtual bool readReady() const override;
	virtual void flush(size_t* rawByteCount) override;
	virtual bool readDisconnected() const override;
	virtual void enableCompression() override;
	virtual void useNagleAlgorithm(bool enable) override;
	virtual std::string textAddress() const override;

private:

	Socket* socket_;

	friend class TCPConnectionPollGroup;
};

} // namespace tcp
