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

#include "proxy.h"
#include "define.h"
#include "cfg.h"
#include "misc.h"
#include "key.h"
#include "fish.h"
#include "main.h"
#include "syntax.h"
#include "dh1080.h"
#include "inc.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <openssl/md5.h>
#include <openssl/sha.h>

using namespace std;

/*extern*/ class socket bnc;
/*extern*/ class socket socks4;
/*extern*/ class socket socks5;
/*extern*/ list<class proxy *> proxys;

inline bool good_irc_server(const string &addr, int port);
inline bool is_chan(const string &s);
inline void format_msg(string &msg, int key_ret);

const string algo_fish = "fish";

proxy::proxy(int proxymode, SOCKET userdes, string addr, int port)
{
	cfg.reset();
	cfg.load("");
	proxys.push_back(this);
	mode = proxymode;
	user.assign(userdes);
	flags.reset();
	reset_irc();
	if (addr != "127.0.0.1") flags.set(reject);
	ircdata.server = cfg.irc_server;
	ircdata.port = cfg.irc_port;
	flags.set(locked, !cfg.password.empty());
	socksdata.sent = false;
	socksdata.received = false;
	socksdata.method = false;
	socksdata.auth = false;
	socksdata.rep = 0;
}

proxy::~proxy()
{
	proxys.remove(this);
}

void proxy::tick()
{
	flags.reset(ticked);
	if (!flags[io]) {
		if (mode == mode_socks4) {
			socks4_auth();
		} else if (mode == mode_socks5) {
			if (!socksdata.method) {
				socks5_method();
			} else if (!socksdata.auth) {
				socks5_auth();
			} else if (!socksdata.received) {
				socks5_receive();
			} else if (!socksdata.sent) {
				socks5_send();
			}
		}
	}
	if (flags[io]) {
		string line;
		if (flags[reject]) {
			while (user.getline(line)) {}
			while (irc.getline(line)) {}
		} else {
			while (user.getline(line)) out_server(line);
			while (irc.getline(line)) in_server(line);
		}
		if (mode == mode_bnc) {
			if (flags[locked] && !flags[told_lock])
				tell_lock();
			if (!flags[locked] && !flags[told_init])
				tell_init(false);
		}
		if (!flags[locked] && !flags[irc_auth]) send_irc_auth();
	}
}

void proxy::reset_irc()
{
	flags.set(ticked);
	if (mode == mode_bnc) flags.set(io);
	flags.reset(irc_pass);
	flags.reset(irc_auth);
	flags.reset(told_ver);
	ircdata.network.clear();
	ircdata.client.clear();
	char buffer[512];
	while (irc.get(buffer, 512) > 0) {}
	irc.reset();
}

void proxy::load_cfg()
{
	bool last_bnc = (bnc.status() == net::listening);
	int last_bouncer_port = cfg.bouncer_port;
	bool last_socks4 = (socks4.status() == net::listening);
	int last_socks4_port = cfg.socks4_port;
	bool last_socks5 = (socks5.status() == net::listening);
	int last_socks5_port = cfg.socks5_port;
	cfg.reset();
	cfg.load("");
	if (!cfg.bouncer && last_bnc) bnc.close(true);
	if (!cfg.socks4 && last_socks4) socks4.close(true);
	if (!cfg.socks5 && last_socks5) socks5.close(true);
	if (last_bouncer_port != cfg.bouncer_port) bnc.close(true);
	if (last_socks4_port != cfg.socks4_port) socks4.close(true);
	if (last_socks5_port != cfg.socks5_port) socks5.close(true);
	dirt::listen();
}

void proxy::unlock(const string &pw)
{
	if (!flags[locked]) return;
	if (cfg.password.empty()) {
		flags.reset(locked);
		flags.set(ticked);
		return;
	}
	bool match = false;
	string password = lowercase(gettok(cfg.password, " ", 0));
	string format = lowercase(cfg.password_format);
	if (format == "sha1") {
		unsigned char sha1[20 + 1] = { 0 };
		SHA1(reinterpret_cast<const unsigned char *>(pw.data()),
			pw.size(), sha1);
		string sha1_hex;
		for (int i = 0; i < 20; i++) {
			string hex = itos((int)sha1[i], 16);
			while (hex.size() < 2) hex.insert(0, "0");
			sha1_hex += hex;
		}
		strlower(sha1_hex);
		match = (sha1_hex == cfg.password);
	} else if (format == "md5sum" || format == "md5") {
		unsigned char md5[16 + 1] = { 0 };
		MD5(reinterpret_cast<const unsigned char *>(pw.data()),
			pw.size(), md5);
		string md5_hex;
		for (int i = 0; i < 16; i++) {
			string hex = itos((int)md5[i], 16);
			while (hex.size() < 2) hex.insert(0, "0");
			md5_hex += hex;
		}
		strlower(md5_hex);
		match = (md5_hex == cfg.password);
	} else if (format == "text") {
		match = (pw == cfg.password);
	}
	if (match) {
		flags.reset(locked);
		flags.set(ticked);
	}
}

void proxy::socks4_auth()
{
	socks4header header;
	if (!socksdata.received) {
		const unsigned int bufsize = sizeof(socks4header) + 512;
		char buf[bufsize] = { 0 };
		string::size_type num = user.peek(buf, bufsize);
		if (num < sizeof(socks4header) + 1) return;
		if (num == bufsize && buf[num - 1] != 0) {
			socksdata.received = true;
			flags.set(reject);
			flags.set(ticked);
			return;
		}
		if (buf[num - 1] != 0) return;
		user.get((char *)&header, sizeof(socks4header));
		user.get(buf, bufsize - sizeof(socks4header));
		socksdata.user = buf;
		if (header.vn != 4 || header.cd != 1) flags.set(reject);
		in_addr inaddr;
		inaddr.s_addr = *(unsigned int *)&header.ip;
		string ip = inet_ntoa(inaddr);
		int port = ntohs(header.port);
		if (!good_irc_server(ip, port)) flags.set(reject);
		ircdata.server = ip;
		ircdata.port = port;
		socksdata.received = true;
		flags.set(ticked);
		if (!flags[reject]) {
			if (flags[locked]) unlock(socksdata.user);
			if (flags[locked]) {
				flags.set(reject);
			} else {
				reset_irc();
				irc.bind("", 0);
				irc.connect(ircdata.server, ircdata.port);
			}
		}
	}
	if (!socksdata.received || socksdata.sent) return;
	bool conn = (irc.status() == net::connected ||
			irc.status() == net::disconnecting);
	if (!flags[reject] && conn) {
		header.vn = 0;
		header.cd = 90;
		header.ip = 0;
		header.port = 0;
		user.put((char *)&header, sizeof(header));
		socksdata.sent = true;
		flags.set(ticked);
		flags.set(io);
		flags.set(told_lock);
		flags.set(told_init);
		flags.set(told_hi);
	} else if (flags[reject] ||
			(irc.status() != net::connecting && !conn)) {
		header.vn = 0;
		header.cd = 91;
		header.ip = 0;
		header.port = 0;
		user.put((char *)&header, sizeof(header));
		user.close(false);
		socksdata.sent = true;
		flags.set(ticked);
	}
}

