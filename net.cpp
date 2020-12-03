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

#include "net.h"
#include "misc.h"

using namespace std;

bool net::inited = false;

bool net::init()
{
	if (inited) return true;
#ifdef DIRT_WINDOWS
	WORD ver = MAKEWORD(2, 0);
	WSADATA wsadata;
	if (WSAStartup(ver, &wsadata) != 0) return false;
	if (wsadata.wVersion != ver) {
		WSACleanup();
		return false;
	}
#endif
	inited = true;
	return true;
}

void net::deinit()
{
	if (!inited) return;
#ifdef DIRT_WINDOWS
	WSACleanup();
#endif
	inited = false;
}

string net::resolve(string name)
{
	const char *s = name.c_str();
	hostent *he = gethostbyname(s);
	unsigned long ip;
	if (he != 0) {
		unsigned char *p = (unsigned char *)he->h_addr;
		ip = *p++;
		ip |= *p++ << 8;
		ip |= *p++ << 16;
		ip |= *p << 24;
		in_addr ia;
		ia.s_addr = ip;
		return inet_ntoa(ia);
	}
	ip = inet_addr(s);
	if (ip == INADDR_NONE) {
		return "";
	} else {
		in_addr ia;
		ia.s_addr = ip;
		return inet_ntoa(ia);
	}
}

socket::socket()
{
	status_flag = net::idle;
	bound = false;
	des = INVALID_SOCKET;
}

socket::~socket()
{
	if (des != INVALID_SOCKET) closesocket(des);
}

int socket::status()
{
	return status_flag;
}

void socket::reset()
{
	if (des != INVALID_SOCKET) { closesocket(des); des = INVALID_SOCKET; }
	status_flag = net::idle;
	bound = false;
	outstr.erase();
}

void socket::assign(SOCKET &new_des)
{
	reset();
	des = new_des;
	unsigned long nonblocking = 1;
	if (ioctlsocket(des, FIONBIO, &nonblocking) != 0) { set_error(); return; }
	bound = true;
	status_flag = net::connected;
}

void socket::bind(string addr, int port)
{
	reset();
	des = ::socket(PF_INET, SOCK_STREAM, 0);
	if (des == INVALID_SOCKET) { set_error(); return; }
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	if (port < 1 || port > 65535) {
		sin.sin_port = 0;
	} else {
		sin.sin_port = htons(port);
	}
	if (addr.empty()) {
		sin.sin_addr.s_addr = INADDR_ANY;
	} else {
		string ip = net::resolve(addr);
		if (ip.empty()) return;
		unsigned long ipv4 = inet_addr(ip.c_str());
		if (ipv4 == INADDR_NONE) return;
		sin.sin_addr.s_addr = ipv4;
	}
	if (::bind(des, (sockaddr *)&sin, sizeof(sin)) != 0) { set_error(); return; }
	unsigned long nonblocking = 1;
	if (ioctlsocket(des, FIONBIO, &nonblocking) != 0) { set_error(); return; }
	bound = true;
}

void socket::connect(string addr, int port)
{
	if (!bound) bind("", 0);
	if (status_flag != net::idle) { set_error(); return; };
	sockaddr_in sin;
	sin.sin_family = AF_INET;
	if (port < 1 || port > 65535) {
		return;
	} else {
		sin.sin_port = htons(port);
	}
	if (addr.empty()) {
		return;
	} else {
		string ip = net::resolve(addr);
		if (ip.empty()) return;
		unsigned long ipv4 = inet_addr(ip.c_str());
		if (ipv4 == INADDR_NONE) return;
		sin.sin_addr.s_addr = ipv4;
	}
	::connect(des, (sockaddr *)&sin, sizeof(sin));
	status_flag = net::connecting;
}

void socket::listen(int maxclients)
{
	if (status_flag != net::idle || !bound) { set_error(); return; }
	if (::listen(des, maxclients) != 0) { set_error(); return; }
	status_flag = net::listening;
}

bool socket::accept(SOCKET &des, sockaddr_in &sin)
{
	if (accepts.empty()) return false;
	acceptstruct new_accept;
	new_accept = accepts.front();
	des = new_accept.des;
	sin = new_accept.sin;
	accepts.pop_front();
	return true;
}

void socket::close(bool hard)
{
	if (status_flag == net::listening) hard = true;
	if (outstr.empty()) hard = true;
	status_flag = net::disconnecting;
	if (hard && des != INVALID_SOCKET) {
		closesocket(des);
		des = INVALID_SOCKET;
	}
	if (hard) status_flag = net::disconnected;
}

