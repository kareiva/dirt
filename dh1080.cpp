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

#include "dh1080.h"
#include "misc.h"
#include <openssl/sha.h>
#include <sstream>

using namespace std;

map<string, dhclass> dhs;
map<string, recvkeystruct> recvkeys;

string fish_prime_b64 = "++ECLiPSE+is+proud+to+present+latest+FiSH+release+featuring+even+more+security+for+you+++shouts+go+out+to+TMG+for+helping+to+generate+this+cool+sophie+germain+prime+number++++/C32L";

dhclass::dhclass()
{
	dh = 0;
	theirpubkey = 0;
}

dhclass::~dhclass()
{
	if (dh != 0) DH_free(dh);
	if (theirpubkey != 0) BN_free(theirpubkey);
}

void dhclass::reset()
{
	if (dh != 0) {
		DH_free(dh);
		dh = 0;
	}
	if (theirpubkey != 0) {
		BN_free(theirpubkey);
		theirpubkey = 0;
	}
	secret.erase();
}

bool dhclass::generate()
{
	if (dh != 0) return false;
	dh = DH_new();
	if (dh == 0) return false;
	if (dh->g != 0 || dh->p != 0) return false;
	dh->g = BN_new();
	dh->p = BN_new();
	BN_dec2bn(&dh->g, "2");
	string prime = fish_prime_b64;
	dh_base64decode(prime);
	if (prime.empty()) return false;
	BIGNUM *ret = BN_bin2bn((unsigned char *)prime.data(), prime.size(), dh->p);
	if (ret == 0 || dh->g == 0 || dh->p == 0) return false;
	int check, codes;
	check = DH_check(dh, &codes);
	if (check != 1 || codes != 0) return false;
	if (DH_generate_key(dh) != 1) return false;
	return true;
}

bool dhclass::compute()
{
	if (dh == 0) return false;
	if (dh->g == 0 || dh->p == 0) return false;
	int size = DH_size(dh);
	if (size != 135) return false;
	if (theirpubkey == 0) return false;
	unsigned char key[135];
	int num = DH_compute_key(key, theirpubkey, dh);
	if (num != size) return false;
	stringstream ss;
	ss.write((char *)key, num);
	secret = ss.str();
	return true;
}

bool dhclass::set_their_key(string &their_public_key)
{
	if (theirpubkey == 0) theirpubkey = BN_new();
	if (theirpubkey == 0) return false;
	BIGNUM *ret = BN_bin2bn((unsigned char *)their_public_key.data(), their_public_key.size(), theirpubkey);
	if (ret == 0 || theirpubkey == 0) return false;
	return true;
}

void dhclass::get_public_key(string &s)
{
	s.erase();
	if (dh == 0) return;
	if (dh->g == 0 || dh->p == 0) return;
	int size = DH_size(dh);
	if (size != 135) return;
	unsigned char key[135];
	BN_bn2bin(dh->pub_key, key);
	stringstream ss;
	ss.write((char *)key, size);
	s = ss.str();
	dh_base64encode(s);
}

void dhclass::get_secret(string &s)
{
	s.erase();
	if (secret.empty()) return;
	unsigned char sha_md[32];
	SHA256((unsigned char *)secret.data(), secret.size(), sha_md);
	stringstream ss;
	ss.write((char *)sha_md, 32);
	s = ss.str();
	dh_base64encode(s);
}

void dh_base64encode(string &s)
{
	string b64;
	base64encode(s, b64);
	if (b64.find('=', 0) == string::npos) {
		b64 += "A";
	} else {
		string::size_type pos;
		while (pos = b64.find('=', 0), pos != string::npos) b64.erase(pos, 1);
	}
	s.swap(b64);
}

void dh_base64decode(string &s)
{
	if (s.size() % 4 == 1 && s.substr(s.size() - 1, 1) == "A") s.erase(s.size() - 1, 1);
	while (s.size() % 4 != 0) s += "=";
	string plain;
	base64decode(s, plain);
	s.swap(plain);
}