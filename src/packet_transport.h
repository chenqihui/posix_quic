#pragma once

#include "net/quic/quartc/quartc_factory.h"

namespace posix_quic {

class PacketTransport
    : public net::QuartcSessionInterface::PacketTransport
{
public:
    void Set(std::shared_ptr<int> udpSocket, QuicSocketAddress const& address);

    int Write(const char* buffer, size_t buf_len) override;

private:
    std::shared_ptr<int> udpSocket_;
    QuicSocketAddress address_;
};

} // namespace posix_quic