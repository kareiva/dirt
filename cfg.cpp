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

#include "cfg.h"
#include "ini.h"
#include "misc.h"
#include "define.h"
#include "platform.h"
#include <fstream>
#include <exception>

using namespace std;

/*extern*/ class cfg cfg;

cfg::cfg()
{
	reset();
}

cfg::~cfg()
{
}

void cfg::reset()
{
	password = "";
	password_format = "text";
	socks4 = true;
	socks4_port = 1088;
	socks5 = true;
	socks5_port = 10588;
	bouncer = true;
	bouncer_port = 6666;
	irc_server = "irc.efnet.org:6667";
	irc_port = 6667;
	bnc_auto_jump = false;
	bnc_disconnect = false;
	auto_accept = true;
	safe_text_prefix = IRC_COLOR_DARK_GREEN ">> " IRC_COLOR_NORMAL;
	safe_text_suffix = "";
	cut_text_prefix = IRC_COLOR_ORANGE ">> " IRC_COLOR_NORMAL;
	cut_text_suffix = "";
	fake_version = true;
	fake_version_reply = "$(irc_client)" IRC_COLOR_NORMAL
		" | Dirt $(version) -- $(webpage)";
}

void cfg::generate(string fn, class ini *ini)
{
	bool alloc = false;
	if (ini == 0) {
		try {
			ini = new class ini;
		}
		catch (...) {
			ini = 0;
		}
		if (ini == 0) throw bad_alloc();
		prepare_ini(*ini);
		alloc = true;
	}
	if (fn.empty()) fn = INIFILE;
	ofstream file(fn.c_str(), ios::out|ios::trunc);
	file << "; Dirt IRC proxy (" << APPURL << ")\n"
		"; Configuration for version " << APPVERSION << "\n;\n"
		"; Uncomment (remove the \";\") to change a setting and use"
		" \"/dirt load\" from\n"
		"; within the program to reload the settings.\n;\n"
		"; See the following commands:\n"
		//2345678|2345678|2345678|
		";   dirtirc --template  "
		"(create a template file with default settings)\n"
		";   dirtirc --upgrade   "
		"(upgrade the configuration, keep changes)\n"
		";   dirtirc --reset     "
		"(overwrite the configuration)\n\n";
	ini->generate(file);
	file.close();
	if (alloc) delete ini;
}

bool cfg::load(string fn, class ini *ini)
{
	bool err = false;
	bool alloc = false;
	if (ini == 0) {
		try {
			ini = new class ini;
		}
		catch (...) {
			ini = 0;
		}
		if (ini == 0) throw bad_alloc();
		prepare_ini(*ini);
		alloc = true;
	}
	if (fn.empty()) fn = INIFILE;
	ifstream file(fn.c_str(), ios::in);
	if (!file.good()) err = true;
	if (!err) ini->load(file);
	if (file.bad()) err = true;
	file.close();
	if (alloc) delete ini;
	return !err;
}

void cfg::upgrade(string fn)
{
	class ini oldini, newini;
	prepare_ini(newini);
	class cfg oldcfg;
	oldcfg.prepare_ini(oldini);
	oldcfg.load(fn, &oldini);

	// patch alpha 25 settings
	string a25_safe_text_prefix = IRC_COLOR_GREEN "> " IRC_COLOR_NORMAL;
	string a25_cut_text_prefix = IRC_COLOR_YELLOW "> " IRC_COLOR_NORMAL;
	string a25_fake_version_reply = "Dirt $(version) » $(webpage)";
	if (oldcfg.safe_text_prefix == a25_safe_text_prefix) {
		oldcfg.safe_text_prefix = safe_text_prefix;
	}
	if (oldcfg.cut_text_prefix == a25_cut_text_prefix) {
		oldcfg.cut_text_prefix = cut_text_prefix;
	}
	if (oldcfg.fake_version_reply == a25_fake_version_reply) {
		oldcfg.fake_version_reply = fake_version_reply;
	}

	ini::upgrade(oldini, newini);
	generate(fn, &newini);
}

