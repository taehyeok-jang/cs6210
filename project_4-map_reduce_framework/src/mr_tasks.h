#pragma once

#include <string>
#include <iostream>
#include <map>
#include <filesystem>
#include <unordered_map>
#include <fstream>


/* CS6210_TASK Implement this data structureas per your implementation.
		You will need this when your worker is running the map task*/
struct BaseMapperInternal {

		/* DON'T change this function's signature */
		BaseMapperInternal();

		/* DON'T change this function's signature */
		void emit(const std::string& key, const std::string& val);

		/* NOW you can add below, data members and member functions as per the need of your implementation*/
	
		void initialization(const int mapper_id, const std::string& intermediate_file_dir, const int n_output);

		int get_hashed_val(const std::string& key);

		void save_as_files();

	private:
		int mapper_id_;
		int n_output_;
		int buffered_words_count;
    	std::string intermediate_file_dir_;
		std::vector<std::vector<std::pair<std::string, std::string>>> reducerBuffers;
};


/* CS6210_TASK Implement this function */
inline BaseMapperInternal::BaseMapperInternal() {
	buffered_words_count = 0;
}


/* CS6210_TASK Implement this function */
inline void BaseMapperInternal::emit(const std::string& key, const std::string& val) {
	int reducer_id = get_hashed_val(key);
	reducerBuffers[reducer_id].emplace_back(key, val);
	buffered_words_count++;

	// if (buffered_words_count > 150) {
	// 	save_as_files();
	// 	buffered_words_count = 0;
	// }
}

inline int BaseMapperInternal::get_hashed_val(const std::string& key) {
	return std::hash<std::string>{}(key)%n_output_;
}

inline void BaseMapperInternal::save_as_files() {
	for (int i = 0; i < n_output_; i++) {
		std::string path = intermediate_file_dir_ + "/mapper_" + std::to_string(mapper_id_) + "_reducer_" + std::to_string(i) + ".txt";
		std::ofstream file(path, std::ios::app);
		if (!file.is_open()) {
			std::cerr << "Failed to open file: " << path << std::endl;
			continue;
		}

		for (const auto& [key, val] : reducerBuffers[i]) {
			file << key << ", " << val << "\n";
		}
		
		file.flush();
		reducerBuffers[i].clear();
	}
}



/* CS6210_TASK Implement this function */
inline void BaseMapperInternal::initialization(const int mapper_id, const std::string& intermediate_file_dir, const int n_output) {
	mapper_id_ = mapper_id;
    intermediate_file_dir_ = intermediate_file_dir;
    n_output_ = n_output;
	reducerBuffers.resize(n_output_);

}


/*-----------------------------------------------------------------------------------------------*/


/* CS6210_TASK Implement this data structureas per your implementation.
		You will need this when your worker is running the reduce task*/
struct BaseReducerInternal {

		/* DON'T change this function's signature */
		BaseReducerInternal();

		/* DON'T change this function's signature */
		void emit(const std::string& key, const std::string& val);

		/* NOW you can add below, data members and member functions as per the need of your implementation*/

		// TODO 
		// 
		std::map<std::string, std::string> outputs;

		void initialization(const int reducer_id, const std::string& output_dir);

		void save_as_file();
	
	private:
		int reducer_id_;
    	std::string output_dir_;
};


/* CS6210_TASK Implement this function */
inline BaseReducerInternal::BaseReducerInternal() {

}


/* CS6210_TASK Implement this function */
/**
 * ASIS.
 * key: "bear"
 * 
 * TOBE.
 * key: "<reducer_id>:bear"
 */
inline void BaseReducerInternal::emit(const std::string& key, const std::string& val) {
	outputs[key] =  val;

	// if (outputs.size() > 50) {
	// 	save_as_file();
	// }

}

inline void BaseReducerInternal::initialization(const int reducer_id, const std::string& output_dir) {
	reducer_id_ = reducer_id;
	output_dir_ = output_dir;

	std::string path = output_dir_ + "/output_" + std::to_string(reducer_id_) + ".txt";

	if (std::filesystem::exists(path)) {
        // Open in trunc mode to clear contents
        std::ofstream ofs(path, std::ios::trunc);
        if (!ofs) {
        std::cerr << "Failed to open file for truncating.\n";
        }
	}
}

inline void BaseReducerInternal::save_as_file() {

	std::string path = output_dir_ + "/output_" + std::to_string(reducer_id_) + ".txt";
	std::ofstream file(path, std::ios::app);
	if (!file.is_open()) {
		std::cerr << "Failed to open file: " << path << std::endl;
		return;
	}

	for (const auto& [key, val] : outputs) {
		file << key << " " << val << "\n";
	}
		
	file.flush();
	outputs.clear();
	
}

