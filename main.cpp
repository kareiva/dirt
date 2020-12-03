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

#include "main.h"
#include "platform.h"
#include "cfg.h"
#include "key.h"
#include "fish.h"
#include "define.h"
#include "net.h"
#include "proxy.h"
#include "misc.h"
#include "syntax.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string>
#include <fstream>
#include <sstream>
#include <exception>

namespace dirt {
	void listen_socks4();
	void listen_socks5();
	void listen_bnc();
	void listen_failed(int port);
	void tests();
	void load_files();
	inline void check_sockets();
	inline bool user_socket_ok(class proxy &proxy);
	inline bool irc_socket_ok(class proxy &proxy);
	inline void check_new_connections();
	inline void update_proxy_cnt();
	const std::string self = "127.0.0.1";
	bool inited = false;
}

/*extern*/ class shared_info shared_info;

using namespace std;

void dirt::program()
{
	if (dying()) return;
	try {
		listen();
		while (!dying()) {
			platform::tick();
			check_sockets();
		}
		bnc.close(true);
		socks4.close(true);
		socks5.close(true);
	}
	catch (const char *s) {
		platform::error(s);
	}
	catch (bad_alloc &e) {
		platform::error((string)"Out of memory (" + e.what() + ")");
	}
	catch (exception &e) {
		platform::error((string)"C++ exception: " + e.what());
	}
	catch (...) {
		platform::error("Unrecognized error");
	}
}

void dirt::init()
{
	if (dying()) return;
	srand((unsigned int)time(0));
	tests();
	if (!net::init())
		platform::error("Could not initialize the network module");
	fish::init();
	load_files();
	platform::init();
	inited = true;
}

void dirt::deinit()
{
	dying(true);
	platform::deinit();
	if (inited) key::save();
	net::deinit();
	inited = false;
}

bool dirt::dying()
{
	bool b;
	shared_info.lock();
	b = shared_info.dying;
	shared_info.unlock();
	return b;
}

void dirt::dying(bool b)
{
	shared_info.lock();
	shared_info.dying = b;
	shared_info.unlock();
}

void dirt::version_info()
{
	string s = (string)APPNAME+" "+APPVERSION+"\n"+
		"ALPHA TEST VERSION\n"+
		"\n"+
		APPURL+"\n"+
		"\n"+
		"Engineered by " APPAUTHOR "\n"+
		APPCOPYRIGHT;
	platform::msg(s.c_str());
}

void dirt::cfg_template()
{
	class cfg defcfg;
	defcfg.generate(INIFILE_DEFAULT);
}

void dirt::cfg_upgrade()
{
	class cfg upgcfg;
	upgcfg.upgrade("");
}

void dirt::cfg_reset()
{
	class cfg defcfg;
	defcfg.generate("");
}

void dirt::listen()
{
	if (bnc.status() != net::listening) dirt::listen_bnc();
	if (socks4.status() != net::listening) dirt::listen_socks4();
	if (socks5.status() != net::listening) dirt::listen_socks5();
}

void dirt::listen_socks4()
{
	if (!cfg.socks4) return;
	socks4.bind(self, cfg.socks4_port);
	socks4.listen(100);
	if (socks4.status() != net::listening) listen_failed(cfg.socks4_port);
}

void dirt::listen_socks5()
{
	if (!cfg.socks5) return;
	socks5.bind(self, cfg.socks5_port);
	socks5.listen(100);
	if (socks5.status() != net::listening) listen_failed(cfg.socks5_port);
}

void dirt::listen_bnc()
{
	if (!cfg.bouncer) return;
	bnc.bind(self, cfg.bouncer_port);
	bnc.listen(100);
	if (bnc.status() != net::listening) listen_failed(cfg.bouncer_port);
}

void dirt::listen_failed(int port)
{
	string err_left = "Failed to listen on ";
	string err_right = ". Is the proxy already running?";
	platform::error(err_left + self + ":" + itos(port) + err_right);
}

void dirt::tests()
{
	const string err_prefix = "Bad compilation!\n";
	bool char_ok = false, short_ok = false, int_ok = false;
	if (sizeof(char) == 1 && sizeof(unsigned char) == 1)
		char_ok = true;
	if (sizeof(short) == 2 && sizeof(unsigned short) == 2)
		short_ok = true;
	if (sizeof(int) == 4 && sizeof(unsigned int) == 4)
		int_ok = true;
	if (!char_ok)
		platform::error(err_prefix + "\"char\" must be 8 bits.");
	if (!short_ok)
		platform::error(err_prefix + "\"short\" must be 16 bits.");
	if (!int_ok)
		platform::error(err_prefix + "\"int\" must be 32 bits.");
	if (sizeof(struct proxy::socks4header) != 8)
		platform::error(err_prefix + "SOCKS v4 header size mismatch");
	const char *feed = "thanks to antonone";
	string text, encd;
	base64encode(feed, encd);
	base64decode(encd, text);
	if (text.compare(feed) != 0)
		platform::error(err_prefix + "Base64 mismatch.");
}

