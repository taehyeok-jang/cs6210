#pragma once

#include <grpcpp/grpcpp.h>
#include "masterworker.grpc.pb.h"

#include "mapreduce_spec.h"
#include "file_shard.h"

#include <iostream>
#include <sstream>
#include <filesystem>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <thread>
#include <mutex>
#include <memory>

#include <algorithm>
#include <random>
#include <vector>
#include <deque>
#include <unordered_map>

namespace fs = std::filesystem;


/* CS6210_TASK: Handle all the bookkeeping that Master is supposed to do.
	This is probably the biggest task for this project, will test your understanding of map reduce */
class Master {

	public:
		/* DON'T change the function signature of this constructor */
		Master(const MapReduceSpec&, const std::vector<FileShard>&);

		/* DON'T change this function's signature */
		bool run();

	private:
        enum class WorkerState { IDLE, BUSY, DEAD };
        struct WorkerInfo {
            std::shared_ptr<grpc::Channel> channel;
            std::unique_ptr<masterworker::MasterWorker::Stub> stub;
            WorkerState state = WorkerState::IDLE;
        };

        enum class Phase { MAP, REDUCE };
        struct TaskMeta {
            int id; 
            Phase phase;
            std::atomic<bool> done{false};
            std::chrono::steady_clock::time_point start;
            std::mutex        mu;
            bool              accepted{false}; // first successful attempt wins
            std::string       accepted_dir; // winning intermediate dir (map only)
        };

        MapReduceSpec                      mr_spec_;
        const std::vector<FileShard>       file_shards_;
        std::vector<WorkerInfo>            workers_;

        std::vector<std::string>           intermediate_dirs_;
        std::mutex                         dirs_mu_;

	    /* RPC functions */
        bool doMapTask(int mapper_id, const FileShard &shard, WorkerInfo &w, std::string &out_dir);
        bool doReduceTask(int reducer_id, WorkerInfo &w);

        /* Helper functions */
        void init_workers_();
        bool run_phase(Phase p, int n_tasks);
        
        std::string gen_random_id_() const;
        void cleanup_output_dir_();
        void cleanup_intermediate_();

        void print_mr_spec_() const;
        void print_file_shards_() const;
                
        static constexpr const char* INTERMEDIATE_ROOT_DIR = "./intermediate";
        static constexpr auto BASE_SPEC_MS = std::chrono::milliseconds(4000); // 4 seconds floor
};


/* CS6210_TASK: This is all the information your master will get from the framework.
	You can populate your other class data members here if you want */
Master::Master(const MapReduceSpec& mr_spec, const std::vector<FileShard>& file_shards)
	: mr_spec_(mr_spec), file_shards_(file_shards) {}


inline bool Master::run() {
    std::cout << "\n================================================" << std::endl;
    std::cout << "master.h: run()..." << std::endl;

    print_mr_spec_();
    print_file_shards_();

    cleanup_output_dir_();

    // Create one gRPC stub per worker (reused across all tasks)
    init_workers_();

    // MAP PHASE
	  std::cout << "[MASTER] Starting map phase..." << std::endl;
    if (!run_phase(Phase::MAP, static_cast<int>(file_shards_.size()))) return false;
    std::cout << "[MASTER] Map phase completed" << std::endl;

    // REDUCE PHASE
    std::cout << "[MASTER] Starting reduce phase..." << std::endl;
    if (!run_phase(Phase::REDUCE, mr_spec_.n_output_files))            return false;
    std::cout << "[MASTER] Reduce phase completed" << std::endl;

    // clean up intermediate files
    cleanup_intermediate_();
    return true;
}

inline bool Master::doMapTask(
	int mapper_id, const FileShard& shard, WorkerInfo &w, std::string &out_dir
	) {
	  std::cout << "[MASTER] Doing map task for mapper... " << mapper_id << std::endl;

    // generate intermediate dir with random id
    const std::string rand_id = gen_random_id_();
    std::ostringstream oss;
    oss << INTERMEDIATE_ROOT_DIR << '/' << mr_spec_.user_id << '/' << mapper_id
        << '/' << rand_id;
    out_dir = oss.str();

    std::cout << "[MASTER] mapper_id: " << mapper_id << ", intermediate_dir: " << out_dir << std::endl;

    masterworker::MapRequest request;
    request.set_user_id(mr_spec_.user_id);
    request.set_mapper_id(mapper_id);
    request.set_intermediate_file_dir(out_dir); 
    request.set_n_output(mr_spec_.n_output_files);

    for (const auto& piece : shard.pieces) {
        auto* fp = request.add_file_pieces();
        fp->set_file_path(piece.filepath);
        fp->set_start_offset(piece.start_offset);
        fp->set_end_offset(piece.end_offset);
    }

    masterworker::WorkerResponse response;
    grpc::ClientContext ctx;
    grpc::Status status = w.stub->assignMapTask(&ctx, request, &response);

    if (!status.ok()) {
        std::cerr << "[MASTER] Map RPC failure (mapper " << mapper_id << ") : "
                  << status.error_message() << std::endl;
        return false;
    }
    if (!response.success()) {
        std::cerr << "[MASTER] Worker‑reported map failure (mapper " << mapper_id << ") : "
                  << response.error() << std::endl;
        return false;
    }

    return true;
}

