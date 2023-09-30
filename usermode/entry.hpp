#pragma once
#include <sstream>
#include <wininet.h>
#include <iphlpapi.h>
#include <IPTypes.h>
#include <iphlpapi.h>
#include <shellapi.h>
#pragma comment(lib, "IPHLPAPI.lib")
#ifndef MAIN_H
#define MAIN_H

void system_no_output(std::string command)
{
	command.insert(0, "/C ");

	SHELLEXECUTEINFOA ShExecInfo = { 0 };
	ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	ShExecInfo.hwnd = NULL;
	ShExecInfo.lpVerb = NULL;
	ShExecInfo.lpFile = "cmd.exe";
	ShExecInfo.lpParameters = command.c_str();
	ShExecInfo.lpDirectory = NULL;
	ShExecInfo.nShow = SW_HIDE;
	ShExecInfo.hInstApp = NULL;

	if (ShellExecuteExA(&ShExecInfo) == FALSE)
		WaitForSingleObject(ShExecInfo.hProcess, INFINITE);

	DWORD rv;
	GetExitCodeProcess(ShExecInfo.hProcess, &rv);
	CloseHandle(ShExecInfo.hProcess);
}

std::string readFileIntoString(const std::string& path) {
	auto ss = std::ostringstream{};
	std::ifstream input_file(path);
	if (!input_file.is_open()) {
		input_file.close();
		input_file.open(path);
	}
	ss << input_file.rdbuf();
	return ss.str();
}

std::string GenerateHexString(int len) {
	srand(time(NULL));
	std::string str = "0123456789ABCDEF";
	std::string newstr;
	int pos;
	while (newstr.size() != len) {
		pos = ((rand() % (str.size() - 1)));
		newstr += str.substr(pos, 1);
	}
	return newstr;
}

#endif // MAIN_H