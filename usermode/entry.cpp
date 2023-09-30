#pragma once
#include <iostream>
#include <boost/asio.hpp>
#include <Windows.h>
#include <string>
#include <thread>
#include <TlHelp32.h>
#include <conio.h>
#include <chrono>
#include <search.h>
#include <iostream>
#include <cstdlib>
#include <future>
#include <fstream>
#include "render.hpp"
#include "comm/driver.hpp"
#include "protect/debugging.h"
#include "protect/obsidium/obsidium64.h"
#include "protect/blowfish/blowfish.h"
#include "ares-lol/auth.hpp"
#include "includes.hpp"
#include "spoof.hpp"
#include "entry.hpp"
#pragma comment (lib, "advapi32.lib")
#pragma comment(lib, "cryptlib.lib")
#pragma comment(lib, "Normaliz.lib")
#pragma comment(lib, "wldap32.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "Ws2_32.lib")
#ifndef MAIN_CPP
#define MAIN_CPP

using namespace boost::asio;
ares::session_ctx session_ctx;

void start_server()
{
	io_service service;
	ip::tcp::endpoint endpoint(ip::tcp::v4(), 1194);
	ip::tcp::acceptor acceptor(service, endpoint);
	ip::tcp::socket socket(service);
	acceptor.accept(socket);
	std::cout << " Client connected: " << socket.remote_endpoint() << std::endl;

	void* hash = nullptr;
	boost::system::error_code error;
	read(socket, boost::asio::buffer(&hash, 1), error);
	if (error)
	{
		std::cout << xorstr("Error, Contact support and check the servers status: ") << error.message() << std::endl;
		Sleep(5000);
		exit(-1);
	}
	
	// do something with the hash

	std::cout << " Received hashed string: " << std::dec << hash << std::endl;
	std::cout << " Streaming driver's image" << std::endl;
	Sleep(500);

	//replacment for the RDP
	//ares::secure_image_ctx image_ctx = session_ctx.module("96714749-f809-4f04-a62c-47692a59951c");
	//std::cout << xorstr(" image streamed!") << std::endl;	
	//std::cout << xorstr(" streamimg image data to client") << std::endl;
	//Sleep(1000);

	//std::vector<std::uint32_t> decrypted = image_ctx.decrypt();
	//boost::asio::write(socket, boost::asio::buffer(decrypted), error);
	//if (error)
	//{
	//	std::cout << xorstr("Error, Contact support and check the servers status: ") << error.message() << std::endl;
	//	Sleep(5000);
	//	exit(-1);
	//}
	//Sleep(500);

	//std::cout << xorstr(" Successfully sent image data to client") << std::endl;
	//std::cout << xorstr(" Exiting server..") << std::endl;
	//socket.release();
	//Sleep(-1);
	return;
}

void get_driver()
{
	io_service service;
	ip::tcp::endpoint endpoint(ip::address::from_string(xorstr("10.8.6.254")), 1194);
	ip::tcp::socket socket(service);

	std::cout << xorstr(" Starting server") << std::endl;
	std::thread t_server(start_server);
	t_server.detach();
	Sleep(2000);

	std::cout << " Connecting to the server.." << std::endl;
	boost::system::error_code error;
	socket.connect(endpoint, error);
	if (error)
	{
		std::cout << xorstr("Error, Contact support and check the servers status: ") << error.message() << std::endl;
		Sleep(5000);
		exit(-1);
	}

	Sleep(1000);
	std::cout << xorstr(" Connected to server successfully") << std::endl;
	std::cout << xorstr(" Sending hashed string ") << std::endl;
	Sleep(500);

	std::cout << " Hexadecimal hash: ";
	static void* hash = hash_string("download");
	boost::asio::write(socket, boost::asio::buffer(&hash, 1), error);
	if (error)
	{
		std::cout << xorstr("Error, Contact support and check the servers status: ") << error.message() << std::endl;
		Sleep(5000);
		exit(-1);
	}

	//std::cout << std::endl << xorstr(" Sent hashed string! ") << hash << std::endl;
	//std::cout << xorstr(" Waiting for server..") << std::endl;

	//std::vector<std::uint32_t> buffer;
	//size_t len = read(socket, boost::asio::buffer(buffer), error);
	//if (error)
	//{
	//	std::cout << xorstr("Error, Contact support and check the servers status: ") << error.message() << std::endl;
	//	Sleep(5000);
	//	exit(-1);
	//}
	//Sleep(2000);

	//std::cout << xorstr(" Successfully received driver's image") << std::endl;
	//std::cout << xorstr(" Image: ") << buffer.data() << std::endl;
	//std::cout << xorstr(" Image size: ") << buffer.size() << std::endl; Sleep(1000);
	//std::cout << xorstr(" Closing client") << std::endl;
	//socket.release();
	//return buffer;
}

