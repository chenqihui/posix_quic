#include "debug.h"
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/time.h>
#include "entry.h"
#include <sys/epoll.h>
#include <poll.h>
#include "epoller_entry.h"
#include "socket_entry.h"
#include "stream_entry.h"

namespace posix_quic {

FILE* debug_output = stdout;
uint64_t debug_mask = 0;

std::string GetCurrentTime()
{
#if __linux__
    struct tm local;
    struct timeval tv;
    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &local);
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d.%06lu",
            local.tm_hour, local.tm_min, local.tm_sec, tv.tv_usec);
//    snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d.%06lu",
//            local.tm_year+1900, local.tm_mon+1, local.tm_mday, 
//            local.tm_hour, local.tm_min, local.tm_sec, tv.tv_usec);
    return std::string(buffer);
#else
    return std::string();
#endif
}

int GetCurrentProcessID()
{
#if __linux__
    return getpid();
#else
    return 0;
#endif 
}

const char* BaseFile(const char* file)
{
    const char* p = strrchr(file, '/');
    if (p) return p + 1;

    p = strrchr(file, '\\');
    if (p) return p + 1;

    return file;
}

uint64_t & TlsConnectionId()
{
    static thread_local uint64_t connId = 0;
    return connId;
}

TlsConnectionIdGuard::TlsConnectionIdGuard(uint64_t id)
{
    backup_ = TlsConnectionId();
    TlsConnectionId() = id;
}
TlsConnectionIdGuard::~TlsConnectionIdGuard()
{
    TlsConnectionId() = backup_;
}

std::string Bin2Hex(const char* data, size_t length, const std::string& split)
{
    if (!data || !length) return "";

    static const char *hex = "0123456789abcdefg";

    char buf[2] = {};
    std::string s;
    s.reserve(length * 2 + split.size() * (length - 1));
    for (size_t i = 0; i < length; ++i) {
        unsigned char c = ((unsigned char*)data)[i];
        buf[0] = hex[c >> 4];
        buf[1] = hex[c & 0xf];
        s.append(buf, 2);

        if (i + 1 < length)
            s.append(split);
    }
    return s;
}

const char* PollEvent2Str(short int event)
{
    switch (event) {
        case 0: return "nil"; 
        case POLLIN: return "POLLIN";
        case POLLOUT: return "POLLOUT";
        case POLLERR: return "POLLERR";

        case POLLIN|POLLOUT: return "POLLIN|POLLOUT";
        case POLLIN|POLLERR: return "POLLIN|POLLERR";
        case POLLOUT|POLLERR: return "POLLOUT|POLLERR";

        case POLLIN|POLLOUT|POLLERR: return "POLLIN|POLLOUT|POLLERR";
        default:
            return "Unkown";
    }
}

const char* EpollEvent2Str(uint32_t event)
{
    switch (event) {
        case 0: return "nil"; 
        case EPOLLIN: return "EPOLLIN";
        case EPOLLOUT: return "EPOLLOUT";
        case EPOLLERR: return "EPOLLERR";

        case EPOLLIN|EPOLLOUT: return "EPOLLIN|EPOLLOUT";
        case EPOLLIN|EPOLLERR: return "EPOLLIN|EPOLLERR";
        case EPOLLOUT|EPOLLERR: return "EPOLLOUT|EPOLLERR";

        case EPOLLIN|EPOLLOUT|EPOLLERR: return "EPOLLIN|EPOLLOUT|EPOLLERR";
        default:
            return "Unkown";
    }
}

const char* EpollOp2Str(int op)
{
    switch (op) {
        case EPOLL_CTL_ADD:
            return "EPOLL_CTL_ADD";

        case EPOLL_CTL_MOD:
            return "EPOLL_CTL_MOD";

        case EPOLL_CTL_DEL:
            return "EPOLL_CTL_DEL";

        default:
            return "nil";
    }
}

const char* EntryCategory2Str(int category)
{
    switch (category) {
        case (int)EntryCategory::Invalid:
            return "Invalid";

        case (int)EntryCategory::Socket:
            return "Socket";

        case (int)EntryCategory::Stream:
            return "Stream";

        default:
            return "Unkown";
    }
}

const char* Perspective2Str(int perspective)
{
    switch (perspective) {
        case (int)net::Perspective::IS_CLIENT:
            return "IS_CLIENT";

        case (int)net::Perspective::IS_SERVER:
            return "IS_SERVER";

        default:
            return "Unkown";
    }
}

std::string Format(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char buf[4096];
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    return std::string(buf, len);
}

std::string P(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char buf[4096];
    int len = vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    buf[len] = '\n';
    buf[len+1] = '\0';
    va_end(ap);
    return std::string(buf, len + 1);
}
std::string P()
{
    return "\n";
}

std::string GlobalDebugInfo(uint32_t sourceMask)
{
    std::string info;
    std::string line(30, '=');
    line += P();

    info += line;
    if (sourceMask & src_epoll) {
        QuicEpollerEntry::GetFdManager().Foreach(
                [&](int epfd, QuicEpollerEntryPtr const& ep) {
                    info += ep->GetDebugInfo(1) + P();
                });
        info += line;
    }

    std::string socketInfo, streamInfo;
    if ((sourceMask & src_socket) || (sourceMask & src_stream)) {
        EntryBase::GetFdManager().Foreach(
                [&](int epfd, EntryPtr const& entry) {
                    EntryCategory category = entry->Category();
                    if (category == EntryCategory::Socket && (sourceMask & src_socket)) {
                        socketInfo += entry->GetDebugInfo(1);
                    }
                    else if (category == EntryCategory::Stream && (sourceMask & src_stream)) {
                        streamInfo += entry->GetDebugInfo(1);
                    }
                });
    }
    info += socketInfo;
    info += line;
    info += streamInfo;
    info += line;

    return info;
}

} // namespace posix_quic
