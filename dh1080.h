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

#ifndef DH1080_H
#define DH1080_H

#include <openssl/dh.h>
#include <openssl/bn.h>
#include <string>
#include <map>

class dhclass {
public:
	dhclass();
	~dhclass();
	void reset();
	bool generate();
	bool compute();
	bool set_their_key(std::string &their_public_key);
	void get_public_key(std::string &s);
	void get_secret(std::string &s);
private:
	DH *dh;
	BIGNUM *theirpubkey;
	std::string secret;
};

struct recvkeystruct {
	std::string sender;
	std::string keydata;
};

extern std::map<std::string, dhclass> dhs;
extern std::map<std::string, recvkeystruct> recvkeys;

void dh_base64encode(std::string &s);
void dh_base64decode(std::string &s);

#endif
