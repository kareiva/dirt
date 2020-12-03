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

#include "key.h"
#include "misc.h"
#include "fish.h"
#include "define.h"
#include <stdlib.h>
#include <stdio.h>
#include <queue>
#include <set>
#include <sstream>
#include <fstream>

using namespace std;

/*extern*/ vector<string> enc_prefixes;

/*extern*/ algos_t algos;
/*extern*/ keys_t keys;
/*extern*/ nicks_t nicks;
/*extern*/ aliases_t aliases;

algoiter_t algoiter;
keyiter_t keyiter;
nickiter_t nickiter;
aliasiter_t aliasiter;

bool find(const std::string &search);
bool find_wildcard(const std::string &pattern);
bool get_nicks(const std::string &search, std::queue<std::string> &names);
bool get_aliases(const std::string &search, std::queue<std::string> &names);

#define EOFCHAR '\x1a'

/*static*/ bool key::save()
{
	bool ren = (rename(KEYFILE, KEYFILE BACKUPSUFFIX) == 0);
	ofstream file(KEYFILE, ios::out|ios::binary|ios::trunc);
	file << "Don't touch!\n" << EOFCHAR << '\n';
	file << APPNAME << '\n';
	file << APPVERSION << '\n';
	const string::size_type pwlen = 64;
	string pw;
	while (pw.size() < pwlen) pw += fish_base64[rand() % 64];
	file << pw << '\n';
	while (nicks.size() > MAX_KEYS) {
		nicks_t::iterator n = nicks.end();
		key::removekey((--n)->first);
	}
	file << itos(nicks.size()) << '\n';
	for (nicks_t::iterator n = nicks.begin(); n != nicks.end(); n++) {
		keys_t::iterator k = n->second;
		algos_t::iterator algo = k->data.algoiter;
		if (algo == algos.end()) continue;
		queue<string> alias;
		get_aliases(n->first, alias);
		if (alias.empty()) file << "-";
		while (!alias.empty()) {
			file << alias.front();
			if (alias.size() != 1) file << " ";
			alias.pop();
		}
		file << '\n';
		file << n->first << '\n';
		file << string((k->data.state) ? "on" : "off") << '\n';
		file << algo->first << '\n';
		stringstream ss;
		fish::encode(k->data.key, ss, pw, false);
		string key;
		getline(ss, key);
		file << key << '\n';
	}
	file.close();
	bool ret = false;
	if (file.good()) {
		remove(KEYFILE BACKUPSUFFIX);
		ret = true;
	} else if (ren) {
		remove(KEYFILE);
		rename(KEYFILE BACKUPSUFFIX, KEYFILE);
	}
	return ret;
}

/*static*/ bool key::load(string fn)
{
	if (fn.empty()) fn = KEYFILE;
	ifstream file(fn.c_str(), ios::in|ios::binary);
	if (!file.good()) return false;
	string line, pw, version, appname, num_s;
	getline(file, line);
	getline(file, line, EOFCHAR);
	getline(file, line);
	getline(file, appname);
	getline(file, version);
	getline(file, pw);
	getline(file, num_s);
	if (!file.good()) return false;
	if (lowercase(appname) != lowercase(APPNAME)) return false;
	if (num_s.empty()) return false;
	int num = stoi(num_s);
	if (num > MAX_KEYS) num = MAX_KEYS;
	for (int i = 0; i < num; i++) {
		string alias, target, algostr, key_enc, key_dec;
		bool state;
		if (!file.good()) return false;
		getline(file, alias);
		getline(file, target);
		target = lowercase(target);
		getline(file, line);
		state = (line == "on");
		getline(file, algostr);
		getline(file, key_enc);
		algos_t::iterator algo = algos.find(lowercase(algostr));
		if (algo == algos.end()) {
			algo = algos.begin();
			state = false;
		}
		fish::decode(key_enc, key_dec, pw);
		class key key;
		key.data.state = state;
		key.data.algoiter = algo;
		key.data.key = key_dec;
		keys.push_front(key);
		nicks[target] = keys.begin();
		if (alias == "-" || alias.empty()) continue;
		vector<string> v;
		tokenize(alias, " ", v);
		while (v.size() != 0) {
			aliases[lowercase(v.back())] = nicks.find(target);
			v.pop_back();
		}
	}
	if (file.bad()) return false;
	file.close();
	return true;
}

