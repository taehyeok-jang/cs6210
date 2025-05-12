#pragma once

#include <vector>
#include "mapreduce_spec.h"
#include <fstream>
#include <cmath>
#include <sstream>
#include <iostream>


/* CS6210_TASK: Create your own data structure here, where you can hold information about file splits,
     that your master would use for its own bookkeeping and to convey the tasks to the workers for mapping */
struct FilePiece {
     std::string filepath;
     size_t start_offset;
     size_t end_offset;
};
     
struct FileShard {
     std::vector<FilePiece> pieces;
};
     


/* CS6210_TASK: Create fileshards from the list of input files, map_kilobytes etc. using mr_spec you populated  */ 
inline bool shard_files(const MapReduceSpec& mr_spec, std::vector<FileShard>& fileShards) {
	std::cout << "file_shard.h: shard_files..." << std::endl;
	
	size_t SHARD_SIZE = mr_spec.map_kilobytes * 1024; // convert KB to bytes
     
	std::ifstream in;
	size_t current_shard_bytes = 0;
	FileShard current_shard;

	for (const auto& file : mr_spec.input_files) {
		in.open(file);
		if (!in) {
			std::cerr << "Failed to open " << file << "\n";
			return false;
		}

		std::string line;
		size_t offset = 0;
		// size_t shard_start = 0;

		while (std::getline(in, line)) {
			size_t line_size = line.size() + 1; // +1 for '\n'

			// If adding this line would exceed the shard size, flush current shard
			if (current_shard_bytes + line_size > SHARD_SIZE && !current_shard.pieces.empty()) {
				fileShards.push_back(current_shard);
				current_shard = FileShard();
				current_shard_bytes = 0;
				// shard_start = offset;
			}

			// Append to current shard
			if (current_shard.pieces.empty() ||
			    current_shard.pieces.back().filepath != file) {
				current_shard.pieces.push_back({file, offset, offset + line_size});
			} else {
				current_shard.pieces.back().end_offset += line_size;
			}

			offset += line_size;
			current_shard_bytes += line_size;
		}

		in.close();
	}

	// Push last shard
	if (!current_shard.pieces.empty()) {
		fileShards.push_back(current_shard);
	}

	return true;
}
