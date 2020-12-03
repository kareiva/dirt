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

#include "misc.h"
#include <ctype.h>

using namespace std;

string base64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

string itos(int i, int base)
{
	if (base < 2 || base > 10+'Z'-'A'+1) return "";
	string pre;
	string s;
	if (i < 0) {
		pre = "-";
		i = 0-i;
	}
	do {
		int digit = i%base;
		char ch = (digit > 9 ? 'A'+digit-10 : '0'+digit);
		s.insert(0, 1, ch);
		i /= base;
	} while (i != 0);
	return pre + s;
}

int stoi(const string &s, int base)
{
	if (s.empty()) return 0;
	if (base < 2 || base > 10+'Z'-'A'+1) return 0;
	bool minus = false;
	int i = 0;
	string::size_type idx = 0;
	if (s[0] == '-') minus = true;
	if (s[0] == '-' || s[0] == '+') idx++;
	for (; idx < s.size(); idx++) {
		int ch = tolower(s[idx]);
		if (ch >= 'a' && ch <= 'z' && base > 10+ch-'a') {
			i = i*base+10+ch-'a';
		} else if (ch >= '0' && ch <= '9' && base > ch-'0') {
			i = i*base+ch-'0';
		} else {
			break;
		}
	}
	return (minus ? 0-i : i);
}

void tokenize(const string &s, string delim, vector<string> &v)
{
	v.clear();
	if (delim.empty() || s.empty()) return;
	string::size_type pos = 0, start = 0;
	do {
		pos = s.find(delim, start);
		v.push_back(s.substr(start, pos-start));
		start = pos+delim.size();
	} while (pos != string::npos);
}

unsigned int numtok(const string &s, string delim)
{
	if (delim.empty() || s.empty()) return 0;
	string::size_type pos = 0, start = 0;
	unsigned int cnt = 0;
	do {
		pos = s.find(delim, start);
		cnt++;
		start = pos+delim.size();
	} while (pos != string::npos);
	return cnt;
}

string gettok(const string &s, string delim, unsigned int index)
{
	if (delim.empty() || s.empty()) return "";
	if (index < 0 || index > s.size()) return s;
	string::size_type pos = 0, start = 0;
	unsigned int cnt = 0;
	do {
		pos = s.find(delim, start);
		cnt++;
		if (index == cnt-1) return s.substr(start, pos-start);
		start = pos+delim.size();
	} while (pos != string::npos);
	return "";
}

void strlower(std::string &s)
{
	string::size_type i, size = s.size();
	for (i = 0; i < size; i++) s[i] = (char)tolower(s[i]);
}

void strupper(std::string &s)
{
	string::size_type i, size = s.size();
	for (i = 0; i < size; i++) s[i] = (char)toupper(s[i]);
}

string lowercase(string s)
{
	strlower(s);
	return s;
}

string uppercase(string s)
{
	strupper(s);
	return s;
}

string strpop(string &s, char delim)
{
	string::size_type i = s.find(delim);
	string ret;
	if (i == string::npos) {
		ret.swap(s);
	} else {
		ret = s.substr(0, i);
		s.erase(0, i+1);
	}
	return ret;
}

void remove_chars(string &s, string chars)
{
	string::size_type i, npos = string::npos;
	while (i = s.find_first_of(chars), i != npos) s.erase(i, 1);
}

void remove_bad_chars(string &s)
{
	string::size_type i, npos = string::npos;
	while (i = s.find('\x00', 0), i != npos) s.erase(i, 1);
	while (i = s.find_first_of("\x0d\x0a"), i != npos) s.erase(i, 1);
}

void addtab(string &s, string add, int len, char fillchar)
{
	add.resize(len, fillchar);
	s += add;
}

void addtab(string &s, int add, int len, char fillchar)
{
	string add_str = itos(add);
	add_str.resize(len, fillchar);
	s += add_str;
}