void proxy::socks5_method()
{
	if (flags[reject]) return;
	if (user.insize() < 2) return;
	unsigned char out[2];
	unsigned char &outver = out[0];
	unsigned char &method = out[1];
	outver = 5;
	method = 0xff;
	unsigned char in[2] = {};
	unsigned char &inver = in[0];
	unsigned char &nmethods = in[1];
	user.peek(in, 2);
	if (nmethods < 1) {
		user.get(in, 2);
		user.put(out, 2);
		user.close(false);
		socksdata.method = true;
		flags.set(reject);
		return;
	}
	if (user.insize() < (string::size_type)2 + nmethods) return;
	unsigned char *methods = new unsigned char [nmethods];
	user.get(in, 2);
	user.get(methods, nmethods);
	for (int i = 0; i < nmethods; i++) {
		if (!flags[locked] && methods[i] == 0) {
			method = 0; // no auth
			socksdata.auth = true;
			break;
		}
		if (methods[i] == 2) method = 2; // user/pass auth
	}
	delete [] methods;
	if (inver != 5) method = 0xff;
	user.put(out, 2);
	socksdata.method = true;
	if (method == 0xff) {
		user.close(false);
		flags.set(reject);
	}
	flags.set(ticked);
}

void proxy::socks5_auth()
{
	if (user.insize() < 3) return;
	unsigned char data[255 * 2 + 3];
	unsigned char *ver = &data[0];
	unsigned char *ulen = &data[1];
	unsigned char *plen = 0;
	string::size_type datalen = user.insize();
	user.peek(data, datalen);
	if ((string::size_type)2 + *ulen > datalen) return;
	plen = &data[2 + *ulen];
	if ((string::size_type)2 + *ulen + 1 + *plen > datalen) return;
	datalen = 2 + *ulen + 1 + *plen;
	user.get(data, datalen);
	if (*ver != 1 || *plen < 1) {
		flags.set(reject);
	} else {
		char *u = (char *)&data[2];
		char *p = (char *)&data[2 + *ulen + 1];
		socksdata.user.assign(u, *ulen);
		socksdata.pass.assign(p, *plen);
		if (flags[locked]) unlock(socksdata.pass);
		if (flags[locked]) unlock(socksdata.user);
		if (flags[locked]) flags.set(reject);
	}
	unsigned char myver = 1;
	unsigned char mystatus = (flags[reject]) ? 1 : 0;
	user.put(&myver, 1);
	user.put(&mystatus, 1);
	socksdata.auth = true;
	flags.set(ticked);
	if (mystatus != 0) user.close(true);
}

void proxy::socks5_receive()
{
	socksdata.rep = 0;
	if (user.insize() < sizeof(struct socks5header)) {
		return;
	}
	struct socks5header header;
	user.peek(&header, sizeof(struct socks5header));
	if (header.ver != 5) {
		socksdata.rep = 1; // general SOCKS server failure
		flags.set(reject);
	} else if (header.cmd != 1) {
		socksdata.rep = 7; // command not supported
		flags.set(reject);
	} else {
		socksdata.rep = (flags[reject]) ? 1 : 0;
	}

	string addr;
	int port = 0;
	if (header.atyp == 3) {
		string::size_type need_len =
			sizeof(struct socks5header) + header.peek + 2;
		if (user.insize() < need_len) return;
		char *domainname = new char [header.peek];
		unsigned short nport;
		user.get(&header, sizeof(struct socks5header));
		user.get(domainname, header.peek);
		user.get(&nport, 2);
		addr.assign(domainname, header.peek);
		port = ntohs(nport);
		delete [] domainname;
	} else if (header.atyp == 1) {
		string::size_type need_len =
			(sizeof(struct socks5header) - 1) + 6;
		if (user.insize() < need_len) return;
		unsigned int ipv4;
		unsigned short nport;
		user.get(&header, sizeof(struct socks5header) - 1);
		user.get(&ipv4, 4);
		user.get(&nport, 2);
		in_addr inaddr;
		inaddr.s_addr = ipv4;
		addr = inet_ntoa(inaddr);
		port = ntohs(nport);
	} else if (header.atyp == 4) {
		string::size_type need_len =
			(sizeof(struct socks5header) - 1) + 18;
		if (user.insize() < need_len) return;
		unsigned char ipv6[16];
		unsigned short nport;
		user.get(&header, sizeof(struct socks5header) - 1);
		user.get(ipv6, 16);
		user.get(&nport, 2);
		if (socksdata.rep == 0) socksdata.rep = 8;
			// address type not supported
		flags.set(reject);
	} else {
		if (socksdata.rep == 0) socksdata.rep = 8;
		flags.set(reject);
		user.close(true);
	}
	if (!flags[reject]) {
		ircdata.server = addr;
		ircdata.port = port;
		reset_irc();
		irc.bind("", 0);
		irc.connect(ircdata.server, ircdata.port);
	}
	socksdata.received = true;
	flags.set(ticked);
}

