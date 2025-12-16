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

	constexpr long RECEIVER_TIMEOUT = 200 * 1000; // 200 ms
	constexpr long ACK_RECEIVER_TIMEOUT = 500 * 1000; // 500 ms

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

		bool SendAckOrNack(bool state, uint32_t seq);
		bool SendFileAckOrNack(bool state);
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
		bool ReceiveData(UDP::Chunk& data, bool& ack, std::string* outFromIp = nullptr, uint16_t* outFromPort = nullptr);

		bool ReceiveAckOrNack(uint32_t expectedSeq, int timeoutMs, bool& outIsNack);
		bool ReceiveAnyAckOrNack(uint32_t& expectedSeq, int timeoutMs, bool& outIsNack);
		bool ReceiveFileAckOrNack(int timeoutMs, bool& outIsNack);
	private:
		SOCKET mSocket = INVALID_SOCKET;
	};
}