inline bool Master::doReduceTask(
	int reducer_id, WorkerInfo &w
	) {
	std::cout << "[MASTER] Doing reduce task for reducer... " << reducer_id << std::endl;

    masterworker::ReduceRequest request;
    request.set_user_id(mr_spec_.user_id);
    request.set_reducer_id(reducer_id);
    request.set_output_dir(mr_spec_.output_dir);

    for (const auto& dir : intermediate_dirs_) {
        request.add_intermediate_file_dirs(dir);
    }

    std::string s = "";
    for (const auto& dir : request.intermediate_file_dirs()) {
        s += dir + " ";
    }
    std::cout << "[MASTER] reducer_id: " << reducer_id << ", intermediate dirs: " << s << std::endl;

    masterworker::WorkerResponse response;
    grpc::ClientContext ctx;
    grpc::Status status = w.stub->assignReduceTask(&ctx, request, &response);

    if (!status.ok()) {
        std::cerr << "[MASTER] Reduce RPC failure (reducer " << reducer_id << ") : "
                  << status.error_message() << std::endl;
        return false;
    }
    if (!response.success()) {
        std::cerr << "[MASTER] Worker‑reported reduce failure (reducer " << reducer_id << ") : "
                  << response.error() << std::endl;
        return false;
    }

    return true;
}

inline void Master::init_workers_() {
    for (const auto &addr : mr_spec_.worker_ipaddr_ports) {
        WorkerInfo w;
        w.channel = grpc::CreateChannel(addr, grpc::InsecureChannelCredentials());
        w.stub    = masterworker::MasterWorker::NewStub(w.channel);
        workers_.push_back(std::move(w));
    }
}

inline bool Master::run_phase(Phase phase, int n_tasks) {
  std::cout << "[MASTER] " << (phase==Phase::MAP?"MAP":"REDUCE") << " phase, tasks=" << n_tasks << std::endl;

  std::vector<TaskMeta> tasks(n_tasks);
  for (int i=0;i<n_tasks;++i){ tasks[i].id=i; tasks[i].phase=phase; }

  std::deque<int> pending; for(int i=0;i<n_tasks;++i) pending.push_back(i);
  std::unordered_map<int,int> running;
  std::mutex m; std::condition_variable cv;
  std::atomic<int> remaining=n_tasks; std::atomic<bool> phase_ok{true};

  auto worker_fn = [&](int widx){
    WorkerInfo &w = workers_[widx];
    while(true){
      int tidx=-1;
      {
        std::unique_lock<std::mutex> lk(m);
        cv.wait(lk,[&]{return !pending.empty()||remaining==0||w.state==WorkerState::DEAD;});
        if(w.state==WorkerState::DEAD || (pending.empty() && remaining==0))
          return;
        if(pending.empty()) 
          continue;
        tidx=pending.front(); pending.pop_front();
        w.state=WorkerState::BUSY; 
        running[widx]=tidx;
        tasks[tidx].start=std::chrono::steady_clock::now();
      }
      bool ok=false; std::string tmp_dir;
      if (phase==Phase::MAP)  ok = doMapTask(tasks[tidx].id, file_shards_[tidx], w, tmp_dir);
      else                    ok = doReduceTask(tasks[tidx].id, w);

      {
          std::lock_guard lk(m);
          running.erase(widx);
          if (!ok) {
              w.state = WorkerState::DEAD;                 // mark dead; heartbeat thread will later attempt revive
              if (!tasks[tidx].done.load()) pending.push_back(tidx); // requeue task
          } else {
              w.state = WorkerState::IDLE;
              if (phase==Phase::MAP) {
                  std::lock_guard tl(tasks[tidx].mu);
                  if (!tasks[tidx].accepted) {
                      tasks[tidx].accepted = true;
                      tasks[tidx].accepted_dir = tmp_dir;
                      {
                          std::lock_guard dirlk(dirs_mu_);
                          intermediate_dirs_.push_back(tmp_dir);
                      }
                  } else {
                      // late speculative copy – discard its output
                      fs::remove_all(tmp_dir);
                  }
              }
              if (!tasks[tidx].done.exchange(true)) --remaining;
          }
      }
      cv.notify_all();
    }
  };

  std::vector<std::thread> threads;
  for(size_t i=0;i<workers_.size();++i) if(workers_[i].state!=WorkerState::DEAD)
    threads.emplace_back(worker_fn, (int)i);


  // monitor for heartbeat
  std::thread heartbeat([&]{
        using namespace std::chrono_literals;
        while (remaining>0) {
            std::this_thread::sleep_for(1500ms);
            for (size_t i=0;i<workers_.size();++i) {
                auto &w = workers_[i];
                if (w.state==WorkerState::DEAD) continue;
                if (!w.channel->WaitForConnected(
                        std::chrono::system_clock::now()+500ms)) {
                    std::lock_guard lk(m);
                    if (w.state==WorkerState::BUSY && running.count((int)i)) {
                        int tidx = running[(int)i];
                        pending.push_back(tidx);                 // requeue in‑flight task
                        running.erase((int)i);
                    }
                    w.state = WorkerState::DEAD;
                    cv.notify_all();
                }
            }
        }
    });

  // monitor for stragglers
  std::thread spec([&]{
    while(remaining>0){ 
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      std::lock_guard<std::mutex> lk(m);
      if(running.empty()) continue;
      auto now=std::chrono::steady_clock::now();
      auto min_rt=std::chrono::milliseconds::max();
      for(auto [widx,tidx]:running){
        auto dur=std::chrono::duration_cast<std::chrono::milliseconds>(now-tasks[tidx].start);
        if(dur<min_rt) min_rt=dur;
      }
      
      auto scaled = std::chrono::milliseconds(static_cast<long long>(min_rt.count() * 2.5));
      auto threshold = std::max(BASE_SPEC_MS, scaled);
      for(auto [widx,tidx]:running){
        auto dur=std::chrono::duration_cast<std::chrono::milliseconds>(now-tasks[tidx].start);
        if (dur > threshold && !tasks[tidx].done.load()) {
          if (std::find(pending.begin(), pending.end(), tidx) == pending.end()) {
            std::cout << "[MASTER]  – speculative re-exec (task="
                      << tidx << ", dur=" << dur.count() << "ms)\n";
            pending.push_back(tidx);
          }
        }
      }
      cv.notify_all();
    }
  });

  for(auto &t:threads) t.join(); 
  heartbeat.join();
  spec.join();


  return phase_ok.load();
}