void proxy::socks5_send()
{
	bool conn = (irc.status() == net::connected ||
			irc.status() == net::disconnecting);
	if (!flags[reject] && irc.status() == net::connecting) {
		return;
	} else if (!flags[reject] && conn) {
	} else {
		// should be one of:
		// 3 network unreachable
		// 4 host unreachable
		// 5 connection refused
		// 6 ttl expired
		// but is:
		// 1 general SOCKS server failure
		socksdata.rep = 1;
	}
	struct socks5header header;
	header.ver = 5;
	if (socksdata.rep == 0 && flags[reject]) socksdata.rep = 1;
		// general SOCKS server failure
	header.cmd = socksdata.rep;
	header.rsv = 0;
	header.atyp = 1;
	user.put(&header, sizeof(struct socks5header) - 1);
	sockaddr_in sin;
	irc.get_local(sin);
	unsigned int addr = sin.sin_addr.s_addr;
	unsigned short port = sin.sin_port;
	user.put(&addr, 4);
	user.put(&port, 2);
	socksdata.sent = true;
	flags.set(ticked);
	if (!flags[reject] && socksdata.rep == 0) {
		flags.set(io);
		flags.set(told_lock);
		flags.set(told_init);
		flags.set(told_hi);
	} else {
		flags.set(reject);
		user.close(false);
	}
}

void proxy::send_irc_auth()
{
	if (flags[reject] || !flags[io] || flags[locked] || flags[irc_auth])
		return;
	if (ircdata.nick.empty() || ircdata.user.empty())
		return;
	flags.set(ticked);
	string s;
	flags.set(irc_pass, !ircdata.pass.empty());
	if (flags[irc_pass]) {
		s = "PASS " + ircdata.pass;
		irc.putline(s);
		ircdata.pass.clear();
		flags.set(irc_pass);
	}
	s = "NICK " + ircdata.nick;
	irc.putline(s);
	s = "USER " + ircdata.user;
	irc.putline(s);
	flags.set(irc_auth);
}

void proxy::jump(string args)
{
	int port = 0;
	if (args.empty()) {
		args = ircdata.server;
		port = ircdata.port;
	}
	if (args.empty()) {
		args = cfg.irc_server;
		port = cfg.irc_port;
	}
	string::size_type i;
	while (i = args.find(':'), i != string::npos) args[i] = ' ';
	string addr = strpop(args);
	string portstr = strpop(args);
	string pass = strpop(args);
	if (!portstr.empty()) port = stoi(portstr);
	if (port < 1 || port > 65535) port = cfg.irc_port;
	string addrstr = '\"' + addr + ':' + itos(port) + '\"';
	if (!good_irc_server(addr, port)) {
		info("Can't connect to " + addrstr);
		end_info();
		return;
	}
	if (!pass.empty()) ircdata.pass = pass;
	ircdata.server = addr;
	ircdata.port = port;
	info("Connecting to " + addrstr + "...");
	end_info();
	reset_irc();
	irc.bind("", 0);
	irc.connect(ircdata.server, ircdata.port);
	flags.set(jumped);
}

void proxy::tell_lock()
{
	if (mode != mode_bnc) return;
	if (!flags[io] || !flags[locked] || flags[told_lock]) return;
	if (ircdata.nick.empty() || ircdata.user.empty()) return;
	flags.set(ticked);
	stringstream ss;
	if (!flags[told_hi]) {
		ss << APPNAME << ' ' << APPVERSION << '\n';
		flags.set(told_hi);
	}
	if (!ircdata.pass.empty()) {
		ss << "Wrong password, saving it for the IRC connection\n"
			INDENT "(use /quote PASS to clear it"
			" if you don't want this)\n";
	} else {
		ss << "No IRC server password\n";
	}
	ss << "Use /dirt pass <password> to authenticate\n"
		INDENT "(for some IRC clients you need to /quote dirt pass)\n";
	play_stream(ss, "", false);
	end_info();
	flags.set(told_lock);
}

void proxy::tell_init(bool disc)
{
	if (mode != mode_bnc) return;
	if (!flags[io] || flags[locked]) return;
	if (ircdata.nick.empty() || ircdata.user.empty()) return;
	flags.set(ticked);
	stringstream ss;
	if (disc) ss << "You have been disconnected from IRC\n\n";
	if (!flags[told_hi]) {
		ss << APPNAME << ' ' << APPVERSION << '\n';
		flags.set(told_hi);
	}
	bool autojumping = false;
	bool closing = false;
	if (flags[reject]) {
		ss << "Your connection was rejected by " << APPNAME << '\n';
		closing = true;
	} else if (disc && cfg.bnc_disconnect) {
		ss << "Good bye\n";
		closing = true;
	} else {
		if (cfg.bnc_auto_jump && !flags[jumped]) {
			ss << "Auto-jumping\n";
			autojumping = true;
		} else {
			ss << "Use /dirt jump [server] [port] [password]"
				" to connect to IRC\n"
				INDENT "(for some IRC clients you need to"
				" /quote dirt jump)\n";
		}
	}
	play_stream(ss, "", false);
	end_info();
	flags.set(told_init);
	if (closing) {
		disconnect_user();
	} else if (autojumping) {
		jump("");
	}
}

void proxy::tell_ver()
{
	play_memory(ax_asc, ax_asc_size, true);
	end_info();
	flags.set(told_ver);
}

