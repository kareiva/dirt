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

#include "platform.h"
#include "misc.h"
#include "net.h"
#include "main.h"
#include "cfg.h"
#include "define.h"
#include <pwd.h>
#include <sys/types.h>
#include <grp.h>
#include <iostream>
#include <fstream>
#include <sstream>

using namespace std;

const string rcdirname = ".dirtrc";
string workdir;
string homedir;
string rcdir;
bool unix_daemon = true;
bool daemonized = false;
bool want_quit = false;
bool want_reset = false;
bool want_template = false;
bool want_upgrade = false;
string want_user;
string want_group;

void args(int argc, const char **argv);
void help(const string &unixname);
void signal_handler(int signum);
void daemonize();
void set_umask();
void get_workdir();
void get_homedir();
void get_rcdir();
bool use_rcdir();
bool rcdir_loop();
void quit();
int main(int argc, const char **argv);

void platform::init()
{
}

void platform::deinit()
{
	if (daemonized) remove(PIDFILE);
}

void platform::tick()
{
}

void platform::error(string text)
{
	cerr << APPNAME << " error: " << text << endl;
	dirt::deinit();
	exit(1);
}

void platform::msg(string text)
{
	cout << text << endl;
}

string platform::installdir()
{
	return workdir;
}

string platform::confdir()
{
	return rcdir;
}

void shared_info::lock()
{
}

void shared_info::unlock()
{
}

void args(int argc, const char **argv)
{
	class arg arg;
	arg.start(argc, argv);
	string n;
	while ((n = arg.next()) != "") {
		if (n == "-b" || n == "--background") {
			unix_daemon = true;
		} else if (n == "-f" || n == "--foreground") {
			unix_daemon = false;
		} else if (n == "-u" || n == "--user=") {
			string a = arg.argument();
			if (a.empty()) platform::error("Option " + n +
					" requires an argument.");
			want_user = a;
		} else if (n == "-g" || n == "--group=") {
			string a = arg.argument();
			if (a.empty()) platform::error("Option " + n +
					" requires an argument.");
			want_group = a;
		} else if (n == "-q" || n == "--quit") {
			want_quit = true;

		} else if (n == "-v" || n == "--version") {
			dirt::version_info();
			exit(0);
		} else if (n == "-h" || n == "--help") {
			help(argv[0]);
			exit(0);
		} else if (n == "--template"
				|| n == "--def" || n == "--writedefault") {
			want_template = true;
		} else if (n == "--upgrade") {
			want_upgrade = true;
		} else if (n == "--reset") {
			want_reset = true;
		} else {
			platform::error("Unrecognized option \"" + n + "\"\n"
					"Try \"" + argv[0] + " --help\""
					" for more information.");
		}
	}
}

void help(const string &unixname)
{
	cout << "Usage: " << unixname << " [options]\n"
		"\nOptions:\n"
		//2345678|2345678|12345678|12345678|12345678|
		"  -b, --background       "
		"start as a daemon (default)\n"
		"  -f, --foreground       "
		"start as a normal user process\n"
		"  -u, --user=            "
		"start as specified user\n"
		"  -g, --group=           "
		"start as specified group\n"
		"  -q, --quit             "
		"quit daemon\n"
		"  -v, --version          "
		"show version information\n"
		"  -h, --help             "
		"show this help\n"
		"\nConfiguration file options"
		" (under ~/" << rcdirname << "/):\n"
		"      --template         "
		"create a template file with default settings\n"
		"      --upgrade          "
		"upgrade the configuration, keep changes\n"
		"      --reset            "
		"overwrite the configuration\n";
}

void signal_handler(int signum)
{
	dirt::deinit();
	if (signum == SIGINT || signum == SIGQUIT) {
		cout << APPNAME << " was closed by the user\n";
	} else if (signum == SIGTERM) {
	}
	exit(2);
}

void daemonize()
{
	pid_t pid;
	pid = fork();
	if (pid > 0) {
		cout << APPNAME << " was started in the background\n";
		exit(0);
	}
	if (pid != 0) exit(0);
	if (setsid() == -1) exit(0);
	daemonized = true;
	pid = getpid();
	ofstream pidfile(PIDFILE, ios::out | ios::trunc);
	pidfile << pid << '\n';
	pidfile.close();
	if (pidfile.fail())
		platform::error((string)"Could not create " + PIDFILE);
}

