#pragma once

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>


/* CS6210_TASK: Create your data structure here for storing spec from the config file */
struct MapReduceSpec {
	int n_workers;
	std::vector<std::string> worker_ipaddr_ports;
	std::vector<std::string> input_files;
	std::string output_dir;
	int n_output_files;
	int map_kilobytes;
	std::string user_id;
};


inline void split(const std::string& s, char delimiter, std::vector<std::string>& tokens) {
	std::stringstream ss(s);
	std::string token;
	while (std::getline(ss, token, delimiter)) {
		if (!token.empty()) tokens.push_back(token);
	}
}

inline bool read_mr_spec_from_config_file(const std::string& config_filename, MapReduceSpec& mr_spec) {
	std::cout << "mapreduce_spec.h: read_mr_spec_from_config_file..." << std::endl;
	
	std::ifstream config(config_filename);
	if (!config.is_open()) {
		std::cerr << "Error: Could not open config file: " << config_filename << "\n";
		return false;
	}

	std::string line;
	while (std::getline(config, line)) {
		if (line.empty()) continue;
		auto delimiter_pos = line.find('=');
		if (delimiter_pos == std::string::npos) continue;

		std::string key = line.substr(0, delimiter_pos);
		std::string value = line.substr(delimiter_pos + 1);

		if (key == "n_workers") {
			mr_spec.n_workers = std::stoi(value);
		} else if (key == "worker_ipaddr_ports") {
			split(value, ',', mr_spec.worker_ipaddr_ports);
		} else if (key == "input_files") {
			split(value, ',', mr_spec.input_files);
		} else if (key == "output_dir") {
			mr_spec.output_dir = value;
		} else if (key == "n_output_files") {
			mr_spec.n_output_files = std::stoi(value);
		} else if (key == "map_kilobytes") {
			mr_spec.map_kilobytes = std::stoi(value);
		} else if (key == "user_id") {
			mr_spec.user_id = value;
		}
	}

	config.close();
	return true;
}


/* CS6210_TASK: validate the specification read from the config file */
inline bool validate_mr_spec(const MapReduceSpec& mr_spec) {
	if (mr_spec.n_workers == 0 || mr_spec.n_workers != mr_spec.worker_ipaddr_ports.size()){
		return false;
	}

	if (mr_spec.output_dir.length() == 0){
		return false;
	}

	if (mr_spec.map_kilobytes == 0){
		return false;
	}

	if (mr_spec.user_id.length() == 0){
		return false;
	}

	if (mr_spec.n_output_files == 0){
		return false;
	}

	// @TODO: think about more ways to validate
	return true;
}