void proxy::in_server(string &line)
{
	string prefix;
	string source, hostmask;
	string cmd;
	string args = line;
	if (args.substr(0, 1) == ":") {
		prefix = strpop(args);
		string::size_type i = prefix.find('!');
		if (i != string::npos) {
			source = prefix.substr(1, i - 1);
			hostmask = prefix.substr(i + 1);
		} else {
			source = prefix.substr(1, string::npos);
		}
	}
	cmd = uppercase(strpop(args));
	if (cmd == "PRIVMSG" || cmd == "NOTICE") {
		string target = strpop(args);
		if (target.empty()) return;
		if (args.substr(0, 1) == ":") args.erase(0, 1);
		string keyname;
		string args_pre;
		if (source == "-psyBNC" && cmd == "PRIVMSG"
				/*&& hostmask == "psyBNC@lam3rz.de"*/
				&& psybnc(args, args_pre, keyname)) {
		} else if ((is_chan(target))/* || source == "dircproxy"*/) {
			keyname = target;
		} else /*if (!hostmask.empty())*/ {
			keyname = source;
		}
		string dec;
		int ret = key::decode(keyname, args, dec);
		args.swap(dec);
		if (args.substr(0, 1) == "\x01" && args.substr(args.size() - 1, 1) == "\x01") {
			// don't add the prefix to incoming CTCPs (PRIVMSG) or CTCP replies (NOTICE)
		} else if (args.substr(0, 12) == "SENDCHANKEY ") {
		} else if (args.substr(0, 14) == "ACCEPTCHANKEY ") {
		} else {
			format_msg(args, ret);
		}
		if (cmd == "NOTICE" && args.substr(0, 12) == "SENDCHANKEY ") {
			strpop(args);
			string chan = lowercase(strpop(args));
			string chankey = args;
			receivechankey(source, hostmask, chan, chankey);
			return;
		} else if (cmd == "NOTICE" && args.substr(0, 14) == "ACCEPTCHANKEY ") {
			if (source.empty() || hostmask.empty()) return;
			string nick = lowercase(source);
			strpop(args);
			string chan = lowercase(strpop(args));
			if (!is_chan(chan)) return;
			info(nick+" has accepted the key for "+chan);
			end_info();
			return;
		} else if (cmd == "NOTICE" && args.substr(0, 7) == "DH1080_") {
			string dhcmd = strpop(args);
			string theirpubkey = strpop(args);
			dh1080_receive(target, source, hostmask, dhcmd, theirpubkey);
			return;
		} else if (cmd == "PRIVMSG" && args.substr(0, 1) == "\x01" && args.substr(args.size() - 1, 1) == "\x01") {
			string s = args.substr(1, args.size() - 2);
			string req = strpop(s);
			string reply = in_ctcp(source, hostmask, req, s);
			if (!reply.empty()) {
				s = "NOTICE " + source + " :\x01" + req + " " + reply + "\x01";
				irc.putline(s);
				return;
			}
		}
		args.insert(0, cmd + " " + target + " :" + args_pre);
		if (!prefix.empty()) args.insert(0, prefix + " ");
		user.putline(args);
	} else if (cmd == "NICK" || cmd == "433" || cmd == "432") {
		user.putline(line);
		string nick = strpop(args);
		if (nick.substr(0, 1) == ":") nick.erase(0, 1);
		if (nick == "*") return;
		if (cmd != "NICK") { ircdata.nick = nick; return; }
		if (!hostmask.empty() &&
				lowercase(source) != lowercase(ircdata.nick))
			return;
		ircdata.nick = nick;
	} else if (cmd == "001" || cmd == "002" || cmd == "003" || cmd == "004") {
		user.putline(line);
		ircdata.network.erase();
	} else if (cmd == "005") {
		// 005 RPL_ISUPPORT
		user.putline(line);
		vector<string> isupport;
		tokenize(line, " ", isupport);
		vector<string>::iterator iter = (isupport.begin())++;
		while (iter != isupport.end()) {
			string &s = *iter;
			if (s.substr(0, 8) == "NETWORK=") {
				ircdata.network = s.substr(8);
				break;
			}
			iter++;
		}
	} else if (cmd == "376") {
		// 376 RPL_ENDOFMOTD
		user.putline(line);
		if (!flags[told_ver]) tell_ver();
	} else if (cmd == "TOPIC" || cmd == "332") {
		// 332 RPL_TOPIC
		string channel, setby;
		if (cmd == "TOPIC") {
			channel = strpop(args);
		} else if (cmd == "332") {
			setby = strpop(args);
			channel = strpop(args);
		}
		if (!is_chan(channel)) {
			info("Unrecognized topic format: " + line);
			end_info();
			user.putline(line);
			return;
		}
		if (args.substr(0, 1) == ":") args.erase(0, 1);
		string dec;
		int ret = key::decode(channel, args, dec);
		args.swap(dec);
		format_msg(args, ret);
		args.insert(0, channel + " :");
		if (!setby.empty()) args.insert(0, setby + " ");
		args.insert(0, cmd + " ");
		if (!prefix.empty()) args.insert(0, prefix + " ");
		user.putline(args);
	} else {
		user.putline(line);
	}
}

string proxy::in_ctcp(string source, string sourceaddr,
		string req, string args)
{
	//if (req == "DIRT") return APPNAME " " APPVERSION;
	return "";
}

void proxy::out_server(string &line)
{
	bool conn = (irc.status() == net::connected ||
			irc.status() == net::disconnecting);
	bool auth = (conn && flags[irc_auth]);
	string prefix;
	string cmd;
	string args = line;
	if (args.substr(0, 1) == ":") prefix = strpop(args);
	cmd = uppercase(strpop(args));
	if (cmd == "PRIVMSG" || cmd == "NOTICE") {
		if (!flags[irc_auth]) return;
		string target = strpop(args);
		out_message(prefix, cmd, target, args);
	} else if (cmd == "DIRT" || cmd == "/DIRT") {
		string dirtcmd = lowercase(strpop(args));
		out_dirt(dirtcmd, args);
	} else if (cmd == "PASS") {
		if (auth) { irc.putline(line); return; }
		string new_pass = args;
		if (new_pass.substr(0, 1) == ":") new_pass.erase(0, 1);
		if (flags[locked]) {
			unlock(new_pass);
			if (flags[locked]) ircdata.pass = new_pass;
		} else {
			ircdata.pass = new_pass;
		}
	} else if (cmd == "NICK") {
		string new_nick = args;
		if (new_nick.substr(0, 1) == ":") new_nick.erase(0, 1);
		if (!new_nick.empty()) ircdata.nick = new_nick;
		if (auth) { irc.putline(line); return; }
		flags.set(ticked);
	} else if (cmd == "USER") {
		if (auth) { irc.putline(line); return; }
		string new_user = args;
		if (new_user.substr(0, 1) == ":") new_user.erase(0, 1);
		if (!new_user.empty()) ircdata.user = new_user;
		flags.set(ticked);
	} else if (cmd == "QUIT") {
		if (auth) { irc.putline(line); return; }
		disconnect_user();
	} else {
		string dirtcmd = lowercase(cmd);
		if (dirtcmd.substr(0, 1) == "/") dirtcmd.erase(0, 1);
		const struct syntax *syntax;
		for (syntax = syntaxes; !syntax->cmd.empty(); syntax++) {
			if (dirtcmd != syntax->cmd) continue;
			if (syntax->subcmd) continue;
			out_dirt(dirtcmd, args);
			return;
		}
		if (!auth) return;
		irc.putline(line);
	}
}

