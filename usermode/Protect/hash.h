#pragma once
#include <openssl/sha.h>
#include <iostream>
#include <filesystem>

#pragma warning( disable : 4996 )

auto hash_string(std::string ctx)
{
	unsigned char hash[SHA256_DIGEST_LENGTH];

	SHA256_CTX sha256;

	SHA256_Init(&sha256);
	SHA256_Update(&sha256, ctx.c_str(), ctx.length());
	SHA256_Final(hash, &sha256);

	for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
	{
		std::cout << std::hex << std::setw(2)
			<< std::setfill('0') <<
			static_cast<int>(hash[i]);
	}

	return std::dec;
}
