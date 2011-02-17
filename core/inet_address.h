// -*- mode: c++ -*-

#ifndef _INET_ADDRESS_H_
#define _INET_ADDRESS_H_

#include <arpa/inet.h>
#include <string>

namespace pipeserv {

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
    socklen_t address_length() const throw();
    unsigned short port() const;
    std::string address_string() const throw();
};

}

#endif /* _INET_ADDRESS_H_ */
