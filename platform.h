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

#ifndef PLATFORM_H
#define PLATFORM_H

#include <string>

namespace platform {
	void init();
	void deinit();
	void tick();
	void error(std::string text);
	void msg(std::string text);
	std::string installdir();
	std::string confdir();
}

class shared_info {
public:
	shared_info();
	void lock();
	void unlock();

	bool dying;
	unsigned long proxy_cnt;
};

extern class shared_info shared_info;

#endif
