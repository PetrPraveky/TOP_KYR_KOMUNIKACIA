#include "FileTransfer.h"
#include "SmartDebug.h"

#include "crc.hpp"
#include "picosha2.h"
#include <filesystem>

using namespace UDP;
namespace fs = std::filesystem;

uint32_t Chunk::ComputeCRC() {
	boost::crc_32_type result;

	result.process_bytes(data.data() + Chunk::seq_padding, packetSize - Chunk::seq_padding);
	crc = result.checksum();

	return result.checksum();
}

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
	// todo hash
	
	// Create sequences
	CreateNameChunk();
	CreateSizeChunk();
	CreateHashChunk(file);

	// Start to read file
	std::vector<unsigned char> buffer(UDP::PACKET_MAX_LENGTH);
	uint32_t offset = 0;
	while (file)
	{
		Chunk fileChunk{};

		std::memcpy(buffer.data() + Chunk::seq_padding, &currentSequence, 4);
		std::memcpy(buffer.data() + Chunk::command_padding, "DATA", 4);
		std::memcpy(buffer.data() + Chunk::offset_padding, &offset, sizeof(offset));

		// Pointer arithmetic -> 'skip' the first 13 bytes -> CRC+COMMAND+OFFSET
		file.read(
			reinterpret_cast<char*>(buffer.data() + Chunk::data_padding), 
			UDP::PACKET_MAX_LENGTH - Chunk::data_padding);

		std::streamsize bytesRead = file.gcount();
		if (bytesRead <= 0) break; // We're on the EOF

		int packetSize = Chunk::data_padding + static_cast<int>(bytesRead); // Max 1024 Bytes

		fileChunk.data.resize(packetSize); // Resize so we don't waste space on our prescius RAM
		std::memcpy(fileChunk.data.data(), buffer.data(), packetSize);
		fileChunk.packetSize = packetSize;

		// Do CRC
		uint32_t crc = fileChunk.ComputeCRC();
		std::memcpy(fileChunk.data.data(), &crc, Chunk::seq_padding);

		this->chunks.insert({ currentSequence, fileChunk });

		offset += static_cast<uint32_t>(bytesRead);

		++currentSequence;
	}

	CreateStopChunk();

	return true;
}


/// <summary>
/// Parses chunk data into properties of file session.
/// </summary>
/// <returns></returns>
bool FileSession::ParseChunkData()
{
	for (auto [seq, chunk] : chunks)
	{
		// Name
		if (chunk.CommandReceived("NAME"))
		{
			auto [ptr, len] = chunk.GetData();
			if (!ptr || len == 0)
			{
				std::cerr << "ParseChunkData: NAME has invalid data!" << "\n";
				return false;
			}
			fileName = std::string(reinterpret_cast<const char*>(ptr), len);
		}
	
		// Size
		if (chunk.CommandReceived("SIZE"))
		{
			auto [ptr, len] = chunk.GetData();
			if (!ptr || len < sizeof(totalSize))
			{
				std::cerr << "ParseChunkData: SIZE has invalid data!" << "\n";
				return false;
			}
			memcpy(&totalSize, ptr, sizeof(totalSize));
		}

		// Hash
		if (chunk.CommandReceived("HASH")) 
		{
			auto [ptr, len] = chunk.GetData();
			if (!ptr || len < sizeof(totalSize)) 
			{
				std::cerr << "ParseChunkData: HASH has invalid data!" << "\n";
				return false;
			}
			memcpy(&hash, ptr, sizeof(hash));
		}
	}

	return true;
}



bool FileSession::SaveToFile(bool& hashOk, const std::string& path)
{
	if (this->fileName.size() == 0)
	{
		ERR("Invalid file name!");
		return false;
	}

	// Create bytestream for picosha2
	std::vector<uint8_t> fileData(totalSize, 0);

	for (auto& [seq, chunk] : chunks)
	{
		// We need only data
		if (!chunk.CommandReceived("DATA")) continue;
		uint32_t offset = chunk.RetrieveOffset();

		size_t payloadSize = chunk.packetSize - UDP::Chunk::data_padding;

		// Check if we're larger than file
		if (offset + payloadSize > fileData.size())
		{
			ERR("Chunk (seq=" << seq << ") with offset=" << offset
				<< " and size=" << payloadSize
				<< " exceeds totalSize=" << fileData.size());
			continue;
		}

		// We copy data from chunk into file data
		std::memcpy(
			fileData.data() + offset,
			chunk.data.data() + UDP::Chunk::data_padding,
			payloadSize
		);
	}

	// Hash check
	picosha2::hash256_one_by_one hasher;
	hasher.process(fileData.begin(), fileData.end());
	hasher.finish();

	std::array<uint8_t, 32> possibleHash;
	hasher.get_hash_bytes(possibleHash.begin(), possibleHash.end());
	
	// We compare the hashes
	if (possibleHash != hash) {
		std::cout << "Hash is not correct!\n";
		hashOk = false;
		return false;
	}

	std::ofstream out(path + this->fileName, std::ios::binary);
	if (!out)
	{
		ERR("Could not create file in specified path: \"" + path + "\"");
		return false;
	}

	// We write the data
	out.write(
		reinterpret_cast<const char*>(fileData.data()),
		static_cast<std::streamsize>(fileData.size())
	);

	return true;
}


