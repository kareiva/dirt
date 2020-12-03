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

#ifndef CFG_H
#define CFG_H

#include <string>

class cfg {
public:
	bool socks4;
	int socks4_port;
	bool socks5;
	int socks5_port;
	bool bouncer;
	int bouncer_port;
	bool bnc_auto_jump;
	bool bnc_disconnect;
	std::string irc_server;
	int irc_port;
	bool auto_accept;
	std::string safe_text_prefix;
	std::string safe_text_suffix;
	std::string cut_text_prefix;
	std::string cut_text_suffix;
	std::string password;
	std::string password_format;
	bool fake_version;
	std::string fake_version_reply;

	cfg();
	~cfg();
	void reset();
	void generate(std::string fn, class ini *ini = 0);
	bool load(std::string fn, class ini *ini = 0);
	void upgrade(std::string fn);

protected:
	void prepare_ini(class ini &ini);
};

extern class cfg cfg;

#endif