void proxy::out_message(const string &prefix,
		const string &cmd, const string &target,
		string text)
{
	if (target.empty()) return;
	if (text.substr(0, 1) == ":") text.erase(0, 1);
	stringstream ss;
	if (text.substr(0, 10) == "+backward ") {
		text.erase(0, 10);
		backward(text);
	}
	if (text.substr(0, 3) == "+p ") {
		text.erase(0, 3);
		ss << text << '\n';
	} else if (cmd == "NOTICE" && text.substr(0, 4) == "DCC ") {
		// don't encrypt DCC notices
		ss << text << '\n';
	} else if (text.substr(0, 1) == "\x01" && text.substr(text.size() - 1, 1) == "\x01" &&
			text.substr(1, 7) != "ACTION ") {
		// don't encrypt outgoing CTCPs (PRIVMSG) or CTCP replies (NOTICE)
		// *except* for ACTIONs (/me ...)
		if (cmd == "NOTICE" && cfg.fake_version && text.substr(1, 8) == "VERSION ") {
			ircdata.client = text.substr(9, text.size() - 10);
			string reply = cfg.fake_version_reply;
			dirtstring(reply);
			if (!reply.empty()) text = "\x01VERSION " + reply + "\x01";
		}
		ss << text << '\n';
	} else {
		key::encode(target, text, ss);
	}
	while (getline(ss, text), ss.good()) {
		text.insert(0, cmd + " " + target + " :");
		if (!prefix.empty()) text.insert(0, prefix + " ");
		irc.putline(text);
	}
}

void proxy::out_dirt(string cmd, string args)
{
	while (cmd.substr(0, 6) == "/dirt ") cmd.erase(0, 6);
	while (cmd.substr(0, 1) == "/") cmd.erase(0, 1);
	if (flags[locked]) {
		if (cmd == "pass") {
			unlock(args);
		} else if (cmd == "disconnect") {
			disconnect_user();
		}
		return;
	}
	bool conn = (irc.status() == net::connected ||
			irc.status() == net::disconnecting);
	bool auth = (conn && flags[irc_auth]);
	if (cmd.empty()) {
		cmdlist("", false);
	} else if (cmd == "keys") {
		string target = strpop(args);
		if (target.empty()) {
			keylist(false);
		} else {
			showkey(target, false);
		}
	} else if (cmd == "showkey") {
		string target = strpop(args);
		if (target.empty()) { bad_syntax(cmd); return; }
		showkey(target, false);
	} else if (cmd == "keytext" || cmd == "showkeytext") {
		string target = strpop(args);
		if (target.empty()) { bad_syntax(cmd); return; }
		showkey(target, true);
	} else if (cmd == "setkey") {
		string target = strpop(args);
		string key = strpop(args);
		if (key.empty()) { bad_syntax(cmd); return; }
		int len = key.size();
		if (len < MIN_KEY_LEN || len > MAX_KEY_LEN) {
			bad_key_length(len);
			return;
		}
		key::addkey(target);
		key::setkey(target, key);
		showkey(target, false);
		key::save();
	} else if (cmd == "generatekey") {
		string target = strpop(args);
		if (target.empty()) { bad_syntax(cmd); return; }
		string length = strpop(args);
		int len = (length.empty()) ? 32 : stoi(length);
		if (len < MIN_KEY_LEN || len > MAX_KEY_LEN) {
			bad_key_length(len);
			return;
		}
		string data;
		for (int i = 0; i < len; i++) data += base64[(rand() % 64)];
		key::addkey(target);
		key::setkey(target, data);
		showkey(target, false);
		key::save();
	} else if (cmd == "removekey") {
		string target = strpop(args);
		if (target.empty()) { bad_syntax(cmd); return; }
		int ret = key::removekey(target);
		if (ret == key::success) {
			info("Removed the key for " + target);
			end_info();
			key::save();
		} else if (ret == key::bad_target) {
			bad_target(target);
		}
	} else if (cmd == "algorithm") {
		string target = strpop(args);
		string algo = strpop(args);
		if (algo.empty()) { bad_syntax(cmd); return; }
		int ret = key::setalgo(target, algo);
		if (ret == key::success) {
			showkey(target, false);
			key::save();
		} else if (ret == key::bad_target) {
			bad_target(target);
		} else if (ret == key::bad_algo) {
			bad_algo(algo);
		}
	} else if (cmd == "sendkey") {
		if (!auth) return;
		string target = lowercase(strpop(args));
		if (target.empty()) { bad_syntax(cmd); return; }
		if (is_chan(target)) { bad_syntax(cmd); return; }
		string channel = lowercase(strpop(args));
		if (channel.empty()) {
			dh1080_sendkey(target);
		} else {
			if (!is_chan(channel)) { bad_syntax(cmd); return; }
			string channel_key = key::getkey(channel);
			string target_key = key::getkey(target);
			if (channel_key.empty()) {
				info("You must have a key for " + channel + " first");
				end_info();
				return;
			}
			if (target_key.empty()) {
				info("You must have a key for " + target + " first");
				end_info();
				return;
			}
			if (!key::getstate(target)) {
				info("You must enable encryption for " + target + " first");
				end_info();
				return;
			}
			key::setalgo(target, algo_fish);
			key::setalgo(channel, algo_fish);
			string s = "SENDCHANKEY " + channel + " " + channel_key;
			string enc;
			key::encode(target, s, enc);
			enc.insert(0, "NOTICE " + target + " :");
			irc.putline(enc);
			info("Attempting to send the key for " + channel + " to " + target);
			end_info();
		}
	} else if (cmd == "acceptkey") {
		if (args.empty()) {
			bad_syntax(cmd);
			return;
		}
		acceptkey(args);
	} else if (cmd == "addalias") {
		string target = strpop(args);
		string alias = strpop(args);
		if (alias.empty()) { bad_syntax(cmd); return; }
		if (key::addalias(target, alias)) {
			showkey(alias, false);
			end_info();
			key::save();
		} else {
			bad_target(target);
		}
	} else if (cmd == "removealias") {
		string target = strpop(args);
		if (target.empty()) { bad_syntax(cmd); return; }
		if (key::removealias(target)) {
			info("Removed the alias " + target);
			end_info();
			key::save();
		} else {
			bad_target(target);
		}
	} else if (cmd == "copykey") {
		string source = strpop(args);
		string target = strpop(args);
		if (target.empty()) { bad_syntax(cmd); return; }
		string key = key::getkey(source);
		if (key.empty()) { bad_target(source); return; }
		key::addkey(target);
		key::setkey(target, key);
		info("The key for " + source + " is now also used for " + target);
		end_info();
		key::save();
	} else if (cmd == "encoff") {
		string target = strpop(args);
		if (target.empty()) { bad_syntax(cmd); return; }
		int ret = key::setstate(target, false);
		if (ret == key::bad_target) { bad_target(target); return; }
		info("Disabled encryption of outgoing text to " + target);
		end_info();
		key::save();
	} else if (cmd == "encon") {
		string target = strpop(args);
		if (target.empty()) { bad_syntax(cmd); return; }
		int ret = key::setstate(target, true);
		if (ret == key::bad_target) { bad_target(target); return; }
		info("Enabled encryption of outgoing text to " + target);
		end_info();
		key::save();
	} else if (cmd == "topic") {
		if (!auth) return;
		string channel = strpop(args);
		if (!is_chan(channel) || args.empty()) { bad_syntax(cmd); return; }
		string channel_key = key::getkey(channel);
		if (channel_key.empty()) {
			info("You must set a key for " + channel + " first");
			end_info();
			return;
		}
		bool enabled = key::getstate(channel);
		if (!enabled) key::setstate(channel, true);
		string s;
		key::encode(channel, args, s);
		if (!enabled) key::setstate(channel, false);
		s.insert(0, "TOPIC " + channel + " :");
		irc.putline(s);
	} else if (cmd == "help") {
		string cmd = lowercase(args);
		help(cmd);
	} else if (cmd == "commands") {
		cmdlist("", true);
	} else if (cmd == "version") {
		tell_ver();
	} else if (cmd == "load") {
		info("Reloading...");
		load_cfg();
		info("Loaded the configuration file");
		end_info();
	} else if (cmd == "status") {
		status();
	} else if (cmd == "jump") {
		jump(args);
	} else if (cmd == "disconnect") {
		if (conn) {
			string quit = "QUIT :";
			irc.putline(quit);
			disconnect_user();
		} else {
			disconnect_user();
		}
	} else if (cmd == "die") {
		dirt::deinit();
		exit(0);
	} else {
		bad_command(cmd);
	}
}