void base64encode(string src, string &dest)
{
	string::size_type size = src.size();
	src.resize(size + (3 - (size % 3)), 0);
	dest.erase();
	unsigned int data;
	unsigned char *p = (unsigned char *)src.data();
	for (string::size_type i = 0; i < size; i += 3) {
		data = *p++ << 16;
		data += *p++ << 8;
		data += *p++;
		for (int part = 0; part < 4; part++) {
			if (part >= 2 && i + part > size) {
				dest += "=";
			} else {
				dest += base64[(data >> 18) & 63];
			}
			data = data << 6;
		}
	}
}

void base64decode(string src, string &dest)
{
	string::size_type npos = string::npos;
	dest.erase();
	if (src.find_first_not_of(base64 + "=") != npos) return;
	if (src.size() % 4 != 0) return;
	unsigned int data = 0, val;
	char *p = (char *)src.data();
	for (string::size_type i = 0; i < src.size(); i += 4) {
		if ((val = base64.find(*p++)) != npos) data = val << 18;
		if ((val = base64.find(*p++)) != npos) data += val << 12;
		if ((val = base64.find(*p++)) != npos) data += val << 6;
		if ((val = base64.find(*p++)) != npos) data += val;
		dest += (data >> 16) & 255;
		if (src[i + 2] != '=') dest += (data >> 8) & 255;
		if (src[i + 3] != '=') dest += data & 255;
	}
}

bool wildcard(const std::string &str, const std::string &pattern)
{
	return wildcard(str.c_str(), pattern.c_str());
}

bool wildcard(const char *s, const char *p)
{
	// ? matches one character
	// * matches zero or more characters
	while (*s) {
		if (!*p) return false;
		if (*p == '*') {
			while (*p == '*') p++;
			if (!*p) return true;
			while (*s) if (wildcard(s++, p)) return true;
			return false;
		}
		if (*s != *p && *p != '?') return false;
		p++;
		s++;
	}
	while (*p == '*') p++;
	return (!*p);
}

void backward(std::string &text)
{
	string s;
	for (string::size_type i = text.size(); i > 0; i--) s += text[i-1];
	text = s;
}

void arg::start(int argc, const char **argv)
{
	this->argc = argc;
	this->argv = argv;
	idx = 1;
	idx2 = 0;
	skip = 0;
	lhs.clear();
	rhs.clear();
	no_more_opts = false;
}

string arg::next()
{
	lhs.clear();
	rhs.clear();
	if (idx < 1) return lhs; // error
	if (idx >= argc) return lhs;
	string str(argv[idx]);
	if (str.empty()) return lhs; // error
	if (str[0] != '-' || str == "-" || no_more_opts) {
		lhs = str;
		idx++;
		idx2 = 0;
		skip = 0;
		return lhs;
	}
	if (str.substr(0, 2) != "--") {
		if (++idx2 == 1) skip = 0;
		if (idx2 >= str.size() || skip > 0) {
			idx += skip + 1;
			if (idx2 < str.size() && skip > 0) idx--;
			idx2 = 0;
			skip = 0;
			return next();
		}
		lhs = string() + str[0] + str[idx2];
		skip = 0;
		if (idx2 + 1 < str.size()) {
			rhs = str.substr(idx2 + 1);
			return lhs;
		}
		if (idx + 1 < argc) rhs = argv[idx + 1];
		return lhs;
	}
	idx++;
	idx2 = 0;
	skip = 0;
	if (str == "--") {
		no_more_opts = true;
		return next();
	}
	string::size_type sep = str.find('=');
	if (sep != string::npos && sep < str.size()) {
		lhs = str.substr(0, sep + 1);
		rhs = str.substr(sep + 1);
	} else {
		lhs = str;
	}
	return lhs;
}

string arg::argument()
{
	if (lhs.empty()) return lhs;
	if (rhs.empty()) return rhs;
	skip = 1;
	return rhs;
}
