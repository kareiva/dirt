//
// bin2c
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version. See License.txt for more details.
//
// bin2c.cpp:
//   Converts any file to a C99/C++ variable.
//
//   Copyright (C) 2005-2013  Mathias Karlsson <tmowhrekf at gmail.com>
//

#include "misc.h"
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

enum errors_t {
	err_syntax = 1,
	err_io = 2,
	err_memory = 3,
	err_unknown = 4
};

int program();
int main(int argc, const char **argv);
int args(int argc, const char **argv);
int syntax(std::string opt, std::string error);
int io_error(std::string fn, std::ios *ios = 0);
int error(int err, const std::string &msg);

const char *title = "bin2c";
const char *self = 0;
const std::string c_fn_def = "output.cpp";
std::string in_fn, c_fn = c_fn_def, h_fn, var;
std::ios_base::openmode write_mode = std::ios::out | std::ios::trunc;
bool msg_info = true, msg_error = true;
const std::streamsize bufsize_min = 1;
const std::streamsize bufsize_max = 0x10000; // 64 kibibytes
const std::streamsize bufsize_def = 0x1000;
	// 4096 bytes (common cluster dividend/divisor)
std::streamsize bufsize = bufsize_def;

int program()
{
	std::string chars;
	std::string::size_type find, npos = std::string::npos;
	if (var.empty()) {
		find = in_fn.find_last_of("/\\");
		if (find != npos && find < in_fn.size() - 1) {
			var = in_fn.substr(find + 1);
		} else {
			var = in_fn;
		}
	}
	for (char ch = '0'; ch <= '9'; ch++) chars += ch;
	if (chars.find(var.substr(0, 1)) != npos) var.insert(0, "_");
	for (char ch = 'a'; ch <= 'z'; ch++) chars += ch;
	for (char ch = 'A'; ch <= 'Z'; ch++) chars += ch;
	chars += "_";
	while ((find = var.find_first_not_of(chars)) != npos) var[find] = '_';
	if (msg_info)
		std::cout << "Writing `" << var << "' from `" << in_fn <<
			"' to `" << c_fn << "'..." << std::endl;
	std::ifstream in_file(in_fn.c_str(), std::ios::in | std::ios::binary);
	if (in_file.fail()) return io_error(in_fn, &in_file);
	std::ofstream c_file(c_fn.c_str(), write_mode);
	if (c_file.fail()) return io_error(c_fn, &c_file);
	c_file << std::hex;
	c_file << "/*extern*/ const char *" << var << " =\n\t\"";
	unsigned char *buf = new unsigned char [bufsize];
	long cnt = 0;
	while (in_file.good()) {
		in_file.read(reinterpret_cast<char *>(buf), bufsize);
		if (in_file.bad()) return io_error(in_fn, &in_file);
		std::streamsize gcount = in_file.gcount();
		for (std::streamsize i = 0; i < gcount; i++) {
			if (cnt > 0 && cnt % 16 == 0) c_file << "\"\n\t\"";
			cnt++;
			if (cnt < 1) return io_error(in_fn);
			c_file << "\\x";
			if (buf[i] < 0x10) c_file << "0";
			c_file << (unsigned int)buf[i];
		}
	}
	delete [] buf;
	c_file << std::dec;
	c_file << "\";\n"
		"/*extern*/ int " << var << "_size = " << cnt << ";\n";
	c_file.close();
	in_file.close();
	if (!h_fn.empty()) {
		if (msg_info)
			std::cout << "Writing header file `" << h_fn
				<< "'..." << std::endl;
		std::ofstream h_file(h_fn.c_str(), write_mode);
		if (h_file.fail()) return io_error(h_fn, &h_file);
		h_file << "extern const char *" << var << ";\n"
			"extern int " << var << "_size;\n";
		h_file.close();
		if (h_file.fail()) return io_error(h_fn, &h_file);
	}
	if (c_file.fail()) return io_error(c_fn, &c_file);
	if (in_file.bad()) return io_error(in_fn, &in_file);
	if (msg_info) std::cout << "Completed.\n";
	return 0;
}

