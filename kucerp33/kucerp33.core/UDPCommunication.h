#pragma once

#include <cstdint>
#include <string>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <string_view>

#pragma comment(lib, "Ws2_32.lib")

namespace UDP
{
	struct Chunk;

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
		bool SendData(const void* data, size_t size);

		bool SendFile(const std::string& path);

	private:
		SOCKET mSocket = INVALID_SOCKET;
		sockaddr_in mTarget{};

		uint32_t counter = 0;
	};


	class Receiver
	{
	public:
		Receiver(uint16_t port);
		~Receiver();

		bool IsOk() const { return mSocket != INVALID_SOCKET; }
		bool ReceiveText(std::string& outText, std::string* outFromIp = nullptr, uint16_t* outFromPort = nullptr);

		uint32_t ParseFileData(const std::string& data, Chunk* output);

		bool RecieveFile();

	private:
		SOCKET mSocket = INVALID_SOCKET;
	};
}