void proxy::acceptkey(string target)
{
	if (target.empty()) return;
	strlower(target);
	if (is_chan(target)) {
		if (recvkeys.find(target) == recvkeys.end()) {
			info("The key for "+target+" is unknown");
			end_info();
			return;
		}
		key::addkey(target);
		key::setkey(target, recvkeys[target].keydata);
		key::setalgo(target, algo_fish);
		string s = "NOTICE "+recvkeys[target].sender+
			" :ACCEPTCHANKEY " + target;
		irc.putline(s);
		stringstream ss;
		ss << "Accepted the key for " << target << " from "
			<< recvkeys[target].sender << '\n';
		play_stream_with_chan(ss, target);
		end_info();
		recvkeys.erase(target);
		key::save();
	} else {
		dh1080_acceptkey(target);
	}
}

void proxy::dh1080_sendkey(string nick)
{
	dhs[nick].reset();
	bool ok = dhs[nick].generate();
	string mypubkey;
	dhs[nick].get_public_key(mypubkey);
	if (!ok || mypubkey.empty()) {
		info("Could not generate a public key");
		end_info();
		return;
	}
	string s = "NOTICE " + nick + " :DH1080_INIT " + mypubkey;
	irc.putline(s);
	info("Attempting to exchange key with " + nick);
	end_info();
}

void proxy::dh1080_acceptkey(string nick)
{
	bool ok;
	ok = dhs[nick].generate();
	string mypubkey, secret;
	dhs[nick].get_public_key(mypubkey);
	if (!ok || mypubkey.empty()) {
		info("Could not generate a public key");
		end_info();
		return;
	}
	ok = dhs[nick].compute();
	dhs[nick].get_secret(secret);
	if (!ok || secret.empty()) {
		info("Failed to compute a shared secret for " + nick);
		info("Perhaps no request has been made?");
		end_info();
		return;
	}
	key::addkey(nick);
	key::setkey(nick, secret);
	key::setalgo(nick, algo_fish);
	dhs[nick].reset();
	info("Exchanged key with " + nick);
	end_info();
	key::save();
	string s = "NOTICE " + nick + " :DH1080_FINISH " + mypubkey;
	irc.putline(s);
}

void proxy::dh1080_receive(string mynick, string nick, string hostmask, string cmd, string theirpubkey)
{
	if (nick.empty()) return;
	string nick_orig = nick;
	strlower(nick);
	if (hostmask.empty()) {
		if (nick == "dircproxy") return;
		info("Ignored key exchange from " + nick + " (no user@host)");
		end_info();
		return;
	}
	dh_base64decode(theirpubkey);
	if (theirpubkey.size() != 135) {
		info(nick + " sent a bad public key");
		end_info();
		return;
	}
	string mypubkey, secret;
	if (cmd == "DH1080_INIT") {
		dhs[nick].reset();
		dhs[nick].set_their_key(theirpubkey);
		string s = "Received a DH1080 key exchange request from " + nick;
		if (!hostmask.empty())
			s += " (" + nick_orig + "!" + hostmask + ")";
		info(s);
		if (cfg.auto_accept) {
			info("Auto-accepting key...");
			acceptkey(nick);
		} else {
			info("Use /acceptkey " + nick + " to accept this key");
			end_info();
		}
	} else if (cmd == "DH1080_FINISH") {
		dhs[nick].get_public_key(mypubkey);
		if (mypubkey.empty()) {
			strlower(mynick);
			if (nick != mynick) {
				info(nick + " sent a DH1080 response, but no request has been made");
				end_info();
			}
			return;
		}
		dhs[nick].set_their_key(theirpubkey);
		bool ok = dhs[nick].compute();
		dhs[nick].get_secret(secret);
		if (!ok || secret.empty()) {
			info("Failed to compute a shared secret for " + nick);
			end_info();
			return;
		}
		key::addkey(nick);
		key::setkey(nick, secret);
		key::setalgo(nick, algo_fish);
		dhs[nick].reset();
		info("Exchanged key with " + nick);
		end_info();
		key::save();
	} else {
		info("Unrecognized DH1080 command from " + nick + ": " + cmd);
		end_info();
		return;
	}
}