int main(int argc, const char **argv)
{
	self = argv[0];
	int ret = 0;
	std::string e_str;
	try {
		if (!(ret = args(argc, argv))) ret = program();
	}
	catch (std::bad_alloc &e) {
		ret = err_memory;
		e_str = e.what();
	}
	catch (std::exception &e) {
		ret = err_unknown;
		e_str = e.what();
	}
	catch (...) {
		ret = err_unknown;
	}
	if (!e_str.empty()) return error(ret, e_str);
	return ret;
}

int args(int argc, const char **argv)
{
	class arg arg;
	arg.start(argc, argv);
	std::string n;
	while ((n = arg.next()) != "") {
		if (n == "--input=" || n == "-i") {
			in_fn = arg.argument();
			if (in_fn.empty()) return syntax(n, "no file name");
		} else if (n == "--code=" || n == "-c") {
			c_fn = arg.argument();
			if (c_fn.empty()) return syntax(n, "no file name");
		} else if (n == "--header=" || n == "-h") {
			h_fn = arg.argument();
			if (h_fn.empty()) return syntax(n, "no file name");
		} else if (n == "--var=" || n == "-v") {
			var = arg.argument();
			if (var.empty()) return syntax(n, "no identifier");
		} else if (n == "--append" || n == "-a") {
			write_mode = std::ios::out | std::ios::app;
		} else if (n == "--silent" || n == "-s") {
			msg_info = false;
		} else if (n == "--ultrasilent" || n == "-S") {
			msg_info = false;
			msg_error = false;
		} else if (n == "--buf=" || n == "-b") {
			std::string a = arg.argument();
			bufsize = 0;
			std::stringstream ss;
			ss << a;
			ss >> bufsize;
			if (bufsize < bufsize_min ||
					bufsize > bufsize_max) {
				std::stringstream e;
				e << "must be between " << bufsize_min <<
					" and " << bufsize_max;
				return syntax(n + a, e.str());
			}
		} else if (n == "--help") {
			return syntax("", "");
		} else {
			return syntax(n, "");
		}
	}
	if (in_fn.empty()) return syntax("", "no input file specified");
	if (c_fn.empty()) return syntax("", "");
	return 0;
}

int syntax(std::string opt, std::string error)
{
	if (!msg_error) return err_syntax;
	if (!opt.empty()) {
		std::cerr << title << ": `" << opt << "': ";
		if (!error.empty()) std::cerr << error;
		else std::cerr << "unrecognized parameter";
		std::cerr << '\n';
	} else if (!error.empty()) {
		std::cerr << title << ": " << error << '\n';
	}
	if (!opt.empty() || !error.empty()) {
		std::cerr << "try `" << self << " --help'\n";
		return err_syntax;
	}
	std::cerr << title << " - "
		"Converts any file to a C99/C++ variable\n"
		"Copyright (C) 2005-2013  Mathias Karlsson "
		"<tmowhrekf at gmail.com>\n"
		"\nUsage: " << self << " <options>\n"
		"\nOptions:\n"
		//2345678|2345678|2345678|2345678|2345678|
		"  -i, --input=          "
		"file to read\n"
		"  -c, --code=           "
		"source code file to write (default: " << c_fn_def << ")\n"
		"  -h, --header=         "
		"source code header file to write (none by default)\n"
		"  -v, --var=            "
		"variable identifier (default is automatic)\n"
		"  -a, --append          "
		"append to existing files\n"
		"  -s, --silent          "
		"no information messages\n"
		"  -S, --ultrasilent     "
		"no information/warning/error messages\n"
		"  -b, --buf=            "
		"buffer size in bytes (default: " << bufsize_def << ")\n"
		"      --help            "
		"show this help\n";
	return err_syntax;
}

int io_error(std::string fn, std::ios *ios)
{
	if (!msg_error) return err_io;
	std::cerr << title << ": `" << fn << "': ";
	if (ios && ios->bad()) std::cerr << "read/write";
	else if (ios && ios->fail()) std::cerr << "access";
	else std::cerr << "unknown";
	std::cerr << " error on i/o operation\n";
	return err_io;
}

int error(int err, const std::string &msg)
{
	if (!msg_error) return err;
	std::cerr << title << ": ";
	if (err == err_memory) std::cerr << "out of memory";
	else std::cerr << "unknown error";
	if (!msg.empty()) std::cerr << " (" << msg << ')';
	std::cerr << '\n';
	return err;
}
