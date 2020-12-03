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

#include "ini.h"
#include "misc.h"
#include <sstream>
#include <fstream>
#include <algorithm>

using namespace std;

const string comment_chars = ";#";

ini::ini()
{
	reset();
}

void ini::reset()
{
	opts.clear();
	err = false;
}

bool ini::error()
{
	if (err) {
		err = false;
		return true;
	}
	return false;
}

void ini::add(const char *sect, const char *var, bool alias,
		std::string &s, const char *desc)
{
	add(sect, var, alias, 's', (void *)&s, desc);
}

void ini::add(const char *sect, const char *var, bool alias,
		int &i, const char *desc)
{
	add(sect, var, alias, 'i', (void *)&i, desc);
}

void ini::add(const char *sect, const char *var, bool alias,
		bool &b, const char *desc)
{
	add(sect, var, alias, 'b', (void *)&b, desc);
}

void ini::add(const char *sect, const char *var, bool alias,
		char type, void *data, const char *desc)
{
	class ini::opt opt;
	opt.sect = sect;
	opt.var = var;
	opt.alias = alias;
	opt.desc = desc;
	opt.type = type;
	opt.data = data;
	opt.comment = true;
	opts.push_back(opt);
}

void ini::load(string fn)
{
	if (fn.empty()) { err = true; return; }
	ifstream file(fn.c_str(), ios::in);
	if (!file.good()) { err = true; return; }
	load(file);
	if (file.bad()) { err = true; return; }
	file.close();
}

void ini::load(istream &in)
{
	string::size_type npos = string::npos;
	string sect;
	while (in.good()) {
		string line;
		getline(in, line);
		remove_bad_chars(line);
		string::size_type i;
		line.erase(0, line.find_first_not_of(' '));
		if (line.empty()) continue;
		if (line[0] == '[') {
			string s = strpop(line);
			if (s[s.size()-1] != ']') { err = true; continue; }
			sect = lowercase(s.substr(1, s.size()-2));
		} else if (comment_chars.find(line[0]) != npos) {
			continue;
		}
		i = line.find_first_not_of(' ');
		if (i != npos) line.erase(0, i);
		line.erase(0, line.find_first_not_of(' '));
		i = line.find('=');
		if (i != npos) {
			string var = lowercase(strpop(line, '='));
			class opt opt(sect.c_str(), var.c_str());
			opts_t::iterator found =
				std::find(opts.begin(), opts.end(), opt);
			if (found == opts.end()) { err = true; continue; }
			if (!found->set(line)) err = true;
		} else {
			if (!line.empty()) err = true;
		}
	}
}

void ini::generate(string fn)
{
	if (fn.empty()) { err = true; return; }
	ofstream file(fn.c_str(), ios::out|ios::trunc);
	generate(file);
	file.close();
}

void ini::generate(ostream &out)
{
	bool desc = false;
	string sect = "\x0a";
	opts_t::iterator i;
	for (i = opts.begin(); i != opts.end(); i++) {
		if (i->alias) continue;
		string nsect = i->sect;
		string var = i->var;
		string data;
		if (!i->get(data)) { err = true; continue; }
		if (desc) out << '\n';
		if (nsect != sect) {
			sect = nsect;
			if (i != opts.begin() && !desc) out << '\n';
			out << "[" << sect << "]\n";
			desc = false;
		}
		if ((i->desc != 0) && !string(i->desc).empty()) {
			if (!desc) out << '\n';
			desc = true;
		} else {
			desc = false;
		}
		if (desc) {
			stringstream ss(i->desc);
			while (ss.good()) {
				string s;
				getline(ss, s);
				if (!ss.good() && s.empty()) break;
				out << comment_chars[0] << ' ' << s << '\n';
			}
		}
		if (i->comment) out << comment_chars[0];
		out << var << "=" << data << '\n';
	}
}

/*static*/ void ini::upgrade(class ini &old_ini, class ini &new_ini)
{
	opts_t::iterator oi, ni;
	for (oi = old_ini.opts.begin(); oi != old_ini.opts.end(); oi++) {
		ni = std::find(new_ini.opts.begin(), new_ini.opts.end(), *oi);
		if (ni == new_ini.opts.end()) continue;
		if (ni->type != oi->type) continue;
		string old_data, new_data;
		oi->get(old_data);
		ni->get(new_data);
		if (old_data != new_data) {
			ni->set(old_data);
			ni->comment = false;
		}
	}
}

/*static*/ string ini::onoff(bool b)
{
	return string((b) ? "on" : "off");
}

/*static*/ bool ini::onoff(string s)
{
	const char *on[] = { "on", "1", "yes", "true", 0 };
	strlower(s);
	for (int i = 0; on[i]; i++) if (s == on[i]) return true;
	return false;
}

ini::opt::opt()
{
	static const char *empty = "";
	sect = empty;
	var = empty;
	alias = true;
	desc = 0;
	type = 0;
	data = 0;
	comment = true;
}

ini::opt::opt(const char *sect, const char *var)
{
	opt();
	this->sect = sect;
	this->var = var;
}

ini::opt::~opt()
{
}

ini::opt &ini::opt::operator = (const ini::opt &other)
{
	sect = other.sect;
	var = other.var;
	alias = other.alias;
	desc = other.desc;
	type = other.type;
	data = other.data;
	comment = other.comment;
	return *this;
}

bool ini::opt::operator == (const ini::opt &other)
{
	return (string(var) == string(other.var)
			&& string(sect) == string(other.sect));
}

bool ini::opt::operator < (const ini::opt &other)
{
	return (string(var) < string(other.var)
			|| string(sect) < string(other.sect));
}

bool ini::opt::get(string &s)
{
	switch (type) {
	case 's':
		s = *(string *)data;
		break;
	case 'i':
		s = itos(*(int *)data);
		break;
	case 'b':
		s = onoff(*(bool *)data);
		break;
	default:
		s = "";
		return false;
	}
	return true;
}

bool ini::opt::set(const string &s)
{
	switch (type) {
	case 's':
		*(string *)data = s;
		break;
	case 'i':
		*(int *)data = stoi(s);
		break;
	case 'b':
		*(bool *)data = onoff(s);
		break;
	default:
		return false;
	}
	return true;
}