inline std::string Master::gen_random_id_() const {
  static constexpr char kAlphabet[] =
      "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  thread_local std::mt19937_64 rng{std::random_device{}()};
  std::uniform_int_distribution<size_t> dist(0, sizeof(kAlphabet) - 2);

  std::string id(8, '\0');  // 8‑byte ID
  for (char& c : id) c = kAlphabet[dist(rng)];
  return id;
}

inline void Master::cleanup_output_dir_() {
  fs::path outdir(mr_spec_.output_dir);
  std::error_code ec;
  if (!fs::exists(outdir, ec)) {
    fs::create_directories(outdir, ec);
    if (ec) std::cerr << "[MASTER] Warning: failed to create output_dir '" << outdir
                      << "': " << ec.message() << std::endl;
    return;
  }

  for (auto &entry : fs::directory_iterator(outdir, ec)) {
    fs::remove_all(entry.path(), ec);
    if (ec) {
      std::cerr << "[MASTER] Warning: failed to remove '" << entry.path()
                << "': " << ec.message() << std::endl;
    }
  }
}

inline void Master::cleanup_intermediate_() {
  const fs::path root = fs::path(INTERMEDIATE_ROOT_DIR) / mr_spec_.user_id;

  std::cout << "[MASTER] Removing intermediate files under " << root.string() << std::endl;

  std::error_code ec;
  fs::remove_all(root, ec);
  if (ec) {
    std::cerr << "[MASTER] WARNING: failed to clean '" << root.string()
              << "' : " << ec.message() << '\n';
  } else {
    std::cout << "[MASTER] Removed intermediate files under " << root.string()
              << '\n';
  }
}

inline void Master::print_mr_spec_() const {
    std::cout << "master.h: printing spec..." << std::endl;

    std::cout << "User ID: " << mr_spec_.user_id << "\n";
    std::cout << "Number of Workers: " << mr_spec_.n_workers << "\n";
    std::cout << "Worker Addresses:" << std::endl;
    for (const auto& addr : mr_spec_.worker_ipaddr_ports) {
        std::cout << "  " << addr << std::endl;
    }
    std::cout << "Input Files:" << std::endl;
    for (const auto& file : mr_spec_.input_files) {
        std::cout << "  " << file << std::endl;
    }
    std::cout << "Output Directory: " << mr_spec_.output_dir << std::endl;
    std::cout << "Number of Output Files: " << mr_spec_.n_output_files << std::endl;
    std::cout << "Map Kilobytes: " << mr_spec_.map_kilobytes << "\n" << std::endl;
}

inline void Master::print_file_shards_() const {
    std::cout << "master.h: printing file shards..." << std::endl;
    int i = 0;
    for (const auto& shard : file_shards_) {
        std::cout << "  Shard " << i++ << ":" << std::endl;
        for (const auto& piece : shard.pieces) {
            std::cout << "    " << piece.filepath << " [" << piece.start_offset << ", "
                      << piece.end_offset << ")" << std::endl;
        }
    }
    std::cout << std::endl;
}