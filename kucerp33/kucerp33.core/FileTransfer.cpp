#include "FileTransfer.h"
#include "SmartDebug.h"

#include <filesystem>
#include <fstream>

using namespace UDP;
namespace fs = std::filesystem;

/// <summary>
/// Opens file and fills its self with data
/// </summary>
/// <param name="path"></param>
/// <returns></returns>
bool FileSession::SetFromFile(const std::string& path)
{
	this->chunks.clear();

	// Try to open the file
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file)
	{
		ERR("File could not be opened: " << path);
		return false;
	}

	std::streamsize size = file.tellg();
	if (size < 0)
	{
		ERR("Size of file could not be received: " << path);
		return false;
	}
	file.seekg(0, std::ios::beg);
	
	fs::path p(path);
	// Get name
	std::string name = p.filename().string();

	this->fileName = name;
	this->totalSize = size;

	// Start to read file
	std::vector<unsigned char> buffer(UDP::PACKET_MAX_LENGTH);
	uint32_t offset = 0;
	while (file)
	{
		Chunk fileChunk{};

		uint32_t offsetNet = htonl(offset);
		std::memcpy(buffer.data(), "DATA=", 5);
		std::memcpy(buffer.data() + 5, &offsetNet, sizeof(offsetNet));

		// Pointer arithmetic -> +5, +4 so we 'skip' the first 4+5 bytes
		file.read(reinterpret_cast<char*>(buffer.data() + 9), UDP::PACKET_MAX_LENGTH - 9);

		std::streamsize bytesRead = file.gcount();
		if (bytesRead <= 0) break; // We're on the EOF

		int packetSize = 9 + static_cast<int>(bytesRead); // Max 1024 Bytes

		fileChunk.data.resize(packetSize); // Resize so we don't waste space on our prescius RAM
		std::memcpy(fileChunk.data.data(), buffer.data(), packetSize);
		fileChunk.packetSize = packetSize;

		this->chunks.insert({ offset, fileChunk });

		offset += static_cast<uint32_t>(bytesRead);
	}

	return true;
}


bool FileSession::SaveToFile(const std::string& path)
{
	if (this->fileName.size() == 0)
	{
		ERR("Invalid file name!");
		return false;
	}

	std::ofstream out(path + this->fileName, std::ios::binary);
	if (!out)
	{
		ERR("Could not create file in specified path: \"" + path + "\"");
		return false;
	}

	// We create correct size
	out.seekp(static_cast<std::streamoff>(totalSize - 1), std::ios::beg);
	out.write("", 1);
	out.seekp(0, std::ios::beg);

	for (auto& [offset, data] : chunks)
	{
		out.seekp(static_cast<std::streamoff>(offset), std::ios::beg);

		out.write(reinterpret_cast<const char*>(data.data.data() + data.padding),
			static_cast<std::streamsize>(data.packetSize - data.padding));
	}
}