#include "UDPCommunication.h"
#include "SmartDebug.h"
#include "FileTransfer.h"

using namespace UDP;

#define _WINSOCK_DEPRECATED_NO_WARNINGS

/// ------------------------------------------------------------------------------------------------
/// SOCKET
/// ------------------------------------------------------------------------------------------------

/// <summary>
/// Initialises windows socket for UPD comunication
/// </summary>
WindowsSocketInit::WindowsSocketInit()
{
	WSADATA wsaData{};
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) == 0)
	{
		mOk = true;
	}
	else
	{
		ERR("WSAStartup failed");
	}
}

/// <summary>
/// Cleans up socket
/// </summary>
WindowsSocketInit::~WindowsSocketInit()
{
	if (mOk) WSACleanup();
}


/// ------------------------------------------------------------------------------------------------
/// SENDER
/// ------------------------------------------------------------------------------------------------

/// <summary>
/// Initializes sender with correct socket.
/// </summary>
/// <param name="ip"></param>
/// <param name="port"></param>
Sender::Sender(const std::string& ip, uint16_t port)
{
	mSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (mSocket == INVALID_SOCKET)
	{
		ERR("socket() failed, error: " << WSAGetLastError());
		return;
	}

	// Target
	mTarget.sin_family = AF_INET;
	mTarget.sin_port = htons(port);
	if (inet_pton(AF_INET, ip.c_str(), &mTarget.sin_addr) <= 0)
	{
		ERR("inet_pton() failed");
		closesocket(mSocket);
		mSocket = INVALID_SOCKET;
	}
}

/// <summary>
/// Simple destructor
/// </summary>
Sender::~Sender()
{
	if (mSocket != INVALID_SOCKET) closesocket(mSocket);
}

/// <summary>
/// Sends text to defined target in constructor. Returns true/false if successful or not
/// </summary>
/// <param name="text"></param>
/// <returns></returns>
bool Sender::SendText(const std::string& text)
{
	if (mSocket == INVALID_SOCKET) return false;

	int sent = sendto(mSocket, text.c_str(), static_cast<int>(text.size()), 0,
		reinterpret_cast<sockaddr*>(&mTarget), sizeof(mTarget));

	if (sent == SOCKET_ERROR)
	{
		ERR("sendto() failed, error: " << WSAGetLastError());
		return false;
	}

	std::cout << "Sending message: \"" << text << "\"\n";

	return true;
}

/// <summary>
/// Sends any data via UDP
/// </summary>
/// <param name="data"></param>
/// <param name="size"></param>
/// <returns></returns>
bool Sender::SendData(const Chunk& chunk)
{
	if (mSocket == INVALID_SOCKET) return false;

	int sent = sendto(mSocket, static_cast<const char*>((void*)chunk.data.data()), chunk.packetSize, 0,
		reinterpret_cast<sockaddr*>(&mTarget), sizeof(mTarget));

	if (sent == SOCKET_ERROR)
	{
		ERR("sendto() failed, error: " << WSAGetLastError());
		return false;
	}

	PrintChunkLine(chunk);

	return true;
}

/// <summary>
/// Sends ACK or NACK message based on state
// todo need to be moved as chunk packet so CRC works
/// </summary>
/// <param name="state"></param>
/// <param name="seq"></param>
/// <param name="ip"></param>
/// <param name="port"></param>
/// <returns></returns>
bool Sender::SendAckOrNack(bool state, uint32_t seq)
{
	std::string msg = "INVALID";
	if (state)
	{
		msg = "ACK=" + std::to_string(seq);
	}
	else
	{
		msg = "NACK=" + std::to_string(seq);
	}

	return SendText(msg);
}



bool Sender::SendFileAckOrNack(bool state)
{
	std::string msg = "INVALID";
	if (state)
	{
		std::cout << "Hash is valid, sending FACK\n";
		msg = "FACK";
	}
	else
	{
		std::cout << "Hash is not valid, sending FNACK\n";
		msg = "FNACK";
	}

	return SendText(msg);
}