/*static*/ class key *key::get(const string &search)
{
	if (!find(search)) return 0;
	return &(*keyiter);
}

/*static*/ int key::encode(const string &search, const string &in, ostream &out)
{
	if (!find(search)) {
		out << in << '\n';
		return bad_target;
	}
	return keyiter->encode(in, out);
}

/*static*/ int key::encode(const string &search, const string &in, string &out)
{
	if (!find(search)) {
		out = in;
		return bad_target;
	}
	return keyiter->encode(in, out);
}

/*static*/ int key::decode(const string &search, const string &in, string &out)
{
	if (!find(search)) {
		out = in;
		return bad_target;
	}
	return keyiter->decode(in, out);
}

/*static*/ bool key::addalias(string search, string alias)
{
	strlower(search);
	strlower(alias);
 	string newnick = search;
	if (!find(search)) {
		search.swap(alias);
		if (!find(search)) return false;
	}
	nickiter_t n = nickiter;
	if (alias != nickiter->first) {
		if (find(alias)) removealias(alias);
		aliases[alias] = n;
	}
	string oldnick = n->first;
	if (oldnick != newnick) {
		queue<string> al;
		get_aliases(oldnick, al);
		al.push(oldnick);
		nicks[newnick] = nicks[oldnick];
		nickiter = nicks.find(newnick);
		while (!al.empty()) {
			aliases[al.front()] = nickiter;
			al.pop();
		}
		aliases.erase(newnick);
		nicks.erase(oldnick);
	}
	return true;
}

/*static*/ bool key::removealias(const string &search)
{
	if (!find(search)) return false;
	if (aliasiter != aliases.end()) {
		aliases.erase(aliasiter);
		return true;
	}
	queue<string> alias;
	get_aliases(search, alias);
	nicks.erase(nickiter);
	if (alias.empty()) {
		keys.erase(keyiter);
		return true;
	}
	string newnick = alias.front();
	nicks[newnick] = keyiter;
	aliases.erase(newnick);
	alias.pop();
	while (!alias.empty()) {
		aliases[alias.front()] = nickiter;
		alias.pop();
	}
	return true;
}

/*static*/ int key::addkey(const std::string &target)
{
	if (find(target)) return success;
	class key key;
	keys.push_front(key);
	nicks[lowercase(target)] = keys.begin();
	return success;
}

/*static*/ int key::removekey(const string &target)
{
	if (!find(target)) return bad_target;
	queue<string> alias;
	get_aliases(target, alias);
	nicks.erase(nickiter);
	keys.erase(keyiter);
	while (!alias.empty()) {
		aliases.erase(alias.front());
		alias.pop();
	}
	return success;
}

/*static*/ string key::getnick(const string &target)
{
	if (!find(target)) return "";
	return nickiter->first;
}

/*static*/ bool key::getstate(const string &target)
{
	if (!find(target)) return false;
	return keyiter->data.state;
}

/*static*/ string key::getalgo(const string &target)
{
	if (!find(target)) return "";
	if (keyiter->data.algoiter == algos.end()) return "";
	return keyiter->data.algoiter->first;
}

/*static*/ string key::getkey(const string &target)
{
	if (!find(target)) return "";
	return keyiter->data.key;
}

/*static*/ int key::setnick(const string &nick)
{
	string n = lowercase(nick);
	queue<string> names;
	if (!get_nicks(n, names)) return bad_target;
	if (nickiter->first == n) return success;
	aliases.erase(aliasiter);
	nicks.erase(nickiter);
	nicks[n] = keyiter;
	nickiter = nicks.find(n);
	while (!names.empty()) {
		if (names.front() != n) aliases[names.front()] = nickiter;
		names.pop();
	}
	return success;
}

