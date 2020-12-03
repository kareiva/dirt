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

#include "syntax.h"
#include "define.h"

using namespace std;

const struct syntax syntaxes[] = {
	{
		"keys", false, 0, "", "Lists all known keys."
	}, {
		"showkey", false, 0, "<target>",
		"Shows key information. Can use * and ? wildcards."
	}, {
		"showkeytext", false, 0, "<target>",
		"Reveals a key in plain text. Can use * and ? wildcards."
	}, {
		"keytext", false, "showkeytext", "", ""
	}, {
		"sendkey", false, 0, "<nick>",
		"Generates a key for a nick name using a secure key exchange"
		" algorithm.\n"
		"Currently, only 1080-bit Diffie Hellman key exchanges are"
		" supported."
	}, {
		"sendkey", false, 0, "<nick> <channel>",
		"Sends the key for a channel to somebody.\n"
		"Note that encryption must be enabled for his/her nick name"
		" first."
	}, {
		"acceptkey", false, 0, "<target>",
		"Accepts a received key."
	}, {
		"setkey", false, 0, "<target> <key>",
		"Sets the key for a channel or nick name manually."
	}, {
		"generatekey", false, 0, "<target> [length]",
		"Generates a randomized key."
	}, {
		"removekey", false, 0, "<target>",
		"Forgets the key. It also removes all known aliases."
	}, {
		"addalias", false, 0, "<nick> <alias>",
		"Adds an alias for a nick name or channel."
	}, {
		"removealias", false, 0, "<alias>",
		"Removes an alias."
	}, {
		"copykey", false, 0, "<from> <to>",
		"Copies all key information from/to a channel or nick"
		" name."
	}, {
		"algorithm", false, 0, "<target> <algorithm>",
		"Selects the encryption algorithm for a specific target.\n"
		"Supported algorithms: $(algorithms)"
	}, {
		"encoff", false, 0, "<target>",
		"Disables outgoing encryption for a specific target."
	}, {
		"encon", false, 0, "<target>",
		"Enables outgoing encryption for a specific target."
	}, {
		"topic", true, 0, "<channel> <text>",
		"Sets an encrypted topic on a channel."
	}, {
		"help", true, 0, "[command]",
		"Shows information about how to use a command.\n"
		"Try /dirt for a list of commands."
	}, {
		"commands", true, 0, "",
		"Shows verbose help for all commands."
	}, {
		"version", true, 0, "",
		"Shows version information."
	}, {
		"load", true, 0, "",
		"Reloads the configuration file."
	}, {
		"status", true, 0, "",
		"Shows the current connection status."
	}, {
		"jump", true, 0, "[server] [port] [password]",
		"Connects to an IRC server."
	}, {
		"disconnect", true, 0, "",
		"Disconnects from the IRC server."
	}, {
		"die", true, 0, "", "Quits " APPNAME "."
	}, {
		""
	}
};
///*extern*/ const int syntaxes_size = sizeof(syntaxes) / sizeof(struct syntax);
string cmdlist_prefix = "List of Commands:";
string cmdlist_suffix =
	"\n"
	"All commands can be used with /dirt to avoid duplicate (script)"
	" aliases.\n"
	"Use /dirt help <command> for more information about a command.";
