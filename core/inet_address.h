// -*- mode: c++ -*-

#ifndef _INET_ADDRESS_H_
#define _INET_ADDRESS_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>

namespace tube {

class InternetAddress
{
    union {
        sockaddr_in6 v6_addr;
        sockaddr_in  v4_addr;
    } addr_;

public:
    InternetAddress();
    // used for accept
    size_t max_address_length() const { return sizeof(addr_); }
    unsigned short family() const { return get_address()->sa_family; }
    sockaddr* get_address() const { return (sockaddr*) &addr_; }
    socklen_t address_length() const ;
    unsigned short port() const;
    std::string address_string() const ;
};

}

#endif /* _INET_ADDRESS_H_ */