void FileSession::CreateNameChunk()
{
	// Name
	Chunk nameChunk;
	std::string nameCommand = "NAME";
	nameChunk.packetSize = Chunk::data_padding + fileName.size();

	// Cuz we dont have the memory yet
	nameChunk.data.resize(nameChunk.packetSize); 
	std::memset(nameChunk.data.data(), 0, Chunk::data_padding);

	memcpy(nameChunk.data.data() + Chunk::data_padding, fileName.data(), fileName.size());
	memcpy(nameChunk.data.data() + Chunk::seq_padding, &currentSequence, 4);
	memcpy(nameChunk.data.data() + Chunk::command_padding, nameCommand.c_str(), 4);
	uint32_t nameCRC = nameChunk.ComputeCRC();
	memcpy(nameChunk.data.data() + Chunk::crc_padding, &nameCRC, 4);

	chunks.insert({ currentSequence, nameChunk });

	++currentSequence;
}


void FileSession::CreateSizeChunk()
{
	// Name
	Chunk sizeChunk;
	std::string sizeCommand = "SIZE";

	sizeChunk.packetSize = Chunk::data_padding + sizeof(totalSize);
	
	// Cuz we dont have the memory yet
	sizeChunk.data.resize(sizeChunk.packetSize);
	std::memset(sizeChunk.data.data(), 0, Chunk::data_padding);

	memcpy(sizeChunk.data.data() + Chunk::data_padding, &totalSize, sizeof(totalSize));
	memcpy(sizeChunk.data.data() + Chunk::seq_padding, &currentSequence, 4);
	memcpy(sizeChunk.data.data() + Chunk::command_padding, sizeCommand.c_str(), 4);
	uint32_t sizeCRC = sizeChunk.ComputeCRC();
	memcpy(sizeChunk.data.data() + Chunk::crc_padding, &sizeCRC, 4);

	chunks.insert({ currentSequence, sizeChunk });

	++currentSequence;
}


void FileSession::CreateStopChunk()
{
	// Name
	Chunk stopChunk;
	std::string stopCommand = "STOP";
	
	stopChunk.packetSize = Chunk::data_padding;
	
	// Cuz we dont have the memory yet
	stopChunk.data.resize(stopChunk.packetSize);
	std::memset(stopChunk.data.data(), 0, Chunk::data_padding);

	memcpy(stopChunk.data.data() + Chunk::seq_padding, &currentSequence, 4);
	memcpy(stopChunk.data.data() + Chunk::command_padding, stopCommand.c_str(), 4);
	uint32_t stopCRC = stopChunk.ComputeCRC();
	memcpy(stopChunk.data.data() + Chunk::crc_padding, &stopCRC, 4);

	chunks.insert({ currentSequence, stopChunk });

	++currentSequence;
}


void FileSession::CreateHashChunk(std::ifstream& file)
{
	// Name
	Chunk hashChunk;
	std::string hashCommand = "HASH";
	std::string hashType = "S256";

	hashChunk.packetSize = Chunk::data_padding + picosha2::k_digest_size;

	// Hash computation
	picosha2::hash256(file, hash.begin(), hash.end());

	// We reset file stream
	file.clear();
	file.seekg(0, std::ios::beg);

	// Cuz we dont have the memory yet
	hashChunk.data.resize(hashChunk.packetSize);
	std::memset(hashChunk.data.data(), 0, Chunk::data_padding);

	memcpy(hashChunk.data.data() + Chunk::seq_padding, &currentSequence, 4);
	memcpy(hashChunk.data.data() + Chunk::command_padding, hashCommand.c_str(), 4);
	memcpy(hashChunk.data.data() + Chunk::offset_padding, hashType.c_str(), 4);
	memcpy(hashChunk.data.data() + Chunk::data_padding, hash.data(), hash.size());
	uint32_t stopCRC = hashChunk.ComputeCRC();
	memcpy(hashChunk.data.data() + Chunk::crc_padding, &stopCRC, 4);

	chunks.insert({ currentSequence, hashChunk });

	++currentSequence;
}