/// ------------------------------------------------------------------------------------------------
/// RECIVER
/// ------------------------------------------------------------------------------------------------
 
/// <summary>
/// Creates reciever for defined port
/// </summary>
/// <param name="port"></param>
Receiver::Receiver(uint16_t port)
{
	mSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (mSocket == INVALID_SOCKET)
	{
		ERR("socket() failed, error: " << WSAGetLastError());
		return;
	}

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port);

	if (bind(mSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
	{
		ERR("bind() failed, error: " << WSAGetLastError());
		closesocket(mSocket);
		mSocket = INVALID_SOCKET;
	}
}

/// <summary>
/// Simple destructor
/// </summary>
Receiver::~Receiver()
{
	if (mSocket != INVALID_SOCKET) closesocket(mSocket);
}

/// <summary>
/// Recieves text, if text was sent
/// </summary>
/// <param name="outText"></param>
/// <param name="outFromIp"></param>
/// <param name="outFromPort"></param>
/// <returns></returns>
bool Receiver::ReceiveText(std::string& outText, std::string* outFromIp, uint16_t* outFromPort)
{
	if (mSocket == INVALID_SOCKET)
		return false;

	char buffer[PACKET_MAX_LENGTH + 1];
	sockaddr_in from{};
	int fromLen = sizeof(from);

	int received = recvfrom(mSocket, buffer, PACKET_MAX_LENGTH, 0,
		reinterpret_cast<sockaddr*>(&from), &fromLen);

	if (received == SOCKET_ERROR)
	{
		ERR("Receiver: recvfrom() failed, error: " << WSAGetLastError());
		return false;
	}

	buffer[received] = '\0';
	outText.assign(buffer, received);

	if (outFromIp)
	{
		char ipBuff[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &from.sin_addr, ipBuff, sizeof(ipBuff));
		*outFromIp = ipBuff;
	}

	if (outFromPort)
		*outFromPort = ntohs(from.sin_port);

	return true;
}


/// <summary>
/// Receives data into chunk. Waits time to see if data is available or not. See: UDP::RECEIVER_TIMEOUT
/// </summary>
/// <param name="data"></param>
/// <param name="outFromIp"></param>
/// <param name="outFromPort"></param>
/// <returns></returns>
bool Receiver::ReceiveData(UDP::Chunk& data, bool& ack, std::string* outFromIp, uint16_t* outFromPort)
{
	ack = true;
	if (mSocket == INVALID_SOCKET)
		return false;

	// buffer for data
	uint8_t buffer[UDP::PACKET_MAX_LENGTH];

	sockaddr_in from{};
	int fromLen = sizeof(from);

	// Timeout
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(mSocket, &readfds);

	timeval tv{};
	tv.tv_sec = 0;
	tv.tv_usec = UDP::RECEIVER_TIMEOUT; // 200 ms timeout

	int sel = select(0, &readfds, nullptr, nullptr, &tv);
	if (sel == SOCKET_ERROR)
	{
		std::cerr << "Receiver: select() failed, err="
			<< WSAGetLastError() << "\n";
		return false;
	}
	if (sel == 0) return false; // Nothing to be read

	int received = recvfrom(mSocket, reinterpret_cast<char*>(buffer), UDP::PACKET_MAX_LENGTH, 0,
							reinterpret_cast<sockaddr*>(&from), &fromLen);

	if (received == SOCKET_ERROR)
	{
		std::cerr << "Receiver: recvfrom() failed, err="
			<< WSAGetLastError() << "\n";
		return false;
	}

	if (received == 0)
	{
		return false;
	}

	// we get chunk
	data.packetSize = static_cast<size_t>(received);
	data.data.resize(data.packetSize);
	std::memcpy(data.data.data(), buffer, data.packetSize);

	if (!data.CheckValidity())
	{
		std::cerr << "Receiver: packet exceeds PACKET_MAX_LENGTH\n";
		return false;
	}

	// We parse data into chunk for easier usage later
	data.RetrieveCRC();
	data.RetrieveSeq();
	data.RetrieveOffset();
	uint32_t crc = data.ComputeCRC();

	// CRC
	if (crc != data.retrievedCRC)
	{
		std::cerr << "Receiver: CRC mismatch (seq=" << data.seq << ", offset=" << data.offset << ")\n";
		ack = false;
	}

	// IP + port
	if (outFromIp)
	{
		char ipBuff[INET_ADDRSTRLEN];
		inet_ntop(AF_INET, &from.sin_addr, ipBuff, sizeof(ipBuff));
		*outFromIp = ipBuff;
	}

	if (outFromPort)
		*outFromPort = ntohs(from.sin_port);

	return true;
}



/// <summary>
/// Receives ACK or NACK. Checks if it has correct sequence number. Waits designated time
/// </summary>
/// <param name="expectedSeq"></param>
/// <param name="timeout">timeout in microseconds! see UDP::ACK_RECEIVER_TIMEOUT</param>
/// <param name="outIsNack">received ACK/NACK</param>
/// <returns></returns>
bool Receiver::ReceiveAckOrNack(uint32_t expectedSeq, int timeout, bool& outIsNack)
{
	if (mSocket == INVALID_SOCKET)
		return false;

	// Timeout
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(mSocket, &readfds);

	timeval tv{};
	tv.tv_sec  = timeout / 1000000;
	tv.tv_usec = timeout % 1000000;

	int sel = select(0, &readfds, nullptr, nullptr, &tv);
	if (sel == SOCKET_ERROR)
	{
		std::cerr << "Sender: select() failed, err=" << WSAGetLastError() << "\n";
		return false;
	}

	// Nothing
	if (sel == 0) return false;

	// Something to read
	char buffer[1024 + 1];
	sockaddr_in from{};
	int fromLen = sizeof(from);

	int received = recvfrom(mSocket, buffer, 1024, 0,
		(sockaddr*)&from, &fromLen);

	if (received == SOCKET_ERROR)
	{
		std::cerr << "Receiver: recvfrom() for ACK/NACK failed, err=" << WSAGetLastError() << "\n";
		return false;
	}

	buffer[received] = '\0';
	std::string msg(buffer, received);

	// We want "ACK=<n>" or "NACK=<n>"
	bool isAck = false;
	bool isNack = false;
	uint32_t seq = 0;

	if (msg.rfind("ACK=", 0) == 0)
	{
		isAck = true;
		try
		{
			seq = static_cast<uint32_t>(std::stoul(msg.substr(4)));
		}
		catch (...)
		{
			std::cerr << "Sender: invalid ACK format: " << msg << "\n";
			return false;
		}
	}
	else if (msg.rfind("NACK=", 0) == 0)
	{
		isNack = true;
		try
		{
			seq = static_cast<uint32_t>(std::stoul(msg.substr(5)));
		}
		catch (...)
		{
			std::cerr << "Sender: invalid NACK format: " << msg << "\n";
			return false;
		}
	}
	else
	{
		// Something different received
		std::cerr << "Sender: unknown control message: " << msg << "\n";
		return false;
	}

	// What if the NACK or ACK is for different packet?
	if (seq != expectedSeq)
	{
		std::cerr << "Sender: ACK/NACK for unexpected seq=" << seq
			<< ", expected=" << expectedSeq << "\n";

		return false;
	}

	// We have our ACK or NACk
	outIsNack = isNack;
	return true;
}


/// <summary>
/// Receives any ACK or NACK. Waits designated time
/// </summary>
/// <param name="expectedSeq"></param>
/// <param name="timeout">timeout in microseconds! see UDP::ACK_RECEIVER_TIMEOUT</param>
/// <param name="outIsNack">received ACK/NACK</param>
/// <returns></returns>
bool Receiver::ReceiveAnyAckOrNack(uint32_t& sequence, int timeout, bool& outIsNack)
{
	if (mSocket == INVALID_SOCKET)
		return false;

	// Timeout
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(mSocket, &readfds);

	timeval tv{};
	tv.tv_sec = timeout / 1000000;
	tv.tv_usec = timeout % 1000000;

	int sel = select(0, &readfds, nullptr, nullptr, &tv);
	if (sel == SOCKET_ERROR)
	{
		std::cerr << "Sender: select() failed, err=" << WSAGetLastError() << "\n";
		return false;
	}

	// Nothing
	if (sel == 0) return false;

	// Something to read
	char buffer[1024 + 1];
	sockaddr_in from{};
	int fromLen = sizeof(from);

	int received = recvfrom(mSocket, buffer, 1024, 0,
		(sockaddr*)&from, &fromLen);

	if (received == SOCKET_ERROR)
	{
		std::cerr << "Receiver: recvfrom() for ACK/NACK failed, err=" << WSAGetLastError() << "\n";
		return false;
	}

	buffer[received] = '\0';
	std::string msg(buffer, received);

	// We want "ACK=<n>" or "NACK=<n>"
	bool isAck = false;
	bool isNack = false;
	uint32_t seq = 0;

	if (msg.rfind("ACK=", 0) == 0)
	{
		isAck = true;
		try
		{
			sequence = static_cast<uint32_t>(std::stoul(msg.substr(4)));
		}
		catch (...)
		{
			std::cerr << "Sender: invalid ACK format: " << msg << "\n";
			return false;
		}
	}
	else if (msg.rfind("NACK=", 0) == 0)
	{
		isNack = true;
		try
		{
			sequence = static_cast<uint32_t>(std::stoul(msg.substr(5)));
		}
		catch (...)
		{
			std::cerr << "Sender: invalid NACK format: " << msg << "\n";
			return false;
		}
	}
	else
	{
		// Something different received
		std::cerr << "Sender: unknown control message: " << msg << "\n";
		return false;
	}

	// We have our ACK or NACk
	outIsNack = isNack;
	return true;
}



bool Receiver::ReceiveFileAckOrNack(int timeout, bool& outIsNack)
{
	if (mSocket == INVALID_SOCKET)
		return false;

	// Timeout
	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(mSocket, &readfds);

	timeval tv{};
	tv.tv_sec = timeout / 1000000;
	tv.tv_usec = timeout % 1000000;

	int sel = select(0, &readfds, nullptr, nullptr, &tv);
	if (sel == SOCKET_ERROR)
	{
		std::cerr << "Sender: select() failed, err=" << WSAGetLastError() << "\n";
		return false;
	}

	// Nothing
	if (sel == 0) return false;

	// Something to read
	char buffer[1024 + 1];
	sockaddr_in from{};
	int fromLen = sizeof(from);

	int received = recvfrom(mSocket, buffer, 1024, 0,
		(sockaddr*)&from, &fromLen);

	if (received == SOCKET_ERROR)
	{
		std::cerr << "Receiver: recvfrom() for ACK/NACK failed, err=" << WSAGetLastError() << "\n";
		return false;
	}

	buffer[received] = '\0';
	std::string msg(buffer, received);

	// We want "ACK=<n>" or "NACK=<n>"
	bool isAck = false;
	bool isNack = false;
	uint32_t seq = 0;

	if (msg.rfind("FACK", 0) == 0)
	{
		isAck = true;
	}
	else if (msg.rfind("FNACK", 0) == 0)
	{
		isNack = true;
	}
	else
	{
		// Something different received
		std::cerr << "Sender: unknown control message: " << msg << "\n";
		return false;
	}

	// We have our ACK or NACk
	outIsNack = isNack;
	return true;
}