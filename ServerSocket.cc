// Jolie Davison jdavi@cs.washington.edu Copyright 2024 Jolie Davison

#include <stdio.h>       // for snprintf()
#include <unistd.h>      // for close(), fcntl()
#include <sys/types.h>   // for socket(), getaddrinfo(), etc.
#include <sys/socket.h>  // for socket(), getaddrinfo(), etc.
#include <arpa/inet.h>   // for inet_ntop()
#include <netdb.h>       // for getaddrinfo()
#include <errno.h>       // for errno, used by strerror()
#include <string.h>      // for memset, strerror()
#include <iostream>      // for std::cerr, etc.

#include "./ServerSocket.h"

extern "C" {
  #include "libhw1/CSE333.h"
}

namespace hw4 {

ServerSocket::ServerSocket(uint16_t port) {
  port_ = port;
  listen_sock_fd_ = -1;
}

ServerSocket::~ServerSocket() {
  // Close the listening socket if it's not zero.  The rest of this
  // class will make sure to zero out the socket if it is closed
  // elsewhere.
  if (listen_sock_fd_ != -1)
    close(listen_sock_fd_);
  listen_sock_fd_ = -1;
}

bool ServerSocket::BindAndListen(int ai_family, int* const listen_fd) {
  // Use "getaddrinfo," "socket," "bind," and "listen" to
  // create a listening socket on port port_.  Return the
  // listening socket through the output parameter "listen_fd"
  // and set the ServerSocket data member "listen_sock_fd_"

  // STEP 1:
  // Populate the "hints" addrinfo structure for getaddrinfo().
  struct addrinfo hints, *result;
  bool firstTry = true;
  bool secondTry = false;
  memset(&hints, 0, sizeof(struct addrinfo));
  // Set family accordingly. Might have to try again with UNSPEC.
  if (ai_family == AF_UNSPEC) {
    hints.ai_family = AF_INET;
  } else {
    hints.ai_family = AF_INET6;
  }
  hints.ai_socktype = SOCK_STREAM;  // stream
  hints.ai_flags = AI_PASSIVE;      // use wildcard "INADDR_ANY"
  hints.ai_flags |= AI_V4MAPPED;    // use v4-mapped v6 if no v6 found
  hints.ai_protocol = IPPROTO_TCP;  // tcp protocol
  hints.ai_canonname = nullptr;
  hints.ai_addr = nullptr;
  hints.ai_next = nullptr;

  // Convert the uint16_t port_ to char* for getaddrinfo().
  std::string port_str = std::to_string(port_);
  const char* port_cstr = port_str.c_str();

  // getaddrinfo() returns a list of address structures
  // via the output parameter "result".
  int res = getaddrinfo(nullptr, port_cstr, &hints, &result);

  // Did getaddrinfo() fail?
  if (res != 0) {
    std::cerr << "getaddrinfo() failed: ";
    std::cerr << gai_strerror(res) << std::endl;
    return false;
  }

  // Loop through the returned address structures until we are able
  // to create a socket and bind to one. The address structures are
  // linked in a list through the "ai_next" field of result.
  while (firstTry || secondTry) {
    *listen_fd = -1;
    for (struct addrinfo *rp = result; rp != nullptr; rp = rp->ai_next) {
      *listen_fd = socket(rp->ai_family,
                         rp->ai_socktype,
                         rp->ai_protocol);
      if (*listen_fd == -1) {
        // Creating this socket failed. So, loop to the next returned
        // result and try again.
        std::cerr << "socket() failed: " << strerror(errno) << std::endl;
        *listen_fd = -1;
        continue;
      }

      // Configure the socket; we're setting a socket "option."  In
      // particular, we set "SO_REUSEADDR", which tells the TCP stack
      // so make the port we bind to available again as soon as we
      // exit, rather than waiting for a few tens of seconds to recycle it.
      int optval = 1;
      setsockopt(*listen_fd, SOL_SOCKET, SO_REUSEADDR,
                 &optval, sizeof(optval));

      // Try binding the socket to the address and port number returned
      // by getaddrinfo().
      if (bind(*listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
        // Bind worked!  Return to the caller the address family.
        break;
      }
      // The bind failed.  Close the socket, then loop back around and
      // try the next address/port returned by getaddrinfo().
      std::cerr << "bind() failed" << std::endl;
    }
    // Checks if we need to try again in the case where socket connection
    // failed and client passed in AF_UNSPEC.
    if (ai_family == AF_UNSPEC
        && hints.ai_family == AF_INET
        && *listen_fd <= 0) {
      hints.ai_family = AF_INET6;
      secondTry = true;
    } else {
      secondTry = false;
    }
    firstTry = false;
  }

  // Free the structure returned by getaddrinfo().
  freeaddrinfo(result);

  // If we failed to bind, return failure.
  if (*listen_fd <= 0) {
    std::cerr << "failed to BIND" << std::endl;
    return false;
  }

  // Success. Tell the OS that we want this to be a listening socket.
  if (listen(*listen_fd, SOMAXCONN) != 0) {
    std::cerr << "Failed to mark socket as listening: ";
    std::cerr << strerror(errno) << std::endl;
    close(*listen_fd);
    return false;
  }

  // Sucess.
  listen_sock_fd_ = *listen_fd;
  sock_family_ = hints.ai_family;
  return true;
}

bool ServerSocket::Accept(int* const accepted_fd,
                          std::string* const client_addr,
                          uint16_t* const client_port,
                          std::string* const client_dns_name,
                          std::string* const server_addr,
                          std::string* const server_dns_name) const {
  // Accept a new connection on the listening socket listen_sock_fd_.
  // (Block until a new connection arrives.)  Return the newly accepted
  // socket, as well as information about both ends of the new connection,
  // through the various output parameters.

  // STEP 2:
  struct sockaddr_storage caddr;
  socklen_t caddr_len = sizeof(caddr);
  while (1) {
    *accepted_fd = accept(listen_sock_fd_,
                           reinterpret_cast<struct sockaddr *>(&caddr),
                           &caddr_len);
    if (*accepted_fd < 0) {
      if ((errno == EAGAIN) || (errno == EINTR)) {
        continue;
      }
      std::cerr << "Failure on accept: " << strerror(errno) << std::endl;
      return false;
    }
    break;
  }

  char hname[1024];

  // Get client IP address and port
  if (caddr.ss_family == AF_INET) {
    // The client is using an IPv4 address.
    struct sockaddr_in *cli4 = reinterpret_cast<struct sockaddr_in*>(&caddr);
    char addrbuf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &cli4->sin_addr, addrbuf, INET_ADDRSTRLEN);

    *client_addr = addrbuf;
    *client_port = ntohs(cli4->sin_port);

  } else {
    // The client is using an IPv6 address.
    struct sockaddr_in6 *cli6 = reinterpret_cast<struct sockaddr_in6*>(&caddr);
    char addrbuf[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &cli6->sin6_addr, addrbuf, INET6_ADDRSTRLEN);

    *client_addr = addrbuf;
    *client_port = ntohs(cli6->sin6_port);
  }

  // Get the client's dns name, or return it's IP address as
  // a substitute if the dns lookup fails.
  if (getnameinfo(reinterpret_cast<struct sockaddr *>(&caddr),
                  caddr_len, hname, 1024, NULL, 0, 0) == 0) {
  *client_dns_name = hname;
  } else {
    *client_dns_name = *client_addr;
  }

  // Get server IP address
  struct sockaddr_storage saddr;
  socklen_t saddr_len = sizeof(saddr);
  if (getsockname(*accepted_fd,
                  reinterpret_cast<struct sockaddr *>(&saddr),
                  &saddr_len) == -1) {
      std::cerr << "Failure on server address: "
                << strerror(errno) << std::endl;
      close(*accepted_fd);
      return false;
  }

  if (saddr.ss_family == AF_INET) {
  // The server is using an IPv4 address.
  struct sockaddr_in *srvr4 = reinterpret_cast<struct sockaddr_in*>(&saddr);
  char addrbuf[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &srvr4->sin_addr, addrbuf, INET_ADDRSTRLEN);
  *server_addr = addrbuf;

  } else {
  // The server is using an IPv6 address.
  struct sockaddr_in6 *srvr6 = reinterpret_cast<struct sockaddr_in6*>(&saddr);
  char addrbuf[INET6_ADDRSTRLEN];
  inet_ntop(AF_INET6, &srvr6->sin6_addr, addrbuf, INET6_ADDRSTRLEN);
  *server_addr = addrbuf;
  }

  // Get the server's dns name, or return it's IP address as
  // a substitute if the dns lookup fails.
  if (getnameinfo(reinterpret_cast<struct sockaddr *>(&saddr),
                  saddr_len, hname, 1024, NULL, 0, 0) == 0) {
    *server_dns_name = hname;
  } else {
    *server_dns_name = *server_addr;
  }

  return true;
}

}  // namespace hw4
