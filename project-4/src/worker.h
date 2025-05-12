#pragma once

#include <mr_task_factory.h>
#include "mr_tasks.h"

#include <grpcpp/grpcpp.h>
#include "masterworker.grpc.pb.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <regex>
#include <filesystem>
#include <unordered_map>
#include <random>

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

using masterworker::MasterWorker;
using masterworker::MapRequest;
using masterworker::ReduceRequest;
using masterworker::WorkerResponse;


std::vector<std::string> splitRegex(const std::string& input, const std::string& pattern) {
    std::regex re(pattern);
    std::sregex_token_iterator it(input.begin(), input.end(), re, -1);
    std::sregex_token_iterator end;
    return {it, end};
}

/* CS6210_TASK: Handle all the task a Worker is supposed to do.
	This is a big task for this project, will test your understanding of map reduce */
	class Worker {

		public:
			/* DON'T change the function signature of this constructor */
			Worker(std::string ip_addr_port);
	
			/* DON'T change this function's signature */
			bool run();
			void handleMapTask(const MapRequest* request, WorkerResponse* response);
			void handleReduceTask(const ReduceRequest* request, WorkerResponse* response);
	
	
		private:
			/* NOW you can add below, data members and member functions as per the need of your implementation*/
			std::string ip_addr_port_;
	
	};

	
// The server implementation
class MasterWorkerServiceImpl final : public MasterWorker::Service {
public:
	explicit MasterWorkerServiceImpl(Worker* worker) : worker_(worker) {}

	Status assignMapTask(ServerContext* context, const MapRequest* request,
                         WorkerResponse* response) override {
		worker_->handleMapTask(request, response);
		return Status::OK;

    }

    Status assignReduceTask(ServerContext* context, const ReduceRequest* request,
                            WorkerResponse* response) override {
		worker_->handleReduceTask(request, response);
		return Status::OK;
    }

	Worker* worker_;
};





/* CS6210_TASK: ip_addr_port is the only information you get when started.
	You can populate your other class data members here if you want */
	Worker::Worker(std::string ip_addr_port) : ip_addr_port_(ip_addr_port) {
		// Store ip_addr_port into member variable
	}

extern std::shared_ptr<BaseMapper> get_mapper_from_task_factory(const std::string& user_id);
extern std::shared_ptr<BaseReducer> get_reducer_from_task_factory(const std::string& user_id);

/* CS6210_TASK: Here you go. once this function is called your woker's job is to keep looking for new tasks 
	from Master, complete when given one and again keep looking for the next one.
	Note that you have the access to BaseMapper's member BaseMapperInternal impl_ and 
	BaseReduer's member BaseReducerInternal impl_ directly, 
	so you can manipulate them however you want when running map/reduce tasks*/
