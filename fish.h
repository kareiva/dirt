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

#ifndef FISH_H
#define FISH_H

#include <string>
#include <ios>

namespace fish {
	void init();
	int encode(std::string s, std::ostream &out, const std::string &key,
			const bool split);
	int decode(std::string s, std::string &out, const std::string &key);
}

namespace dummy {
	void init();
	int encode(std::string s, std::ostream &out, const std::string &key,
			const bool split);
	int decode(std::string s, std::string &out, const std::string &key);
}

extern const std::string fish_base64;

#endif