void proxy::receivechankey(string nick, const string &hostmask,
		string chan, string chankey)
{
	if (nick.empty()) return;
	string nick_orig = nick;
	strlower(nick);
	if (hostmask.empty()) {
		if (nick == "dircproxy") return;
		info("Ignored channel key exchange from "+nick+
				" (no user@host)");
		end_info();
		return;
	}
	string keydata = strpop(chankey);
	if (!is_chan(chan) || keydata.empty() || !chankey.empty()) {
		info(nick+" sent an unrecognized SENDCHANKEY command");
		end_info();
		return;
	}
	class key *key_ptr = key::get(nick);
	if (key_ptr == 0) {
		info(nick+" tried to send a key for "+chan+", but you"
				" don't know his/her key");
		end_info();
		return;
	}
	recvkeys[chan].keydata = keydata;
	recvkeys[chan].sender = nick;
	stringstream ss;
	ss << "Received the key for " << chan << " from " << nick;
	if (!hostmask.empty())
		ss << " (" << nick_orig << "!" << hostmask << ")";
	ss << "\nUse /acceptkey " << chan << " to accept this key\n";
	play_stream_with_chan(ss, chan);
	end_info();
}

void proxy::info(string s)
{
	// 300 RPL_NONE (?)
	// 371 RPL_INFO
	// 374 RPL_ENDOFINFO
	string nick = ircdata.nick;
	if (nick.empty()) nick = "DirtUser";
	string line = ":dirt 371 " + nick + " :" + s;
	if (s.empty()) line += " ";
	user.putline(line);
}

void proxy::end_info()
{
	string nick = ircdata.nick;
	if (nick.empty()) nick = "DirtUser";
	string line = ":dirt 374 " + nick + " :";
	user.putline(line);
}

void proxy::play_file(string filename, bool codes)
{
	ifstream file(filename.c_str());
	play_stream(file, "", codes);
	file.close();
}

void proxy::play_memory(const char *buf, int len, bool codes)
{
	stringstream ss;
	ss.write(buf, len);
	play_stream(ss, "", codes);
}

void proxy::play_string(const string &s, const string prefix, bool codes)
{
	stringstream ss(s);
	play_stream(ss, prefix, codes);
}

void proxy::play_stream(istream &in, const string prefix, const bool codes)
{
	while (in.good()) {
		string line;
		getline(in, line);
		if (!in.good() && line.empty()) break;
		if (codes) {
			dirtstring(line);
		} else {
			remove_bad_chars(line);
		}
		info(prefix + line);
	}
}

void proxy::play_stream_with_chan(istream &in, const string chan)
{
	stringstream ss;
	while (in.good()) {
		string s;
		getline(in, s);
		if (!in.good() && s.empty()) break;
		ss << s << '\n';
	}
	while (ss.good()) {
		string s;
		getline(ss, s);
		if (!ss.good() && s.empty()) break;
		info(s);
	}
	ss.clear();
	ss.seekg(0);
	while (ss.good()) {
		string s;
		getline(ss, s);
		if (!ss.good() && s.empty()) break;
		s.insert(0, ":"+chan+" PRIVMSG "+chan+" :");
		user.putline(s);
	}
}

void proxy::dirtstring(string &s)
{
	remove_bad_chars(s);
	string::size_type begin, end;
	while (begin = s.find("$(", 0), end = s.find(")", begin), begin != string::npos && end != string::npos) {
		string code = lowercase(s.substr(begin, end-begin+1));
		s.erase(begin, end-begin+1);
		if (code == "$(version)") {
			s.insert(begin, APPVERSION);
		} else if (code == "$(program)") {
			s.insert(begin, APPNAME);
		} else if (code == "$(author)") {
			s.insert(begin, APPAUTHOR);
		} else if (code == "$(webpage)") {
			s.insert(begin, APPURL);
		} else if (code == "$(irc_client)") {
			s.insert(begin, ircdata.client);
		} else if (code == "$(algorithms)") {
			algoiter_t i;
			string algostr;
			for (i = algos.begin(); i != algos.end(); i++) {
				if (i != algos.begin()) algostr += ", ";
				algostr += i->first;
			}
			s.insert(begin, algostr);
		}
	}
}

bool proxy::psybnc(string &args, string &args_pre, string &keyname)
{
	string::size_type begin, end, erase, npos = string::npos;
	if ((begin = args.find(" :(", 7)) == npos) return false;
	begin += 3;
	if ((end = args.find(") ", begin + 5)) == npos) return false; // n!u@h
	string mask = args.substr(begin, end - begin);
	if (mask.find(' ') != npos) return false;
	if ((erase = mask.find('@')) == npos) return false;
	mask.erase(erase);
	if ((erase = mask.find('!')) == npos) return false;
	mask.erase(erase);
	if (mask.empty()) return false;
	keyname = mask;
	args_pre = args.substr(0, end + 2);
	args.erase(0, args_pre.size());
	return true;
}

void proxy::cmdlist(std::string cmd, bool desc)
{
	bool all = cmd.empty();
	bool match = false;
	if (all) play_string(cmdlist_prefix, "", false);
	string indent;
	if (all && !desc) indent = INDENT;
	const struct syntax *syntax;
	bool first = true;
	for (syntax = syntaxes; !syntax->cmd.empty(); syntax++) {
		if (!all && cmd != syntax->cmd) continue;
		if (syntax->alias) {
			if (all) continue;
			cmdlist(syntax->alias, desc);
			return;
		}
		match = true;
		stringstream ss;
		if (all && desc && !first) ss << '\n';
		first = false;
		if (!all && desc) ss << "Usage: ";
		if (syntax->subcmd)
			ss << "/dirt ";
		else
			ss << "/";
		ss << syntax->cmd;
		if (syntax->params && syntax->params[0])
			ss << " " << syntax->params;
		ss << '\n';
		play_stream(ss, indent, false);
		if (desc) play_string(syntax->desc, indent+INDENT, true);
	}
	if (all) play_string(cmdlist_suffix, "", false);
	if (all || match) {
		end_info();
	} else {
		bad_command(cmd);
	}
}

