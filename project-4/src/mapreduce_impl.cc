#include <iostream>

#include "mapreduce_impl.h"
#include "master.h"


/* DON'T touch this function */
bool MapReduceImpl::run(const std::string& config_filename) {
    std::cout << "\n================================================" << std::endl;
    std::cout << "mapreduce_impl.cc: run()..." << std::endl;

    if(!read_and_validate_spec(config_filename)) {
        std::cerr << "Spec not configured properly." << std::endl;
        return false;
    }

    if(!create_shards()) {
        std::cerr << "Failed to create shards." << std::endl;
        return false;
    }

    if(!run_master()) {
        std::cerr << "MapReduce failure. Something didn't go well!" << std::endl;
        return false;
    }

    return true;
}


/* DON'T touch this function */
bool MapReduceImpl::read_and_validate_spec(const std::string& config_filename) {
    std::cout << "mapreduce_impl.cc: reading and validating spec..." << std::endl;
    return read_mr_spec_from_config_file(config_filename, mr_spec_) && validate_mr_spec(mr_spec_);
}


/* DON'T touch this function */
bool MapReduceImpl::create_shards() {
    std::cout << "mapreduce_impl.cc: creating shards..." << std::endl;
    return shard_files(mr_spec_, file_shards_);
}


/* DON'T touch this function */
bool MapReduceImpl::run_master() {
    std::cout << "mapreduce_impl.cc: running master..." << std::endl;
    Master master(mr_spec_, file_shards_);
    return master.run();
}
