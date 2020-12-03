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

#ifndef PROXY_H
#define PROXY_H

#include "net.h"
#include <string>
#include <list>
#include <bitset>

class proxy {
public:
	enum mode_t {
		mode_bnc = 0,
		mode_socks4,
		mode_socks5
	};
	enum flags_t {
		ticked = 0,
		reject,
		io,
		locked,
		irc_pass,
		irc_auth, // sent_irc_info
		told_lock,
		told_init,
		told_hi,
		told_ver,
		jumped,
		flags_size
	};
	struct ircdata {
		std::string pass;
		std::string nick, user;
		std::string server;
		int port;
		std::string network;
		std::string client;
	};
	struct socksdata {
		std::string user;
		std::string pass;
		bool sent;
		bool received;
		bool method;
		bool auth;
		unsigned char rep;
	};
#pragma pack(push)
#pragma pack(1)
	struct socks4header {
		unsigned char vn;
		unsigned char cd;
		unsigned short port;
		unsigned int ip;
	};
	struct socks5header {
		unsigned char ver;
		unsigned char cmd;
		unsigned char rsv;
		unsigned char atyp;
		unsigned char peek;
		//union addr_t {
		//	struct ipv4_t {
		//		unsigned int ip;
		//		unsigned short port;
		//		unsigned char fill[12];
		//	} ipv4;
		//	struct ipv6_t {
		//		unsigned char ip[16];
		//		unsigned short port;
		//	} ipv6;
		//} addr;
	};
#pragma pack(pop)

	proxy(int proxymode, SOCKET userdes, std::string addr, int port);
	~proxy();
	void tick();
	void reset_irc();
	void load_cfg();
	void unlock(const std::string &pw);

	void socks4_auth();
	void socks5_method();
	void socks5_auth();
	void socks5_receive();
	void socks5_send();
	void send_irc_auth();
	void jump(std::string args);

	void tell_lock();
	void tell_init(bool disc);
	void tell_ver();

	void in_server(std::string &line);
	std::string in_ctcp(std::string source, std::string sourceaddr,
			std::string req, std::string args);
	void out_server(std::string &line);
	void out_message(const std::string &prefix,
			const std::string &cmd, const std::string &target,
			std::string text);
	void out_dirt(std::string cmd, std::string args);

	void acceptkey(std::string target);
	void dh1080_sendkey(std::string target);
	void dh1080_acceptkey(std::string target);
	void dh1080_receive(std::string mynick, std::string nick, std::string addr, std::string cmd, std::string theirpubkey);
	void receivechankey(std::string nick, const std::string &hostmask,
			std::string chan, std::string chankey);

	void info(std::string s);
	void end_info();
	void play_file(std::string filename, bool codes);
	void play_memory(const char *buf, int len, bool codes);
	void play_string(const std::string &s,
			const std::string prefix, const bool codes);
	void play_stream(std::istream &in,
			const std::string prefix, const bool codes);
	void play_stream_with_chan(std::istream &in, const std::string chan);
	void dirtstring(std::string &s);

	bool psybnc(std::string &args, std::string &args_pre,
			std::string &keyname);
	void cmdlist(std::string cmd, bool full);
	void help(std::string cmd);
	void bad_command(const std::string &cmd);
	void bad_syntax(const std::string &cmd);
	void bad_target(const std::string &target);
	void bad_algo(const std::string &algo);
	void bad_key_length(const int len);
	void keylist(const bool reveal);
	void matchkeys(const std::string &pattern, const bool reveal);
	void showkey(const std::string &target, const bool reveal);
	void disconnect_user();
	void status();
	static void status_listener(std::ostream &out, class socket &sock);
	static void status_proxy(std::ostream &out, class proxy &proxy);

	class socket user, irc;
	int mode;
	std::bitset<flags_size> flags;
	struct socksdata socksdata;
	struct ircdata ircdata;
};

extern class socket bnc;
extern class socket socks4;
extern class socket socks5;
extern std::list<class proxy *> proxys;

#endif