bool Worker::run() {
	MasterWorkerServiceImpl service(this);

    ServerBuilder builder;
    builder.AddListeningPort(ip_addr_port_, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Worker Server listening on " << ip_addr_port_ << std::endl;

    server->Wait();


	return true;
}

void Worker::handleMapTask(const MapRequest* request, WorkerResponse* response) {

	// Randomly introduce a 10-second delay with 5% probability
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_real_distribution<> dis(0.0, 1.0);
	
	// simulating fault-injection (slow worker)
	if (dis(gen) < 0.15) { // 10% probability
		std::cout << "[WORKER " << ip_addr_port_ << "] Simulating slow worker - sleeping for 10s" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds(10));
	}

	std::ifstream in;
	auto mapper = get_mapper_from_task_factory("cs6210");
	mapper->impl_->initialization(
		request->mapper_id(),
		request->intermediate_file_dir(),
		request->n_output()
	);

	namespace fs = std::filesystem;
	if (!fs::exists(request->intermediate_file_dir())) {
        try {
            fs::create_directories(request->intermediate_file_dir());  // creates all intermediate directories if needed
            std::cout << "Created directory: " << request->intermediate_file_dir() << std::endl;
        } catch (const fs::filesystem_error& e) {
			std::string error_msg = std::string("Failed to create directory: ") + e.what();
			std::cerr << error_msg << std::endl;
			response->set_success(false);
			response->set_output_files("");
			response->set_error(error_msg);
			return;
        }
    }

	bool all_success = true;
	std::ostringstream error_messages;

	for (const auto& file : request->file_pieces()) {
		in.open(file.file_path());
		if (!in) {
			all_success = false;
			std::cerr << "[ERROR] Mapper task failed: failed to open " << file.file_path() << std::endl;
			error_messages << "Failed to open file: " << file.file_path() << "\n";
			continue;
		}

		in.seekg(file.start_offset());
		if (!in) {
			all_success = false;
			std::cerr << "[ERROR] Failed to seek to offset in " << file.file_path() << std::endl;
			error_messages << "Failed to seek in file: " << file.file_path() << "\n";
			in.close();
			continue;
		}


		std::string line;
		size_t current_shard_bytes = file.start_offset();

		while (current_shard_bytes < file.end_offset() && std::getline(in, line)) {
			size_t line_size = line.size() + 1; // for '\n'
			current_shard_bytes += line_size;
			mapper->map(line);
		}

		in.close();
	}

	mapper->impl_->save_as_files();

	std::ostringstream output_files_stream;
	for (int i = 0; i < request->n_output(); ++i) {
		if (i > 0) output_files_stream << ",";
		output_files_stream << request->intermediate_file_dir() + "/mapper_" + std::to_string(request->mapper_id()) + "_reducer_" + std::to_string(i) + ".txt";
	}

	response->set_success(all_success);
	response->set_output_files(output_files_stream.str());
	response->set_error(error_messages.str());
}


void Worker::handleReduceTask(const ReduceRequest* request, WorkerResponse* response) {

    namespace fs = std::filesystem;

    std::string user_id = request->user_id();
    int reducer_id = request->reducer_id();
    std::string output_dir = request->output_dir();

    std::unordered_map<std::string, std::vector<std::string>> keyValues;
    std::string target_suffix = "reducer_" + std::to_string(reducer_id) + ".txt";


    try {
		if (!fs::exists(request->output_dir())) {
            fs::create_directories(request->output_dir());  // creates all intermediate directories if needed
            std::cout << "Created directory: " << request->output_dir() << std::endl;
        }

        // 1. Read all relevant intermediate files
		std::vector<fs::directory_entry> entries;
		for (const auto& dir: request->intermediate_file_dirs()){
			for (const auto& entry : fs::directory_iterator(dir)) {
				entries.push_back(entry);
			}
		}

        for (const auto& entry : entries) {
            if (entry.is_regular_file()) {
                std::string filename = entry.path().filename().string();

                if (filename.size() >= target_suffix.size() &&
                    filename.compare(filename.size() - target_suffix.size(), target_suffix.size(), target_suffix) == 0) {

                    std::ifstream in(entry.path());
                    if (!in) {
                        std::cerr << "[ERROR] Failed to open intermediate file: " << entry.path() << "\n";
                        continue;
                    }

                    std::string line;
                    while (std::getline(in, line)) {
                        size_t delim = line.find(",");
                        if (delim == std::string::npos) continue;

                        std::string key = line.substr(0, delim);
                        std::string value = line.substr(delim + 1);
                        if (!value.empty() && value[0] == ' ') value.erase(0, 1);  // trim space

                        keyValues[key].push_back(value);
                    }

                    in.close();
                }
            }
        }

        // 2. Sort keys (by inserting into std::map)
        std::map<std::string, std::vector<std::string>> sortedKeyValues(keyValues.begin(), keyValues.end());

        // 3. Run reducer logic
        auto reducer = get_reducer_from_task_factory(user_id);
        reducer->impl_->initialization(reducer_id, output_dir);

        for (const auto& [key, values] : sortedKeyValues) {
            reducer->reduce(key, values);
        }

        reducer->impl_->save_as_file();

        response->set_success(true);
        response->set_output_files("");
        response->set_error("");


    } catch (const std::exception& ex) {
        std::cerr << "[ERROR] Reduce task failed: " << ex.what() << std::endl;
        response->set_success(false);
        response->set_error(std::string("Reduce task failed: ") + ex.what());
    }
}
