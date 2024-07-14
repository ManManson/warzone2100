#pragma once

#include <type_traits>

class IClientConnection;

/// <summary>
/// Server-side listen socket abstraction.
/// Need only `listenSocket()` and `acceptSocket()` functions.
/// </summary>
class IListenSocket
{
public:

	virtual ~IListenSocket() = default;

	enum class IPVersions : uint8_t
	{
		IPV4 = 0b00000001,
		IPV6 = 0b00000010
	};
	using IPVersionsMask = std::underlying_type_t<IPVersions>;

	// Accept incoming client connection on the current server-side listen socket
	virtual IClientConnection* accept() = 0;
	virtual IPVersionsMask supportedIpVersions() const = 0;
};

IListenSocket* openTCPListenSocket(uint16_t port);
