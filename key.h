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

#ifndef KEY_H
#define KEY_H

#include <string>
#include <map>
#include <list>
#include <vector>
#include <ios>

typedef std::map<std::string, struct algo> algos_t;
typedef std::list<class key> keys_t;
typedef std::map<std::string, keys_t::iterator> nicks_t;
typedef std::map<std::string, nicks_t::iterator> aliases_t;

typedef algos_t::iterator algoiter_t;
typedef keys_t::iterator keyiter_t;
typedef nicks_t::iterator nickiter_t;
typedef aliases_t::iterator aliasiter_t;

struct algo {
	int (*encode)(std::string in, std::ostream &out, const std::string &key, const bool split);
	int (*decode)(std::string in, std::string &out, const std::string &key);
};

class key {
public:
	enum { success = 0, cut, plain_text, bad_algo, bad_chars, bad_target };
	struct data {
		bool state;
		algos_t::iterator algoiter;
		std::string key;
	};
	class iter;
	static bool save();
	static bool load(std::string fn);
	static class key *get(const std::string &search);
	static int encode(const std::string &search, const std::string &in, std::ostream &out);
	static int encode(const std::string &search, const std::string &in, std::string &out);
	static int decode(const std::string &search, const std::string &in, std::string &out);
	static bool addalias(std::string search, std::string alias);
	static bool removealias(const std::string &search);
	static int addkey(const std::string &target);
	static int removekey(const std::string &target);
	static std::string getnick(const std::string &target);
	static bool getstate(const std::string &target);
	static std::string getalgo(const std::string &target);
	static std::string getkey(const std::string &target);
	static int setnick(const std::string &nick);
	static int setstate(const std::string &target, const bool &state);
	static int setalgo(const std::string &target, const std::string &algo);
	static int setkey(const std::string &target, const std::string &keystr);
	static void text(std::ostream &out, const bool reveal, const nicks_t::iterator n);
	static bool text(std::ostream &out, const bool reveal, const std::string &target);
	static bool text_wildcard(std::ostream &out, const bool reveal, std::string pattern);
	static bool text(std::ostream &out, const bool reveal);
	key();
	struct data &operator ()();
	int encode(const std::string &in, std::ostream &out);
	int encode(const std::string &in, std::string &out);
	int decode(const std::string &in, std::string &out);
private:
	struct data data;
};

class key::iter {
public:
	keyiter_t key;
	algoiter_t algo;
	nickiter_t nick;
	aliasiter_t alias;
	bool found;
	iter();
	iter(const std::string &name);
	bool find(const std::string &name);
	bool find_wildcard(const std::string &pattern);
private:
	void reset();
};

extern std::vector<std::string> enc_prefixes;
extern algos_t algos;
extern keys_t keys;
extern nicks_t nicks;
extern aliases_t aliases;

#endif
