#pragma once

#include <string.h>

namespace posix_quic {

enum eQuicSocketOptionType
{
    // 需要对端回复的时候, N秒没有收到回复或任何有效包, 即认为链接断开
    sockopt_ack_timeout_secs,

    // 链路空闲超时, 由于客户端会发心跳, 这个可以用于server端检测链接是否断开
    sockopt_idle_timeout_secs,

    // QuicStream的发送缓冲区大小 (实际使用可能超过这个值, 但超过的量不会多于一个包)
    // 默认值 5M, 定义于libquic中.
    sockopt_stream_wmem,

    // 底层udp socket的收发缓冲区大小 (默认设置为5M)
    // 设置这个的时候要注意, server端的连接都是共享同一个udp socket的.
    sockopt_udp_rmem,
    sockopt_udp_wmem,

    sockopt_count,
};

template <int Count>
class Options
{
public:
    Options() {
        memset(values_, 0, sizeof(values_));
    }
    
    // @return: Be changed ?
    bool SetOption(int type, long value) {
        if (type >= Count) return false;

        if (values_[type] != value) {
            values_[type] = value;
            return true;
        }

        return false;
    }

    long GetOption(int type) const {
        if (type >= Count) return 0;
        return values_[type];
    }

private:
    long values_[Count];
};

typedef Options<sockopt_count> QuicSocketOptions;

} // namespace posix_quic
