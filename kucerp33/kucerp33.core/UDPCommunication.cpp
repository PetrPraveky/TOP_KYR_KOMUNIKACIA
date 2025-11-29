#include "UDPCommunication.h"
#include "SmartDebug.h"

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

	return true;
}

/// ------------------------------------------------------------------------------------------------
/// RECIVER
/// ------------------------------------------------------------------------------------------------
 
/// <summary>
/// Creates reciever for defined port
/// </summary>
/// <param name="port"></param>
Reciever::Reciever(uint16_t port)
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
Reciever::~Reciever()
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
bool Reciever::RecieveText(std::string& outText, std::string* outFromIp, uint16_t* outFromPort)
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