void proxy::help(string cmd)
{
	while (cmd.substr(0, 6) == "/dirt ") cmd.erase(0, 6);
	while (cmd.substr(0, 1) == "/") cmd.erase(0, 1);
	if (cmd.empty()) cmd = "help";
	if (cmd == "all") {
		cmdlist("", true);
	} else {
		cmdlist(cmd, true);
	}
}

void proxy::bad_command(const string &cmd)
{
	info("Unrecognized command: " + cmd);
	info("Try /dirt for a list of commands");
	end_info();
}

void proxy::bad_syntax(const string &cmd)
{
	help(cmd);
}

void proxy::bad_target(const string &target)
{
	info("Unrecognized message target: " + target);
	end_info();
}

void proxy::bad_algo(const string &algo)
{
	info("Unrecognized encryption algorithm: " + algo);
	end_info();
}

void proxy::bad_key_length(const int len)
{
	info("The key is " + itos(len) +
		" characters long, that is outside the allowed range of " +
		itos(MIN_KEY_LEN) + " to " + itos(MAX_KEY_LEN));
	end_info();
	return;
}

void proxy::keylist(const bool reveal)
{
	stringstream ss;
	if (key::text(ss, reveal)) {
		info("Keys:");
		play_stream(ss, INDENT, false);
	} else {
		info("The key list is empty");
	}
	end_info();
}

void proxy::matchkeys(const string &pattern, const bool reveal)
{
	stringstream ss;
	if (pattern.size() > MAX_PATTERN_LEN
			|| pattern.find_first_of("*?") == string::npos) {
		bad_target(pattern);
		return;
	}
	if (key::text_wildcard(ss, reveal, pattern)) {
		info("Matching keys:");
		play_stream(ss, INDENT, false);
	} else {
		bad_target(pattern);
		return;
	}
	end_info();
}

void proxy::showkey(const string &target, const bool reveal)
{
	stringstream ss;
	bool match = key::text(ss, reveal, target);
	if (!match) {
		matchkeys(target, reveal);
		return;
	}
	info("Key:");
	play_stream(ss, INDENT, false);
	end_info();
}

void proxy::disconnect_user()
{
	string addr;
	int port;
	user.get_local(addr, port);
	string error = "ERROR :Closing Link: " + addr +
		" (Disconnected from " + APPNAME + ')';
	info("Good bye");
	end_info();
	user.putline(error);
	user.close(false);
}

void proxy::status()
{
	info("Current status:");
	stringstream ss;
	ss << "Bouncer ";
	status_listener(ss, bnc);
	ss << "SOCKS v4 proxy ";
	status_listener(ss, socks4);
	ss << "SOCKS v5 proxy ";
	status_listener(ss, socks5);
	ss << proxys.size() << " local connection";
	if (proxys.size() != 1) ss << "s";
	ss << '\n';
	list<class proxy *>::iterator iter;
	for (iter = proxys.begin(); iter != proxys.end(); iter++) {
		ss << '\n';
		status_proxy(ss, **iter);
	}
	play_stream(ss, INDENT, false);
	end_info();
}

/*static*/ void proxy::status_listener(ostream &out, class socket &sock)
{
	if (sock.status() == net::listening) {
		string addr;
		int port;
		sock.get_local(addr, port);
		out << "ONLINE at " << addr << ':' << port;
	} else {
		out << "OFFLINE";
	}
	out << '\n';
}

/*static*/ void proxy::status_proxy(ostream &out, class proxy &p)
{
	if (p.mode == mode_bnc) {
		out << "Bouncer";
	} else if (p.mode == mode_socks4) {
		out << "SOCKS v4 proxy";
	} else if (p.mode == mode_socks5) {
		out << "SOCKS v5 proxy";
	} else {
		out << "Unknown";
	}
	out << " connection\n" << INDENT;
	bool conn = (p.irc.status() == net::connected ||
			p.irc.status() == net::disconnecting);
	if (!conn) {
		out << "Not connected to IRC\n";
		return;
	}
	string laddr, raddr;
	int lport, rport;
	p.irc.get_local(laddr, lport);
	p.irc.get_remote(raddr, rport);
	if (laddr == "0.0.0.0" || laddr.empty()) laddr = "?";
	if (raddr == "0.0.0.0" || raddr.empty()) raddr = "?";
	out << laddr << ':' << lport << " -> " << raddr << ':' << rport;
	if (!p.ircdata.network.empty()) {
		out << " [" << p.ircdata.network << "]";
	}
	out << '\n' << INDENT;
	if (!p.flags[irc_auth] ||
			p.ircdata.nick.empty() || p.ircdata.user.empty()) {
		out << "Not logged in\n";
		return;
	}
	out << "Logged in: " << p.ircdata.nick;
	if (p.flags[irc_pass]) out << " *passworded*";
	out << '\n';
}

inline bool good_irc_server(const string &addr, int port)
{
	if (addr.empty()) return false;
	string ip = net::resolve(addr);
	if (ip == "127.0.0.1" && port == cfg.bouncer_port) return false;
	if (ip == "127.0.0.1" && port == cfg.socks4_port) return false;
	if (ip.empty()) return false;
	if (port < 1 || port > 65535) return false;
	return true;
}

inline bool is_chan(const string &s)
{
	return (s.substr(0, 1).find_first_of("#&") != string::npos);
}

inline void format_msg(string &msg, int key_ret)
{
	string text_prefix, text_suffix;
	if (key_ret == key::success) {
		text_prefix = cfg.safe_text_prefix;
		text_suffix = cfg.safe_text_suffix;
	} else if (key_ret == key::cut) {
		text_prefix = cfg.cut_text_prefix;
		text_suffix = cfg.cut_text_suffix;
	} else if (key_ret == key::bad_chars) {
		// ...
	}
	msg.insert(0, text_prefix);
	msg.append(text_suffix);
}
