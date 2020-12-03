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

#ifndef SYNTAX_H
#define SYNTAX_H

#include <string>

struct syntax {
	const std::string cmd;
	bool subcmd;
	const char *alias;
	const char *params;
	const char *desc;
};

extern const struct syntax syntaxes[];
//extern const int syntaxes_size;
extern std::string cmdlist_prefix;
extern std::string cmdlist_suffix;

#endif