void socket::before_select(fd_set &readfds, fd_set &writefds, fd_set &exceptfds)
{
	if (status_flag == net::listening) {
		FD_SET(des, &readfds);
	} else if (status_flag == net::connecting) {
		FD_SET(des, &writefds);
		FD_SET(des, &exceptfds);
	} else if (status_flag == net::connected || status_flag == net::disconnecting) {
		FD_SET(des, &readfds);
		if (!outstr.empty()) FD_SET(des, &writefds);
	}
}

void socket::after_select(fd_set &readfds, fd_set &writefds, fd_set &exceptfds)
{
	if (status_flag == net::listening) {
		if (!FD_ISSET(des, &readfds)) return;
		acceptstruct new_accept;
		socklen_t length = sizeof(sockaddr_in);
		new_accept.des = ::accept(des, (sockaddr *)&new_accept.sin, &length);
		if (new_accept.des == INVALID_SOCKET) {
			set_error();
		} else {
			accepts.push_back(new_accept);
		}
	} else if (status_flag == net::connecting) {
		if (FD_ISSET(des, &writefds)) {
			status_flag = net::connected;
		}
		if (FD_ISSET(des, &exceptfds)) {
			status_flag = net::disconnected;
		}
	} else if (status_flag == net::connected || status_flag == net::disconnecting) {
		if (FD_ISSET(des, &readfds)) readable();
		if (FD_ISSET(des, &writefds)) writable();
	}
	if (status_flag == net::disconnecting && outstr.empty()) close(true);
}

string::size_type socket::insize()
{
	return instr.size();
}

string::size_type socket::outsize()
{
	return outstr.size();
}

string::size_type socket::peek(void *buffer, string::size_type bytes)
{
	string::size_type num = (bytes > instr.size()) ? instr.size() : bytes;
	num = instr.copy((char *)buffer, num);
	return num;
}

string::size_type socket::get(void *buffer, string::size_type bytes)
{
	string::size_type num = (bytes > instr.size()) ? instr.size() : bytes;
	num = instr.copy((char *)buffer, num);
	if (num > 0) instr.erase(0, num);
	return num;
}

void socket::put(void *buffer, std::string::size_type bytes)
{
	outstr.append((char *)buffer, bytes);
}

bool socket::getline(string &s)
{
	string::size_type pos;
	pos = instr.find_first_of('\x0a');
	if (pos == string::npos) {
		s.erase();
		return false;
	}
	s = instr.substr(0, pos);
	instr.erase(0, pos + 1);
	remove_bad_chars(s);
	return true;
}

void socket::putline(const string &s)
{
	outstr.append(s);
	outstr.append("\x0d\x0a");
}

bool socket::get_local(sockaddr_in &sin)
{
	sin.sin_family = AF_INET;
	socklen_t addrlen = sizeof(sockaddr_in);
	if (getsockname(des, (sockaddr *)&sin, &addrlen) != 0) {
		sin.sin_port = 0;
		sin.sin_addr.s_addr = INADDR_ANY;
		return false;
	}
	return true;
}

bool socket::get_local(string &addr, int &port)
{
	addr.clear();
	port = 0;
	sockaddr_in sin;
	if (!get_local(sin)) return false;
	addr = inet_ntoa(sin.sin_addr);
	port = ntohs(sin.sin_port);
	return true;
}

bool socket::get_remote(sockaddr_in &sin)
{
	sin.sin_family = AF_INET;
	socklen_t addrlen = sizeof(sockaddr_in);
	if (getpeername(des, (sockaddr *)&sin, &addrlen) != 0) {
		sin.sin_port = 0;
		sin.sin_addr.s_addr = INADDR_ANY;
		return false;
	}
	return true;
}

bool socket::get_remote(string &addr, int &port)
{
	addr.clear();
	port = 0;
	sockaddr_in sin;
	if (!get_remote(sin)) return false;
	addr = inet_ntoa(sin.sin_addr);
	port = ntohs(sin.sin_port);
	return true;
}

void socket::set_error()
{
	reset();
	status_flag = net::error;
}

void socket::readable()
{
	const int bufsize = 2048;
	char buffer[bufsize];
	int num = recv(des, buffer, bufsize, 0);
	if (num < 0) {
		status_flag = net::error;
	} else if (num == 0) {
		status_flag = net::disconnected;
	} else if (num > 0) {
		instr.append(buffer, num);
	}
}

void socket::writable()
{
	int num = send(des, outstr.data(), outstr.size(), 0);
	if (num == -1) {
		status_flag = net::error;
	} else if (num > 0) {
		outstr.erase(0, num);
	}
}