/*static*/ int key::setstate(const string &target, const bool &state)
{
	if (!find(target)) return bad_target;
	keyiter->data.state = state;
	return success;
}

/*static*/ int key::setalgo(const string &target, const string &algo)
{
	if (!find(target)) return bad_target;
	algoiter = algos.find(lowercase(algo));
	if (algoiter == algos.end()) return bad_algo;
	keyiter->data.algoiter = algoiter;
	return success;
}

/*static*/ int key::setkey(const string &target, const string &keystr)
{
	if (!find(target)) return bad_target;
	keyiter->data.key = keystr;
	return success;
}

/*static*/ void key::text(ostream &out, const bool reveal, const nicks_t::iterator n)
{
	const string &nick = n->first;
	class key &key = *(n->second);
	string s;
	if (nick.size() <= 15) {
		addtab(s, nick, 16);
	} else {
		s += nick + " ";
	}
	addtab(s, key.data.state ? "on" : "off", 4);
	string algo;
	if (key.data.algoiter != algos.end()) algo = key.data.algoiter->first;
	addtab(s, algo, 6);
	addtab(s, itos(key.data.key.size()) + "c", 5);
	if (reveal) {
		s += key.data.key;
	} else {
		s += "<hidden>";
	}
	out << s << '\n';
	queue<string> alias;
	for (aliasiter = aliases.begin(); aliasiter != aliases.end(); aliasiter++) {
		if (aliasiter->second->first == n->first) alias.push(aliasiter->first);
	}
	if (alias.empty()) return;
	s = "  ";
	while (!alias.empty()) {
		if (s.size() + alias.front().size() >= 74) {
			out << s << '\n';
			s = "  ";
		}
		s += alias.front();
		if (alias.size() != 1) s += ", ";
		alias.pop();
	}
	if (!s.empty()) out << s << '\n';
}

/*static*/ bool key::text(ostream &out, const bool reveal, const string &target)
{
	if (!find(target)) return false;
	text(out, reveal, nickiter);
	return true;
}

/*static*/ bool key::text_wildcard(ostream &out, const bool reveal, string pattern)
{
	strlower(pattern);
	set<string> matches;
	set<string>::iterator m;
	nickiter_t n;
	aliasiter_t a;
	for (n = nicks.begin(); n != nicks.end(); n++) {
		if (wildcard(n->first, pattern)) {
			matches.insert(n->first);
		}
	}
	for (a = aliases.begin(); a != aliases.end(); a++) {
		if (wildcard(a->first, pattern)) {
			matches.insert(a->second->first);
		}
	}
	for (m = matches.begin(); m != matches.end(); m++) {
		text(out, reveal, *m);
	}
	return (matches.size() > 0);
}

/*static*/ bool key::text(ostream &out, const bool reveal)
{
	for (nickiter = nicks.begin(); nickiter != nicks.end(); nickiter++) text(out, reveal, nickiter);
	return (nicks.size() != 0);
}

key::key()
{
	data.state = true;
	data.algoiter = algos.find("fish");
	if (data.algoiter == algos.end()) data.algoiter = algos.begin();
}

struct key::data &key::operator ()()
{
	return data;
}

int key::encode(const string &in, ostream &out)
{
	if (data.algoiter == algos.end()) {
		out << in << '\n';
		return bad_algo;
	}
	struct algo &algo = data.algoiter->second;
	if (data.state) {
		return algo.encode(in, out, data.key, true);
	}
	out << in << '\n';
	return success;
}

int key::encode(const string &in, string &out)
{
	if (data.algoiter == algos.end()) {
		out = in;
		return bad_algo;
	}
	struct algo &algo = data.algoiter->second;
	if (data.state) {
		stringstream ss;
		int ret = algo.encode(in, ss, data.key, false);
		out.erase();
		getline(ss, out);
		return ret;
	}
	out = in;
	return success;
}

