#pragma once
#pragma once
#include <Windows.h>
#include <TlHelp32.h>
#include <filesystem>
#include <vector>
#include "xorstr.h"
#include "skCrypt.h"
#include <iostream>

void check_processes()
{
    std::vector<const wchar_t*> processes = {
    TEXT("ollydbg.exe"),
    TEXT("ProcessHacker.exe"),
    TEXT("tcpview.exe"),
    TEXT("autoruns.exe"),
    TEXT("autorunsc.exe"),
    TEXT("filemon.exe"),
    TEXT("procmon.exe"),
    TEXT("regmon.exe"),
    TEXT("procexp.exe"),
    TEXT("ImmunityDebugger.exe"),
    TEXT("Wireshark.exe"),
    TEXT("dumpcap.exe"),
    TEXT("HookExplorer.exe"),
    TEXT("ImportREC.exe"),
    TEXT("PETools.exe"),
    TEXT("LordPE.exe"),
    TEXT("SysInspector.exe"),
    TEXT("proc_analyzer.exe"),
    TEXT("sysAnalyzer.exe"),
    TEXT("sniff_hit.exe"),
    TEXT("windbg.exe"),
    TEXT("joeboxcontrol.exe"),
    TEXT("joeboxserver.exe"),
    TEXT("ResourceHacker.exe"),
    TEXT("ida.exe"),
    TEXT("ida64.exe"),
    TEXT("ida32.exe"),
    TEXT("x32dbg.exe"),
    TEXT("x64dbg.exe"),
    TEXT("Fiddler.exe"),
    TEXT("httpdebugger.exe"),
    TEXT("HTTP Debugger Windows Service (32 bit).exe"),
    TEXT("HTTPDebuggerUI.exe"),
    TEXT("HTTPDebuggerSvc.exe"),
    TEXT("cheatengine-x86_64.exe"),
    TEXT("cheatengine-x86_64-SSE4-AVX2.exe"),
    TEXT("Scylla.exe"),
    TEXT("KsDumper11.exe"),
    TEXT("KsDumper7.exe"),
    };

    for (auto process : processes)
    {
        if (auto handle = GetModuleHandleA((LPCSTR)process); handle)
        {
            std::wstring wprocess(process);
            std::string process_name(wprocess.begin(), wprocess.end());
            system("cls");
            std::cout << xorstr(" unallowed process or processes found, process: ").c_str() + process_name << std::endl;
            Sleep(5000);
            exit(1);
            *(uintptr_t*)nullptr = 0;
        }
    }
}

inline void window_check()
{
	if (LI_FN(FindWindowA).forwarded_safe_cached()(xorstr("ProcessHacker").c_str(), NULL)) exit(1);
	if (LI_FN(FindWindowA).forwarded_safe_cached()(xorstr("PROCEXPL").c_str(), NULL)) exit(1);
	if (LI_FN(FindWindowA).forwarded_safe_cached()(xorstr("dbgviewClass").c_str(), NULL)) exit(1);
	if (LI_FN(FindWindowA).forwarded_safe_cached()(xorstr("XTPMainFrame").c_str(), NULL)) exit(1);
	if (LI_FN(FindWindowA).forwarded_safe_cached()(xorstr("WdcWindow").c_str(), xorstr("Resource Monitor").c_str())) exit(1);
}