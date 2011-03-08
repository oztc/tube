#include "pch.h"

#include "core/inet_address.h"
#include "utils/exception.h"

namespace pipeserv {

InternetAddress::InternetAddress()
{
    memset(&addr_, 0, max_address_length());
}

socklen_t
InternetAddress::address_length() const throw()
{
    switch (family()) {
    case AF_INET:
        return sizeof(sockaddr_in);
    case AF_INET6:
        return sizeof(sockaddr_in6);
    default:
        throw utils::UnrecognizedAddress();
    }
}

std::string
InternetAddress::address_string() const throw()
{
    char pstr[INET6_ADDRSTRLEN];
    void* inaddr = NULL;
    const char* result = NULL;

    memset(pstr, 0, INET6_ADDRSTRLEN);
    switch (family()) {
    case AF_INET:
        result = inet_ntop(AF_INET, &(addr_.v4_addr.sin_addr), pstr,
                           INET_ADDRSTRLEN);
        break;
    case AF_INET6:
        result = inet_ntop(AF_INET6, &(addr_.v6_addr.sin6_addr), pstr,
                           INET6_ADDRSTRLEN);
        break;
    }
    if (result == NULL) {
        throw utils::UnrecognizedAddress();
    }
    return std::string(pstr);
}

unsigned short
InternetAddress::port() const
{
    switch (family()) {
    case AF_INET:
        return addr_.v4_addr.sin_port;
    case AF_INET6:
        return addr_.v6_addr.sin6_port;
    }
}

}