int main()
{
	OBSIDIUM_VM_START;
	OBSIDIUM_ENC_START;

	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 0x2);
	std::thread security(security_loop);
	LI_FN(SetConsoleTitleA)("");

	system_no_output(xorstr("taskkill /F /T /IM FortniteClient-Win64-Shipping.exe")); // fortnite
	system_no_output(xorstr("taskkill/F /T /IM EpicGamesLauncher.exe")); // fortnite's launcher
	system_no_output(xorstr("taskkill/F /T /IM FortniteClient-Win64-Shipping_BE.exe")); // BE for fortnite
	system_no_output(xorstr("taskkill/F /T /IM FortniteClient-Win64-Shipping_EAC.exe")); // EAC for fortnite
	system_no_output(xorstr("taskkill/F /T /IM RiotClientServices.exe ")); // val
	system_no_output(xorstr("taskkill/F /T /IM SteamService.exe")); // steam
	system_no_output(xorstr("taskkill/F /T /IM vgtray.exe")); // vanguard

	if (WSADATA ws; WSAStartup(MAKEWORD(2, 2), &ws) != 0)
	{
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	const hostent* host_info = gethostbyname(xorstr("ares.lol").c_str());
	struct in_addr address;
	if (!host_info)
	{
		std::cout << xorstr(" No stable internet connection found.") << endl;
		Sleep(2000);
		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	int i = 0;
	bool works = false;
	while (host_info->h_addr_list[i] != 0)
	{
		address.s_addr = *(unsigned long*)host_info->h_addr_list[i++];
		if (_stricmp(inet_ntoa(address), xorstr("172.67.148.108").c_str()) == 1)
		{
			std::cout << xorstr(" Failed to connect.") << endl;
			Sleep(-1);
			exit(0);
			*(uintptr_t*)(0) = 0;
		}
	}

	std::cout << xorstr(" Connected to host!") << endl; Sleep(1500);
	std::cout << xorstr(" Connecting to auth") << std::endl;
	std::vector<std::uint32_t> app_encrypted = { 62412, 62363, 62413, 62406, 62415, 62414, 62411, 62363, 62418, 62361, 62407, 62413, 62406, 62418, 62411, 62366, 62365, 62408, 62418, 62407, 62365, 62366, 62407, 62418, 62413, 62414, 62407, 62366, 62409, 62413, 62409, 62362, 62409, 62361, 62414, 62366 };
	session_ctx = ares::connect(app_encrypted);
	std::cout << " Successfully connected to auth!" << std::endl;
	Sleep(2000);
login: { }
	LI_FN(system)("CLS");
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 0x5);
	ShowWindow(GetConsoleWindow(), SW_MAXIMIZE);
	std::cout << xorstr(R"(
      ___           ___           ___           ___                                  ___           ___                                  ___           ___                   
     /\  \         /\__\         /\  \         /\__\                                /\ \         /\__\                  _____         /\__\         /\__\                  
    _\:\  \       /:/ _/_       /::\  \       /:/ _/_                  ___          \:\  \       /:/ _/_                /::\  \       /:/ _/_       /:/ _/_         ___     
   /\ \:\  \     /:/ /\__\     /:/\:\__\     /:/ /\__\                /\__\          \:\  \     /:/ /\__\              /:/\:\  \     /:/ /\__\     /:/ /\  \       /\__\    
  _\:\ \:\  \   /:/ /:/ _/_   /:/ /:/  /    /:/ /:/ _/_              /:/  /      ___ /::\  \   /:/ /:/ _/_            /:/ /::\__\   /:/ /:/ _/_   /:/ /::\  \     /:/  /    
 \ \:\ \:\__\ /:/_/:/ /\__\ /:/_/:/__/___ /:/_/:/ /\__\            /:/__/      /\  /:/\:\__\ /:/_/:/ /\__\          /:/_/:/\:|__| /:/_/:/ /\__\ /:/_/:/\:\__\   /:/__/     
 \:\ \:\/:/  / \:\/:/ /:/  / \:\/:::::/  / \:\/:/ /:/  /           /::\  \      \:\/:/  \/__/ \:\/:/ /:/  /          \:\/:/ /:/  / \:\/:/ /:/  / \:\/:/ /:/  /  /::\  \     
  \:\ \::/  /   \::/_/:/  /   \::/~~/~~~~   \::/_/:/  /           /:/\:\  \      \::/__/       \::/_/:/  /            \::/_/:/  /   \::/_/:/  /   \::/ /:/  /  /:/\:\  \    
   \:\/:/  /     \:\/:/  /     \:\~~\        \:\/:/  /            \/__\:\  \      \:\  \        \:\/:/  /              \:\/:/  /     \:\/:/  /     \/_/:/  /   \/__\:\  \   
    \::/  /       \::/  /       \:\__\        \::/  /                  \:\__\      \:\__\        \::/  /                \::/  /       \::/  /        /:/  /         \:\__\  
     \/__/         \/__/         \/__/         \/__/                    \/__/       \/__/         \/__/                  \/__/         \/__/         \/__/           \/__/  PredictionFN )") << std::endl << std::endl;
	Sleep(500);
	auto t_1 = std::chrono::high_resolution_clock::now();
	std::string License;
	std::ifstream output_login(xorstr("key.txt"));
	std::string reg_file(xorstr("key.txt"));
	std::ofstream output_log;
	output_login.seekg(0, std::ios::end);
	if (output_login.tellg() == 0)
	{
		std::cout << xorstr(" Did not find any existing license") << std::endl;
		Sleep(800);

		std::cout << xorstr(" Enter license key: ");
		char character;
		while (std::cin.get(character) && character != '\n' && character != '\r')
		{
			if (character == '\b')
			{
				if (!License.empty()) {
					License.pop_back();
					std::cout << "\b \b";
				}
			}

			License.push_back(character);
		}

		if (License.empty() || License.length() < 1)
		{
			std::ofstream(reg_file, std::ios::trunc);
			if ((MessageBoxA(0, "Licnese is too short! Cancel or retry", "Error!", MB_RETRYCANCEL | MB_DEFBUTTON1 | MB_ICONSTOP | MB_SYSTEMMODAL | MB_SERVICE_NOTIFICATION)) == IDCANCEL) {
				Sleep(500);
				exit(1);
			}
			system("CLS");
			goto login;
		}

		output_log.open("key.txt");
		output_log << License;
		output_log.close();
	}
	else
	{
		output_login.seekg(0, std::ios::beg);
		std::cout << xorstr(" Found a existing license") << std::endl;
		Sleep(2000);

		License = readFileIntoString(reg_file);
	}

	if (ares::response_e response = session_ctx.authenticate(License); response == ares::valid)
	{
		std::cout << xorstr(" Successfully logged in!") << std::endl;
		Sleep(2000);
		system("CLS");
	}
	else
	{
		std::ofstream(reg_file, std::ios::trunc);
		switch (response)
		{
		case ares::hwid:
			std::cout << xorstr(" HWID does not match one on record.") << std::endl;
			Sleep(3000);
			exit(1);
			break;
		case ares::banned:
			std::cout << xorstr(" This license is banned") << std::endl;
			Sleep(3000);
			exit(1);
			break;
		case ares::expired:
			std::cout << xorstr(" This license is expired") << std::endl;
			Sleep(3000);
			exit(1);
			break;
		default:
			std::cout << xorstr(" This license is invalid") << std::endl;
			Sleep(3000);
			exit(1);
			break;
		}
	}
	ares::license_ctx license_ctx = session_ctx.license_ctx();
	auto t_C = std::chrono::high_resolution_clock::now();
	auto ms_int_ = std::chrono::duration_cast<std::chrono::seconds>(t_C - t_1);
	std::chrono::duration<double, std::milli> ms_double_ = t_C - t_1;
	SetConsoleDisplayMode(GetStdHandle(STD_OUTPUT_HANDLE), CONSOLE_WINDOWED_MODE, 0);
	ShowWindow(GetConsoleWindow(), SW_NORMAL);
	SetConsoleTitleA("PredictionCheats");
	std::cout << xorstr(" Duration: ") << license_ctx.duration() << " Days" << std::endl;
	std::cout << xorstr(" Expiry: ") << license_ctx.expiry() << std::endl;
	std::cout << xorstr(" HWID: ") << license_ctx.hwid() << std::endl;
	std::cout << xorstr(" Last Login: ") << license_ctx.lastLogin() << std::endl;
	std::cout << xorstr(" IP: ") << license_ctx.ip() << std::endl;
	std::cout << xorstr(" Elapsed Time: ") << ms_double_ << std::endl;
	Sleep(10000);
	system("cls");
	SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 0x2);
	get_driver();
	std::cout << xorstr(" Driver is loaded.") << endl;
	Sleep(-1);
	LI_FN(system)("CLS");

	std::cout << xorstr("\n Waiting for FortniteClient-Win64-Shipping.exe") << std::endl;
	while (HWND window = FindWindowA("UnrealWindow", "Fortnite  ")) {}
	std::cout << xorstr("\n Attaching to FortniteClient-Win64-Shipping.exe") << std::endl;
	module::process_id = driver::get_process_pid(TEXT("FortniteClient-Win64-Shipping.exe"));
	if (!module::process_id) {
		std::cout << xorstr(" Driver Error: Failed to get games PID please restart and remap driver") << endl;
		Sleep(5000);
		exit(1);
		*(uintptr_t*)nullptr = 0;
	}

	if (!driver::update(module::process_id)) {
		std::cout << xorstr(" Driver Error: Failed to attach to fortnite please restart and remap driver");
		Sleep(5000);
		exit(1);
		*(uintptr_t*)nullptr = 0;
	}

	module::image_base = driver::get_image_base("FortniteClient-Win64-Shipping.exe");
	if (!module::image_base) {
		std::cout << xorstr(" Driver Error: Failed to get games base address please restart and remap driver");
		Sleep(5000);
		exit(1);
		*(uintptr_t*)nullptr = 0;
	}

	if (!driver::initialize(module::image_base)) {
		std::cout << xorstr(" Driver Error: Failed to get CR3 please restart and remap driver");
		Sleep(5000);
		exit(1);
		*(uintptr_t*)nullptr = 0;
	}

	std::cout << xorstr(" Hooked to Fortnite! ") << std::endl;
	std::cout << xorstr(" Initiated prediction") << std::endl;
	std::cout << xorstr(" PredictionFN 5.2.0-25909622+++Fortnite+Release-25.00") << std::endl;
	spoof_init();

	std::thread(cache_actors).detach();
	std::thread(cache_levels).detach();
	Sleep(500);

	std::thread aimbot = std::thread(aimbot_thread);
	std::thread exploits = std::thread(execute_exploits);

	// if an error occured throw the execption back
	try
	{
		c_overlay* overlay = new c_overlay;
		overlay->fortnite_window = FindWindowA("UnrealWindow", "Fortnite  ");
		if (!overlay->fortnite_window)
			throw std::runtime_error(xorstr(" Failed to find fortnite"));

		// create canvas
		if (!create_window(overlay))
		{
			UnregisterClassW(overlay->wc.lpszClassName, overlay->wc.hInstance);
			DestroyWindow(overlay->Window);
			throw std::runtime_error(xorstr("Failed to setup window"));
		}

		// init backend
		if (!directx_init(overlay))
		{
			ImGui::DestroyContext();
			overlay->DestroyRenderTarget();
			throw std::runtime_error(xorstr(" Failed to setup backend"));
		}

		// main loop
		while (!main_loop())
		{
			overlay->DestroyRenderTarget();
			ImGui_ImplDX11_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
			DestroyWindow(overlay->Window);
			UnregisterClassW(overlay->wc.lpszClassName, overlay->wc.hInstance);
			// Might want to join the threads
			throw std::runtime_error(xorstr(" Something failed."));
		}
	}
	catch (const std::runtime_error& e)
	{
		std::cout << " Runtime error, Please contact support. " << e.what() << std::endl;
		Sleep(2000);

		OBSIDIUM_VM_END;
		OBSIDIUM_ENC_END;

		exit(-1);
		*(uintptr_t*)nullptr = 0;
	}

	std::cout << "Exiting.. Last Error: " << GetLastError() << ", Contact Support" << std::endl;
	Sleep(-1);
	return 0;
}
#endif // MAIN_CPP
