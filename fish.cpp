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

#include "fish.h"
#include "key.h"
#include "misc.h"
#include <openssl/blowfish.h>
#include <ostream>
#include <vector>
#include <algorithm>

using namespace std;

union bf_data {
	struct {
		unsigned int left;
		unsigned int right;
	} lr;
	BF_LONG bf_long;
};

/*extern*/ const string fish_base64 = "./0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

vector<string> fish_prefix;
string dummy_prefix;

inline bool dircproxy(string &in, string &pp);
inline void fish_encrypt(string in, string &out, const string &key);
inline int fish_decrypt(string in, string &out, const string &key);

void fish::init()
{
	const string fish_str = "fish";
	fish_prefix.clear();
	fish_prefix.push_back("+OK");
	fish_prefix.push_back("mcps");
	if (algos.find(fish_str) != algos.end()) return;
	algos[fish_str];
	algos[fish_str].encode = fish::encode;
	algos[fish_str].decode = fish::decode;
	enc_prefixes.insert(enc_prefixes.end(),
			fish_prefix.begin(), fish_prefix.end());
	dummy::init();
}

int fish::encode(string in, ostream &out, const string &key, const bool split)
{
	string enc;
	const string::size_type maxlen = 256;
	while (split && in.size() > maxlen) {
		string substr = in.substr(0, maxlen/* - 3*/);
		in.erase(0, maxlen/* - 3*/);
		//substr += ">>>";
		//in.insert(0, ">>>");
		fish_encrypt(substr, enc, key);
		out << fish_prefix.at(0) << " " << enc << '\n';
	}
	fish_encrypt(in, enc, key);
	out << fish_prefix.at(0) << " " << enc << '\n';
	return key::success;
}

int fish::decode(string in, string &out, const string &key)
{
	string pp;
	dircproxy(in, pp);
	string prefix;
	string::size_type i = in.find(" ", 0);
	vector<string>::iterator iter = fish_prefix.end();
	if (i != string::npos) {
		prefix = in.substr(0, i);
		iter = find(fish_prefix.begin(), fish_prefix.end(), prefix);
	}
	if (prefix.empty() || iter == fish_prefix.end()) {
		out = pp + in;
		return key::plain_text;
	}
	string dec;
	int ret = fish_decrypt(in.substr(prefix.size() + 1), dec, key);
	if (ret == key::bad_chars) {
		out = pp + in;
	} else {
		out = pp + dec;
	}
	return ret;
}

void dummy::init()
{
	dummy_prefix = "+itsnotOK ";
	const string encname = "dummy";
	if (algos.find(encname) != algos.end()) return;
	algos[encname];
	algos[encname].encode = dummy::encode;
	algos[encname].decode = dummy::decode;
	enc_prefixes.push_back(dummy_prefix);
}

int dummy::encode(string in, ostream &out, const string &key, const bool split)
{
	string s = in;
	backward(s);
	out << dummy_prefix << s << '\n';
	return key::success;
}

int dummy::decode(string in, string &out, const string &key)
{
	string pp;
	dircproxy(in, pp);
	if (in.substr(0, dummy_prefix.size()) != dummy_prefix) {
		out = pp + in;
		return key::plain_text;
	}
	out = in.substr(dummy_prefix.size());
	backward(out);
	out.insert(0, pp);
	return key::success;
}

inline bool dircproxy(string &in, string &pp)
{
	if (in.substr(0, 1) != "[") return false;
	string::size_type pos = in.find("] ", 1);
	if (pos == string::npos) return false;
	pp = in.substr(0, pos + 2);
	in.erase(0, pp.size());
	if (numtok(in, " ") < 5) return true;
	vector<string> v;
	tokenize(in, " ", v);
	//string &nick = v[0];
	string &host = v[1];
	string &act1 = v[2];
	string &act2 = v[3];
	if (host.substr(0, 1) != "(") return true;
	if (host.substr(host.size()-1, 1) != ")") return true;
	if (act1 != "changed" || act2 != "topic:") return true;
	for (int i = 0; i < 4; i++) pp += strpop(in) + " ";
	return true;
}

inline void fish_encrypt(string in, string &out, const string &key)
{
	int datalen = in.size();
	if (datalen % 8 != 0) {
		datalen += 8 - (datalen % 8);
		in.resize(datalen, 0);
	}
	out.erase();
	BF_KEY bf_key;
	BF_set_key(&bf_key, key.size(), (unsigned char *)key.data());
	bf_data data;
	unsigned long i, part;
	unsigned char *s = (unsigned char *)in.data();
	for (i = 0; i < in.size(); i += 8) {
		data.lr.left = *s++ << 24;
		data.lr.left += *s++ << 16;
		data.lr.left += *s++ << 8;
		data.lr.left += *s++;
		data.lr.right = *s++ << 24;
		data.lr.right += *s++ << 16;
		data.lr.right += *s++ << 8;
		data.lr.right += *s++;
		BF_encrypt(&data.bf_long, &bf_key);
		for (part = 0; part < 6; part++) {
			out += fish_base64[data.lr.right & 0x3f];
			data.lr.right = data.lr.right >> 6;
		}
		for (part = 0; part < 6; part++) {
			out += fish_base64[data.lr.left & 0x3f];
			data.lr.left = data.lr.left >> 6;
		}
	}
}

inline int fish_decrypt(string in, string &out, const string &key)
{
	if (in.size() < 12) return key::bad_chars;
	int cut = in.size() % 12;
	if (cut > 0) {
		in.erase(in.size() - cut, cut);
	}
	if (in.find_first_not_of(fish_base64, 0) != string::npos)
		return key::bad_chars;
	out.erase();
	BF_KEY bf_key;
	BF_set_key(&bf_key, key.size(), (unsigned char *)key.data());
	bf_data data;
	unsigned long val, i, part;
	char *s = (char *)in.data();
	for (i = 0; i < in.size(); i += 12) {
		data.lr.left = 0;
		data.lr.right = 0;
		for (part = 0; part < 6; part++) {
			if ((val = fish_base64.find(*s++)) == string::npos)
				return key::bad_chars;
			data.lr.right |= val << part * 6;
		}
		for (part = 0; part < 6; part++) {
			if ((val = fish_base64.find(*s++)) == string::npos)
				return key::bad_chars;
			data.lr.left |= val << part * 6;
		}
		BF_decrypt(&data.bf_long, &bf_key);
		for (part = 0; part < 4; part++)
			out += (data.lr.left & (0xff << ((3 - part) * 8))) >> ((3 - part) * 8);
		for (part = 0; part < 4; part++)
			out += (data.lr.right & (0xff << ((3 - part) * 8))) >> ((3 - part) * 8);
	}
	remove_bad_chars(out);
	return (cut > 0) ? key::cut : key::success;
}