void dirt::load_files()
{
	bool cfg_gen = false;
	string installdir = platform::installdir();
	string confdir = platform::confdir();
#ifdef DIRT_WINDOWS
	if (!installdir.empty()) installdir += '\\';
	if (!confdir.empty()) confdir += '\\';
#else
	if (!installdir.empty()) installdir += '/';
	if (!confdir.empty()) confdir += '/';
#endif
	string old_cfg_fn = installdir + INIFILE;
	string old_keys_fn = installdir + KEYFILE;
	cfg.reset();
	bool old_cfg = cfg.load(old_cfg_fn);
	bool old_keys = key::load(old_keys_fn);
	string text;
	if (old_cfg) {
		cfg_gen = true;
		remove(old_cfg_fn.c_str());
		text += "A configuration file was imported from: " +
			old_cfg_fn + '\n';
	}
	if (old_keys) {
		remove(old_keys_fn.c_str());
		text += "A key file was imported from: " + old_keys_fn + '\n';
	}
	if (!text.empty()) {
		text += "The home for configuration files is: " +
			confdir + '\n';
		platform::msg(text);
	}
	if (!cfg.load("")) cfg_gen = true;
	if (cfg_gen) cfg.generate("");
	key::load("");
	key::save();
}

inline void dirt::check_sockets()
{
	fd_set readfds, writefds, exceptfds;
	FD_ZERO(&readfds); FD_ZERO(&writefds); FD_ZERO(&exceptfds);
	bnc.before_select(readfds, writefds, exceptfds);
	socks4.before_select(readfds, writefds, exceptfds);
	socks5.before_select(readfds, writefds, exceptfds);
	list<class proxy *>::iterator iter;
	for (iter = proxys.begin(); iter != proxys.end(); iter++) {
		class proxy &proxy = **iter;
		proxy.user.before_select(readfds, writefds, exceptfds);
		proxy.irc.before_select(readfds, writefds, exceptfds);
	}
	if (dying()) return;
	int num = select(FD_SETSIZE, &readfds, &writefds, &exceptfds, 0);
	if (dying()) return;
	if (num <= 0 || num == SOCKET_ERROR) /* recover here */ return;
	bnc.after_select(readfds, writefds, exceptfds);
	socks4.after_select(readfds, writefds, exceptfds);
	socks5.after_select(readfds, writefds, exceptfds);
	check_new_connections();
	for (iter = proxys.begin(); iter != proxys.end(); iter++) {
		class proxy &proxy = **iter;
		proxy.user.after_select(readfds, writefds, exceptfds);
		proxy.irc.after_select(readfds, writefds, exceptfds);
		if (!user_socket_ok(proxy)) {
			delete *iter;
			update_proxy_cnt();
			return;
		}
		if (!irc_socket_ok(proxy)) {
			if (proxy.mode == proxy::mode_bnc &&
					!cfg.bnc_disconnect) {
				proxy.reset_irc();
				proxy.tell_init(true);
			} else {
				delete *iter;
				update_proxy_cnt();
				return;
			}
		}
		do {
			proxy.tick();
		} while (proxy.flags[proxy::ticked]);
	}
}

inline bool dirt::user_socket_ok(class proxy &proxy)
{
	if (proxy.user.status() == net::error) return false;
	if (proxy.user.status() == net::idle) return false;
	if (proxy.user.status() == net::disconnected) return false;
	return true;
}

inline bool dirt::irc_socket_ok(class proxy &proxy)
{
	if (proxy.irc.status() == net::error) return false;
	if (proxy.irc.status() == net::disconnected) return false;
	return true;
}

inline void dirt::check_new_connections()
{
	SOCKET des;
	sockaddr_in sin;
	while (bnc.accept(des, sin)) {
		string addr = inet_ntoa(sin.sin_addr);
		unsigned short port = ntohs(sin.sin_port);
		new class proxy(proxy::mode_bnc, des, addr, port);
		update_proxy_cnt();
	}
	while (socks4.accept(des, sin)) {
		string addr = inet_ntoa(sin.sin_addr);
		unsigned short port = ntohs(sin.sin_port);
		new class proxy(proxy::mode_socks4, des, addr, port);
		update_proxy_cnt();
	}
	while (socks5.accept(des, sin)) {
		string addr = inet_ntoa(sin.sin_addr);
		unsigned short port = ntohs(sin.sin_port);
		new class proxy(proxy::mode_socks5, des, addr, port);
		update_proxy_cnt();
	}
}

inline void dirt::update_proxy_cnt()
{
	shared_info.lock();
	shared_info.proxy_cnt = proxys.size();
	shared_info.unlock();
}

shared_info::shared_info()
{
	dying = false;
	proxy_cnt = 0;
}

void assume_error(const char *file, const int line, const char *expr)
{
	stringstream ss;
	ss << "Assumption failed!\n\n";
	ss << "In file " << file << " at line " << line << ": ";
	ss << expr << "\n\n";
	ss << "Please report this error to ";
	ss << APPAUTHOR << " <" << APPMAIL << ">\n";
	platform::error(ss.str());
}
