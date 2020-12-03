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

#ifndef MAIN_H
#define MAIN_H

#define assume(expr) if (!(expr)) assume_error(__FILE__, __LINE__, #expr)

void assume_error(const char *file, const int line, const char *expr);

namespace dirt {
	void program();
	void init();
	void deinit();
	bool dying();
	void dying(bool b);
	void version_info();
	void cfg_template();
	void cfg_upgrade();
	void cfg_reset();
	void listen();
}

#endif
