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

	constexpr uint16_t RECEIVER_PORT = 15000;
	constexpr uint16_t SEND_PORT = 14000;
	constexpr uint16_t RECEIVER_PORT_ACK = 15001;
	constexpr uint16_t SEND_PORT_ACK = 14001;

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
		bool SendData(const Chunk& chunk);
	private:
		SOCKET mSocket = INVALID_SOCKET;
		sockaddr_in mTarget{};
	};


	class Receiver
	{
	public:
		Receiver(uint16_t port);
		~Receiver();

		bool IsOk() const { return mSocket != INVALID_SOCKET; }
		bool ReceiveText(std::string& outText, std::string* outFromIp = nullptr, uint16_t* outFromPort = nullptr);
		bool ReceiveData(UDP::Chunk& data, std::string* outFromIp = nullptr, uint16_t* outFromPort = nullptr);

		bool SendAckOrNack(bool state, uint32_t seq, std::string ip, uint16_t port);
		bool ReceiveAckOrNack(uint32_t expectedSeq, int timeoutMs, bool& outIsNack);
	private:
		SOCKET mSocket = INVALID_SOCKET;
	};
}