//
// Dirt - Encrypting IRC proxy for all IRC clients
// Copyright (C) 2005-2013  Mathias Karlsson
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// See License.txt for more details.
//
// Mathias Karlsson <tmowhrekf at gmail dot com>
//

#ifndef NET_H
#define NET_H

#include <list>
#include <string>

#ifdef DIRT_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#define socklen_t int
#endif

#ifdef DIRT_UNIX
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#define SOCKET int
#define INVALID_SOCKET (SOCKET)(~0)
#define SOCKET_ERROR (-1)
#define closesocket(sock) ::close(sock)
#define ioctlsocket(sock, cmd, argp) ::ioctl(sock, cmd, argp)
#endif

namespace net {
	extern bool inited;
	bool init();
	void deinit();
	std::string resolve(std::string name);
	enum {
		error,
		idle,
		listening,
		connecting,
		connected,
		disconnecting,
		disconnected
	};
}

class socket {
public:
	socket();
	~socket();
	int status();
	void reset();
	void assign(SOCKET &new_des);
	void bind(std::string addr, int port);
	void connect(std::string addr, int port);
	void listen(int maxclients);
	bool accept(SOCKET &des, sockaddr_in &sin);
	void close(bool hard);

	void before_select(fd_set &readfds, fd_set &writefds, fd_set &exceptfds);
	void after_select(fd_set &readfds, fd_set &writefds, fd_set &exceptfds);

	std::string::size_type insize();
	std::string::size_type outsize();
	std::string::size_type peek(void *buffer, std::string::size_type bytes);
	std::string::size_type get(void *buffer, std::string::size_type bytes);
	void put(void *buffer, std::string::size_type bytes);
	bool getline(std::string &s);
	void putline(const std::string &s);

	bool get_local(sockaddr_in &sin);
	bool get_local(std::string &addr, int &port);
	bool get_remote(sockaddr_in &sin);
	bool get_remote(std::string &addr, int &port);

	//std::string l_addr, l_ip, r_addr, r_ip;
	//int l_port, r_port;

private:
	struct acceptstruct {
		SOCKET des;
		sockaddr_in sin;
	};
	void set_error();
	void readable();
	void writable();
	int status_flag;
	bool bound;
	SOCKET des;
	std::string instr;
	std::string outstr;
	std::list<acceptstruct> accepts;
};

#endif
