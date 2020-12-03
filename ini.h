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

#ifndef INI_H
#define INI_H

#include <ios>
#include <string>
#include <vector>

class ini {
protected:
	class opt;
	typedef std::vector<class ini::opt> opts_t;
	opts_t opts;
	bool err;
public:
	ini();
	void reset();
	bool error();
	void add(const char *sect, const char *var, bool alias,
			std::string &s, const char *desc = 0);
	void add(const char *sect, const char *var, bool alias,
			int &i, const char *desc = 0);
	void add(const char *sect, const char *var, bool alias,
			bool &b, const char *desc = 0);
	void add(const char *sect, const char *var, bool alias,
			char type, void *data, const char *desc);
	void load(std::string fn);
	void load(std::istream &in);
	void generate(std::string fn);
	void generate(std::ostream &out);
	static void upgrade(class ini &old_ini, class ini &new_ini);
	static std::string onoff(bool b);
	static bool onoff(std::string s);
};

class ini::opt {
	friend class ini;
protected:
	const char *sect, *var;
	bool alias;
	const char *desc;
	char type;
	void *data;
	bool comment;
public:
	opt();
	opt(const char *sect, const char *var);
	~opt();
	ini::opt &operator = (const ini::opt &other);
	bool operator == (const ini::opt &other);
	bool operator < (const ini::opt &other);
	std::string operator () ();
	bool get(std::string &s);
	bool set(const std::string &s);
};

#endif
