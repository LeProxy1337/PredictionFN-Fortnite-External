#pragma once
#include <Windows.h>
#include <iostream>
#include <vector>

namespace ares {
	enum status_e : std::uint32_t {
		online,
		offline
	};

	enum response_e : std::uint32_t {
		valid,
		invalid,
		hwid,
		expired,
		banned
	};

	std::size_t write_function(void* data, std::size_t size, std::size_t nmemb, void* userdata);

	class hardware_id_ctx {
	private:
		std::string m_computer_name;
		std::string m_cpu_id;

		std::string m_hash;

		bool acquire_serials();
	public:
		hardware_id_ctx();
		~hardware_id_ctx();

		std::string hash();
	};

	class app_ctx {
	private:
		std::string m_id;
		std::string m_name;
		ares::status_e m_status;

		std::uint32_t m_key;
	public:
		app_ctx(std::string id, std::string name, ares::status_e status);
		app_ctx();

		~app_ctx();

		ares::status_e status();
		std::string id();
		std::string name();
	};

	class license_ctx {
	private:
		std::string m_id;
		ares::app_ctx m_app;
		std::string m_hwid;
		std::string m_expiry;
		std::string m_lastLogin;
		bool m_banned;
		std::string m_ip;
		std::uint32_t m_duration;
		std::uint32_t m_status;
		std::string m_created_on;

		std::uint32_t m_key;
	public:
		license_ctx(std::string id, ares::app_ctx app, std::string hwid, std::string expiry, std::string lastLogin, bool banned, std::string ip, std::uint32_t duration, std::uint32_t status, std::string created_on);

		~license_ctx();

		std::string id();

		ares::app_ctx app();

		std::string hwid();

		std::string expiry();

		std::string lastLogin();

		bool banned();

		std::string ip();

		std::uint32_t duration();

		std::uint32_t status();

		std::string created_on();
	};

	class secure_image_ctx {
	private:
		std::vector<std::uint32_t> m_image;
		std::uint32_t m_key;
	public:
		secure_image_ctx(std::vector<std::uint32_t> image, std::uint32_t key);

		secure_image_ctx();

		~secure_image_ctx();

		std::vector<std::uint32_t> decrypt();
	};

	class session_ctx {
	private:
		std::string m_token;
		std::vector<std::uint32_t> m_public_key;
		std::vector<std::uint32_t> m_private_key;
		std::string m_license;

		std::uint32_t m_key;
		ares::app_ctx m_app;

		ares::license_ctx m_license_ctx;

	public:
		session_ctx(std::string token, std::vector<std::uint32_t> public_key, std::vector<std::uint32_t> private_key, ares::app_ctx app, std::uint32_t key);

		session_ctx();

		void set_license_ctx(ares::license_ctx license_ctx);

		ares::response_e authenticate(std::string license);

		~session_ctx();

		ares::license_ctx license_ctx();

		std::string token();

		ares::secure_image_ctx module(std::string id);

		std::string variable(std::string name);

		std::string license();

		ares::app_ctx app();
	};

	ares::session_ctx connect(std::vector<std::uint32_t> id);
}