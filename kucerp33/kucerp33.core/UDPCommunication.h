#pragma once

#include <cstdint>
#include <string>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <string_view>

#pragma comment(lib, "Ws2_32.lib")

namespace UDP
{
	constexpr std::string_view DEBUG_IP = "127.0.0.1";
	constexpr std::string_view NTB_IP = "192.168.0.199";
	constexpr uint16_t DEBUG_PORT = 9000;

	constexpr uint32_t PACKET_MAX_LENGTH = 1024;

	class WindowsSocketInit
	{
	public:
		WindowsSocketInit();
		~WindowsSocketInit();

		bool IsOk() const { return mOk; }
	private:
		bool mOk = false;
	};


	class Sender
	{
	public:
		Sender(const std::string& ip, uint16_t port);
		~Sender();

		bool IsOk() const { return mSocket != INVALID_SOCKET; }
		bool SendText(const std::string& text);

	private:
		SOCKET mSocket = INVALID_SOCKET;
		sockaddr_in mTarget{};
	};


	class Reciever
	{
	public:
		Reciever(uint16_t port);
		~Reciever();

		bool IsOk() const { return mSocket != INVALID_SOCKET; }
		bool RecieveText(std::string& outText, std::string* outFromIp = nullptr, uint16_t* outFromPort = nullptr);

	private:
		SOCKET mSocket = INVALID_SOCKET;
	};
}