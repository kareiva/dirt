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

#ifndef MISC_H
#define MISC_H

#include <string>
#include <vector>

extern std::string base64;

std::string itos(int i, int base = 10);
int stoi(const std::string &s, int base = 10);

void tokenize(const std::string &s, std::string delim,
		std::vector<std::string> &v);
unsigned int numtok(const std::string &s, std::string delim);
std::string gettok(const std::string &s, std::string delim, unsigned int idx);

void strlower(std::string &s);
void strupper(std::string &s);
std::string lowercase(std::string s);
std::string uppercase(std::string s);

std::string strpop(std::string &s, char delim = ' ');

void remove_chars(std::string &s, std::string chars);
void remove_bad_chars(std::string &s);

void addtab(std::string &s, std::string add, int len, char fillchar = ' ');
void addtab(std::string &s, int add, int len, char fillchar = ' ');

void base64encode(std::string src, std::string &dest);
void base64decode(std::string src, std::string &dest);

bool wildcard(const std::string &str, const std::string &pattern);
bool wildcard(const char *str, const char *pattern);

void backward(std::string &text);

class arg {
private:
	int argc;
	const char **argv;
	int idx;
	std::string::size_type idx2;
		// -abc will return 3 times (0..2) with lhs -a, -b and -c
	int skip;
		// increase this to skip next argument(s)
		// parsing -a with argument should increase skip
	std::string lhs;
		// can be formatted a, -a, --abc or --abc=
	std::string rhs;
		// option argument
	bool no_more_opts;
public:
	void start(int argc, const char **argv);
	std::string next();
	std::string argument();
		// only use if the option returned by next() uses an argument
};

#endif
