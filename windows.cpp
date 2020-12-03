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

#define WIN32_LEAN_AND_MEAN
#define _WIN32_IE 0x0700
#define WMUSER_SYSTRAY (WM_USER + 1)
#include <winsock2.h>
#include <windows.h>
#include <time.h>
#include <shellapi.h>
#include <shlobj.h>

#include "define.h"
#include "platform.h"
#include "net.h"
#include "main.h"
#include "misc.h"
#include "cfg.h"
#include "resource.h"
#include <sstream>
#include <fstream>

using namespace std;

HWND mainwindow = 0;
HINSTANCE appinst = 0;
int appshowcmd = 0;
WORD cur_notify_icon = 0;
bool icon_busy = false;
UINT wm_quit_dirt = 0;
const char *wm_quit_dirt_text = "DirtPleaseQuitYourself";
HANDLE socket_thread_handle = 0;
HMENU menu = 0;
HANDLE si_mutex = 0;
const char *si_mutex_name = "shared_info";

bool use_confdir();
void run_file(string file);
void notify_icon(DWORD action, WORD icon);
DWORD WINAPI socket_thread(LPVOID);
LRESULT CALLBACK windowproc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR cmdline, int showcmd);

void platform::init()
{
	WNDCLASSEX wc;
	wc.cbSize = sizeof(wc);
	wc.style = 0;
	wc.lpfnWndProc = windowproc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = appinst;
	wc.hIcon = (HICON)LoadImage(appinst, MAKEINTRESOURCE(ICON_DIRT), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
	wc.lpszMenuName = 0;
	wc.lpszClassName = "main";
	wc.hIconSm = (HICON)LoadImage(appinst, MAKEINTRESOURCE(ICON_DIRT), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
	if (RegisterClassEx(&wc) == 0) platform::error("RegisterClassEx() failed");
	int x = 0, y = 0, w = 400, h = 300;
	DWORD style = WS_OVERLAPPEDWINDOW;
	RECT rect;
	if (SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0) != 0) {
		x = ((rect.right - rect.left) / 2) - (w / 2);
		y = ((rect.bottom - rect.top) / 2) - (h / 2);
	}
	mainwindow = CreateWindow("main", APPNAME, style, x, y, w, h, 0, 0, appinst, 0);
	if (mainwindow == 0) platform::error("Could not create main window");
	ShowWindow(mainwindow, SW_HIDE);
	menu = LoadMenu(appinst, MAKEINTRESOURCE(MENU_SYSTRAY));
	if (menu == 0) platform::error("Could not load the menu resource");
	notify_icon(NIM_ADD, ICON_OFFLINE);
	si_mutex = CreateMutex(0, FALSE, si_mutex_name);
	if (si_mutex == 0) platform::error("Could not create a mutex");
	socket_thread_handle = CreateThread(0, 0, socket_thread, 0, 0, 0);
	if (socket_thread_handle == 0) platform::error("Could not create the socket thread");
}

void platform::deinit()
{
	if (cur_notify_icon != 0) notify_icon(NIM_DELETE, 0);
	cur_notify_icon = 0;
	if (menu != 0) DestroyMenu(menu);
	menu = 0;
	if (si_mutex != 0) CloseHandle(si_mutex);
	si_mutex = 0;
}

void platform::tick()
{
	if (cur_notify_icon != ICON_ONLINE && cur_notify_icon != ICON_OFFLINE) return;
	unsigned long proxy_cnt;
	shared_info.lock();
	proxy_cnt = shared_info.proxy_cnt;
	shared_info.unlock();
	WORD icon = (proxy_cnt > 0) ? ICON_ONLINE : ICON_OFFLINE;
	if (icon != cur_notify_icon) notify_icon(NIM_MODIFY, icon);
}

void platform::error(string text)
{
	icon_busy = true;
	if (cur_notify_icon != 0) notify_icon(NIM_MODIFY, ICON_ERROR);
	MessageBox(0, text.c_str(), APPNAME, MB_OK | MB_TASKMODAL | MB_ICONERROR);
	deinit();
	exit(1);
}

void platform::msg(string text)
{
	icon_busy = true;
	MessageBox(0, text.c_str(), APPNAME, MB_OK | MB_TASKMODAL);
	icon_busy = false;
}

string platform::installdir()
{
	//HKEY hkey;
	//char data[MAX_PATH + 1];
	//DWORD data_size = MAX_PATH;
	//DWORD data_type;
	//if (RegOpenKey(HKEY_CURRENT_USER, "Software\\Dirt", &hkey) != ERROR_SUCCESS) return "";
	//if (RegQueryValueEx(hkey, "InstallDir", 0, &data_type, (BYTE *)data, &data_size) != ERROR_SUCCESS) return "";
	//data[data_size + 1] = 0;
	//RegCloseKey(hkey);
	//return data;
	char self[MAX_PATH+1];
	self[MAX_PATH] = 0;
	GetModuleFileName(0, self, MAX_PATH);
	string path = self;
	string::size_type len = path.find_last_of("\\/");
	path.erase(len);
	return path;
}

string platform::confdir()
{
	char path[MAX_PATH];
	if (!SHGetSpecialFolderPath(0, path, CSIDL_APPDATA, true)) return false;
	return (string)path+"\\Dirt";
}

void shared_info::lock()
{
	if (si_mutex == 0) return;
	if (WaitForSingleObject(si_mutex, INFINITE) == WAIT_FAILED)
		platform::error("Could not wait for mutex");
}

void shared_info::unlock()
{
	if (si_mutex == 0) return;
	if (ReleaseMutex(si_mutex) == 0) platform::error("Could not release mutex");
}

bool use_confdir()
{
	string dir = platform::confdir();
	if (SetCurrentDirectory(dir.c_str()) == 0) {
		if (CreateDirectory(dir.c_str(), 0) == 0) return false;
		if (SetCurrentDirectory(dir.c_str()) == 0) return false;
	}
	return true;
}

void run_file(string file)
{
	string dir = platform::installdir();
	if (!dir.empty()) file.insert(0, dir + '\\');
	ShellExecute(0, "open", file.c_str(), "", "", SW_SHOWNORMAL);
}

void notify_icon(DWORD action, WORD icon)
{
	NOTIFYICONDATA nid;
	nid.cbSize = sizeof(nid);
	nid.hWnd = mainwindow;
	nid.uID = 1;
	if (action == NIM_ADD || action == NIM_MODIFY) {
		nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
		nid.uCallbackMessage = WMUSER_SYSTRAY;
		nid.hIcon = (HICON)LoadImage(appinst, MAKEINTRESOURCE(icon), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
		string tip = APPNAME;
		memcpy(nid.szTip, tip.c_str(), tip.size() + 1);
		cur_notify_icon = icon;
	} else {
		nid.uFlags = 0;
	}
	Shell_NotifyIcon(action, &nid);
}

DWORD WINAPI socket_thread(LPVOID)
{
	dirt::program();
	return 0;
}

LRESULT CALLBACK windowproc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
	static UINT wm_taskbar_restart = 0;
	switch (message) {
	case WM_CREATE:
		wm_taskbar_restart = RegisterWindowMessage("TaskbarCreated");
		wm_quit_dirt = RegisterWindowMessage(wm_quit_dirt_text);
		break;
	case WM_CLOSE:
		ShowWindow(window, SW_HIDE);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	case WMUSER_SYSTRAY:
		switch (lparam) {
		case WM_LBUTTONUP:
		case WM_LBUTTONDBLCLK:
			if (icon_busy) return 0;
			//ShowWindow(window, SW_MINIMIZE);
			//ShowWindow(window, SW_RESTORE);
			return 0;
		case WM_RBUTTONUP:
			if (icon_busy) return 0;
			POINT pt;
			if (GetCursorPos(&pt) == 0) return 0;
			HMENU popup = GetSubMenu(menu, 0);
			if (popup == 0) return 0;
			SetForegroundWindow(window);
			WORD cmd = (WORD)TrackPopupMenuEx(popup, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, window, 0);
			PostMessage(window, WM_NULL, 0, 0);
			switch (cmd) {
			case CMD_EXIT:
				icon_busy = true;
				if (MessageBox(0, "Really quit?", APPNAME, MB_YESNO | MB_ICONWARNING) == IDYES) {
					dirt::deinit();
					exit(0);
				}
				icon_busy = false;
				return 0;
			case CMD_ABOUT:
				dirt::version_info();
				return 0;
			case CMD_UPGRADE:
				run_file("upgrade.exe");
				return 0;
			case CMD_MANUAL:
				run_file("Manual.txt");
				return 0;
			}
			return 0;
		}
		return 0;
	}
	if (message == wm_taskbar_restart && cur_notify_icon != 0) {
		notify_icon(NIM_DELETE, 0);
		notify_icon(NIM_ADD, cur_notify_icon);
	} else if (message == wm_quit_dirt) {
		dirt::deinit();
		exit(0);
	}
	return DefWindowProc(window, message, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR cmdline, int showcmd)
{
	appinst = inst;
	appshowcmd = showcmd;
	use_confdir();
	string params(cmdline);
	string exefn = "dirtirc"; // fix this later.
	string s;
	while (s = strpop(params), !s.empty()) {
		if (s == "-h" || s == "--help") {
			stringstream ss;
			ss << "Usage: " << exefn << " [option]\n\n";
			ss << "Options:\n";
			ss << "  -h, --help\tshow this help\n";
			ss << "  -v, --version\tshow version information\n";
			ss << "  -q, --quit\tquit all running instances of " APPNAME "\n";
			ss << "\n";
			ss << "Configuration file options:\n";
			ss << "  --template\tcreate a template file with default settings\n";
			ss << "  --upgrade\tupgrade the configuration, keep changes\n";
			ss << "  --reset\t\toverwrite the configuration\n";
			platform::msg(ss.str());
			exit(0);
		} else if (s == "-v" || s == "--version") {
			dirt::version_info();
			exit(0);
		} else if (s == "-q" || s == "--quit") {
			wm_quit_dirt = RegisterWindowMessage(wm_quit_dirt_text);
			SendMessage(HWND_BROADCAST, wm_quit_dirt, 0, 0);
			exit(0);
		} else if (s == "--template"
				|| s == "--def" || s == "--writedefault") {
			dirt::cfg_template();
			exit(0);
		} else if (s == "--upgrade") {
			dirt::cfg_upgrade();
			exit(0);
		} else if (s == "--reset") {
			dirt::cfg_reset();
			exit(0);
		} else {
			platform::error("Unrecognized option \"" + s + "\"\nTry \"" + exefn + " --help\" for more information.");
		}
	}
	dirt::init();
	MSG message;
	BOOL gmresult;
	while (gmresult = GetMessage(&message, 0, 0, 0), gmresult != 0 && gmresult != -1 && !dirt::dying()) {
		TranslateMessage(&message);
		DispatchMessage(&message);
	}
	if (gmresult == -1) platform::error("GetMessage() returned -1 (error)");
	dirt::deinit();
	exit(0);
}