void cfg::prepare_ini(class ini &ini)
{
	// sections/variables
	static const char *s_socks4 = "socks4";
	static const char *s_socks5 = "socks5";
	static const char *s_socks = "socks";
	static const char *s_bouncer = "bouncer";
	static const char *s_socks4_port = "socks4_port";
	static const char *s_socks5_port = "socks5_port";
	static const char *s_bouncer_port = "bouncer_port";
	static const char *s_jump = "jump";
	static const char *s_encryption = "encryption";
	static const char *s_extra_security = "extra_security";
	static const char *s_misc = "misc";
	static const char *s_status = "status";
	static const char *s_port = "port";
	static const char *s_irc_server = "irc_server";
	static const char *s_irc_port = "irc_port";
	static const char *s_auto_jump = "auto_jump";
	static const char *s_disconnect = "disconnect";
	static const char *s_auto_accept = "auto_accept";
	static const char *s_safe_text_prefix = "safe_text_prefix";
	static const char *s_safe_text_suffix = "safe_text_suffix";
	static const char *s_cut_text_prefix = "cut_text_prefix";
	static const char *s_cut_text_suffix = "cut_text_suffix";
	static const char *s_password = "password";
	static const char *s_password_format = "password_format";
	static const char *s_fake_version = "fake_version";
	static const char *s_fake_version_reply = "fake_version_reply";
	// descriptions
	static const char *s_socks4_desc =
		"Listen for SOCKS version 4 connections?";
	static const char *s_socks4_port_desc =
		"SOCKS version 4 port";
	static const char *s_socks5_desc =
		"Listen for SOCKS version 5 connections?";
	static const char *s_socks5_port_desc =
		"SOCKS version 5 port";
	static const char *s_bouncer_desc =
		"Listen for IRC connections (bouncer mode)?";
	static const char *s_bouncer_port_desc =
		"IRC connection bouncer port";
	static const char *s_auto_jump_desc =
		"Automatically jump (connect) to the default IRC server?";
	static const char *s_disconnect_desc =
		"Disconnect user when the IRC server disconnects?";
	static const char *s_irc_server_desc =
		"Default IRC server for /dirt jump";
	static const char *s_irc_port_desc =
		"Default IRC server port for /dirt jump <server>";
	static const char *s_auto_accept_desc =
		"Automatically accept keys for nick names?\n"
		"Note that channel keys are never auto-accepted.";
	static const char *s_password_desc =
		"Password will be required to connect, if specified.\n"
		"It must not contain spaces.";
	static const char *s_password_format_desc =
		"Password format can be one of: sha1 md5 text\n"
		"  sha1:   SHA-1 (160-bit) hash; "
		"unix \"echo -n password|openssl dgst -sha1\"\n"
		"  md5:    MD5 (128-bit) hash;   "
		"unix \"echo -n password|md5sum\"\n"
		"  text:   plain text password (protect dirt.ini)";
	// options
	ini.add(s_socks, s_socks5, false, socks5, s_socks5_desc);
	ini.add(s_socks, s_socks5_port, false, socks5_port, s_socks5_port_desc);
	ini.add(s_socks, s_socks4, false, socks4, s_socks4_desc);
	ini.add(s_socks, s_socks4_port, false, socks4_port, s_socks4_port_desc);
	ini.add(s_bouncer, s_bouncer, false, bouncer, s_bouncer_desc);
	ini.add(s_bouncer, s_bouncer_port, false, bouncer_port, s_bouncer_port_desc);
	ini.add(s_bouncer, s_auto_jump, false, bnc_auto_jump, s_auto_jump_desc);
	ini.add(s_bouncer, s_disconnect, false, bnc_disconnect, s_disconnect_desc);
	ini.add(s_jump, s_irc_server, false, irc_server, s_irc_server_desc);
	ini.add(s_jump, s_irc_port, false, irc_port, s_irc_port_desc);
	ini.add(s_encryption, s_auto_accept, false, auto_accept, s_auto_accept_desc);
	ini.add(s_encryption, s_safe_text_prefix, false, safe_text_prefix);
	ini.add(s_encryption, s_safe_text_suffix, false, safe_text_suffix);
	ini.add(s_encryption, s_cut_text_prefix, false, cut_text_prefix);
	ini.add(s_encryption, s_cut_text_suffix, false, cut_text_suffix);
	ini.add(s_extra_security, s_password, false, password, s_password_desc);
	ini.add(s_extra_security, s_password_format, false, password_format, s_password_format_desc);
	ini.add(s_misc, s_fake_version, false, fake_version);
	ini.add(s_misc, s_fake_version_reply, false, fake_version_reply);
	// option aliases
	ini.add(s_misc, s_safe_text_prefix, true, safe_text_prefix);
	ini.add(s_misc, s_safe_text_suffix, true, safe_text_suffix);
	ini.add(s_misc, s_cut_text_prefix, true, cut_text_prefix);
	ini.add(s_misc, s_cut_text_suffix, true, cut_text_suffix);
	ini.add(s_socks4, s_status, true, socks4);
	ini.add(s_socks4, s_port, true, socks4_port);
	ini.add(s_bouncer, s_irc_server, true, irc_server);
	ini.add(s_bouncer, s_irc_port, true, irc_port);
}