void set_umask()
{
	umask(S_IRGRP | S_IROTH | S_IWGRP | S_IWOTH);
}

void get_workdir()
{
	const int pathlen = 256;
	char path[pathlen + 1];
	*path = 0;
	if (getcwd(path, pathlen) == 0)
		platform::error("Could not determine the current directory");
	workdir = path;
}

void get_homedir()
{
	//homedir = getenv("HOME");
	struct passwd *pwd = getpwuid(getuid());
	if (!pwd) platform::error("Could not determine your home directory");
	homedir = string(pwd->pw_dir);
}

void get_rcdir()
{
	get_homedir();
	rcdir = homedir + '/' + rcdirname;
}

bool use_rcdir()
{
	if (chdir(rcdir.c_str()) == 0) return true;
	if (mkdir(rcdir.c_str(), 0700) != 0) return false;
	if (chdir(rcdir.c_str()) == 0) return true;
	return false;
}

bool rcdir_loop()
{
	// this is because it seems like the directory change is not
	// immediate in some cases (particularly with -u and -g).
	// it should never stall if the implementation conforms to the
	// specifications.
	const int pathlen = 256;
	char path[pathlen + 1];
	*path = 0;
	workdir.clear();
	homedir.clear();
	rcdir.clear();
	do { get_workdir(); } while (workdir.empty());
	while (homedir.empty() || rcdir.empty() || rcdir != path) {
		get_rcdir();
		if (!use_rcdir()) return false;
		if (getcwd(path, pathlen) == 0) return false;
	}
	if (workdir == path) return false;
	return true;
}

void quit()
{
	pid_t pid = 0;
	ifstream pidfile(PIDFILE, ios::in);
	if (pidfile.good()) {
		pidfile >> pid;
		pidfile.close();
	}
	if (pid == 0) platform::error((string)APPNAME+" is not running");
	if (kill(pid, 0) != -1) {
		kill(pid, SIGTERM);
		cout << APPNAME << " has been closed\n";
	} else {
		cout << "Could not quit " << APPNAME << '\n';
	}
	remove(PIDFILE);
	exit(0);
}

int main(int argc, const char **argv)
{
	args(argc, argv);
	if (!want_user.empty()) {
		struct passwd *pwd = getpwnam(want_user.c_str());
		if (!pwd) platform::error("Unrecognized user: " +
				want_user + '.');
		if (setuid(pwd->pw_uid) != 0)
			platform::error("No permissions to change user.");
	}
	if (!want_group.empty()) {
		group *grp = getgrnam(want_group.c_str());
		if (!grp) platform::error("Unrecognized group: " +
				want_group + '.');
		if (setgid(grp->gr_gid) != 0)
			platform::error("No permissions to change group.");
	}
	set_umask();
	if (!rcdir_loop())
		platform::error("Could not change directory to ~/" +
				rcdirname);
	if (want_quit) quit();
	if (want_template) {
		dirt::cfg_template();
		platform::msg((string)"Wrote the configuration template"
				" (" + INIFILE_DEFAULT + ").");
		exit(0);
	}
	if (want_upgrade) {
		dirt::cfg_upgrade();
		platform::msg((string)"The configuration has been upgraded to"
				" version " + APPVERSION + '.');
		exit(0);
	}
	if (want_reset) {
		dirt::cfg_reset();
		platform::msg((string)"The configuration has been reset to"
				" default settings.");
		exit(0);
	}
	pid_t pid = 0;
	ifstream pidfile(PIDFILE, ios::in);
	if (pidfile.good()) {
		pidfile >> pid;
		pidfile.close();
	}
	if (pid != 0) {
		if (kill(pid, 0) == -1) {
			remove(PIDFILE);
		} else {
			string s = (string)PIDFILE+" exists! ";
			s += (string)APPNAME + " might already be running. ";
			s += "Otherwise remove this file.";
			platform::error(s);
		}
	}
	if (unix_daemon) daemonize();
	set_umask();
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGQUIT, signal_handler);
	if (!unix_daemon) cout << APPNAME << " " << APPVERSION << '\n';
	dirt::init();
	dirt::program();
	dirt::deinit();
	exit(0);
}