int key::decode(const string &in, string &out)
{
	if (data.algoiter == algos.end()) {
		out = in;
		return bad_algo;
	}
	struct algo &algo = data.algoiter->second;
	return algo.decode(in, out, data.key);
}

key::iter::iter()
{
	reset();
}

key::iter::iter(const string &name)
{
	reset();
	find(name);
}

bool key::iter::find(const string &name)
{
	reset();
	string search = lowercase(name);
	nick = nicks.find(search);
	if (nick == nicks.end()) {
		alias = aliases.find(search);
		if (alias == aliases.end()) return false;
		nick = alias->second;
	}
	key = nick->second;
	algo = key->data.algoiter;
	return (found = true);
}

bool key::iter::find_wildcard(const string &pattern)
{
	reset();
	string search = lowercase(pattern);
	nickiter_t n;
	aliasiter_t a;
	for (n = nicks.begin(); n != nicks.end() && nick == nicks.end(); n++) {
		if (wildcard(n->first, search)) nick = n;
	}
	if (nick == nicks.end()) {
		for (a = aliases.begin(); a != aliases.end() && alias == aliases.end(); a++) {
			if (wildcard(a->first, search)) alias = a;
		}
		if (alias == aliases.end()) return false;
		nick = alias->second;
	}
	key = nick->second;
	algo = key->data.algoiter;
	return (found = true);
}

void key::iter::reset()
{
	key = keys.end();
	algo = algos.end();
	nick = nicks.end();
	alias = aliases.end();
	found = false;
}

bool find(const string &search)
{
	string target = lowercase(search);
	algoiter = algos.end();
	keyiter = keys.end();
	nickiter = nicks.end();
	aliasiter = aliases.end();
	nickiter = nicks.find(target);
	if (nickiter == nicks.end()) {
		aliasiter = aliases.find(target);
		if (aliasiter == aliases.end()) return false;
		nickiter = aliasiter->second;
	}
	keyiter = nickiter->second;
	algoiter = (*keyiter)().algoiter;
	return true;
}

bool find_wildcard(const std::string &pattern)
{
	string target = lowercase(pattern);
	algoiter = algos.end();
	keyiter = keys.end();
	nickiter = nicks.end();
	aliasiter = aliases.end();
	nickiter_t n;
	aliasiter_t a;
	//nickiter = nicks.find(target);
	for (n = nicks.begin(); n != nicks.end() && nickiter == nicks.end(); n++) {
		if (wildcard(n->first, target)) nickiter = n;
	}
	if (nickiter == nicks.end()) {
		//aliasiter = aliases.find(target);
		for (a = aliases.begin(); a != aliases.end() && aliasiter == aliases.end(); a++) {
			if (wildcard(a->first, target)) aliasiter = a;
		}
		if (aliasiter == aliases.end()) return false;
		nickiter = aliasiter->second;
	}
	keyiter = nickiter->second;
	algoiter = (*keyiter)().algoiter;
	return true;
}

bool get_nicks(const string &search, queue<string> &names)
{
	if (!find(search)) return false;
	names.push(nickiter->first);
	aliases_t::iterator aliasiter_reset = aliasiter;
	for (aliasiter = aliases.begin(); aliasiter != aliases.end(); aliasiter++) {
		if (aliasiter->second == nickiter) names.push(aliasiter->first);
	}
	aliasiter = aliasiter_reset;
	return true;
}

bool get_aliases(const string &search, queue<string> &names)
{
	if (!find(search)) return false;
	aliases_t::iterator aliasiter_reset = aliasiter;
	for (aliasiter = aliases.begin(); aliasiter != aliases.end(); aliasiter++) {
		if (aliasiter->second == nickiter) names.push(aliasiter->first);
	}
	aliasiter = aliasiter_reset;
	return true;
}
