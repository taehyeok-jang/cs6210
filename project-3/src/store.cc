#include "threadpool.h"

#include <iostream>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <future>
#include <fstream> 

#include <cassert>

#include <grpc++/grpc++.h>
#include "vendor.grpc.pb.h"
#include "store.grpc.pb.h"
// #include "absl/log/check.h"

#define CHECK(x) assert(x)
#define CHECK_EQ(x, y) assert((x) == (y))

using grpc::Channel;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::ServerAsyncResponseWriter;

using vendor::BidQuery;
using vendor::BidReply;
using vendor::Vendor;

using store::Store;
using store::ProductQuery;
using store::ProductReply;
using store::ProductInfo;

/**
 * gRPC ref.
 * https://github.com/grpc/grpc/blob/master/examples/cpp/helloworld/greeter_async_client2.cc
 * https://github.com/grpc/grpc/blob/master/examples/cpp/helloworld/greeter_async_server.cc
 * https://github.com/grpc/grpc/blob/master/examples/cpp/helloworld/greeter_callback_server.cc
 */

class AsyncClientCall {
	public:
		BidReply reply;
		ClientContext context;
		Status status;
		std::unique_ptr<grpc::ClientAsyncResponseReader<BidReply>> response_reader;
		std::string vendor_address;
	};
	
struct VendorClient {
	std::string address;
	std::unique_ptr<Vendor::Stub> stub;

	VendorClient(const std::string& addr)
		: address(addr), stub(Vendor::NewStub(grpc::CreateChannel(addr, grpc::InsecureChannelCredentials()))) {}
	};

class VendorsClient {
	public:
		VendorsClient(const std::vector<std::string>& vendor_addresses)
			{
			for (const auto& address : vendor_addresses) {
				vendors.emplace_back(address);
			}
		}

		void asyncRequestProductBidsFromAllVendors(
			const std::string& product_name,
			std::vector<std::pair<int, std::string>>& results,
			std::mutex& result_mutex) {
			
			std::vector<std::unique_ptr<AsyncClientCall>> calls;
			std::cout << "Requesting product bids for: " << product_name << std::endl;

			for (auto& vendor : vendors) {
				std::cout << "Requesting from vendor: " << vendor.address << std::endl;
				BidQuery request;
				request.set_product_name(product_name);
	
				auto call = std::make_unique<AsyncClientCall>();
				call->vendor_address = vendor.address;
				call->response_reader = vendor.stub->AsyncgetProductBid(&call->context, request, &cq);
				call->response_reader->Finish(&call->reply, &call->status, (void*)call.get());
	
				calls.push_back(std::move(call));
			}
	
			
			void* got_tag;
			bool ok = false;
			
			for (auto& call : calls) {
				while (cq.Next(&got_tag, &ok)) {
					if (!ok) {
						std::cerr << "RPC failed for product: " << product_name << std::endl;
						continue;
					}
				
					AsyncClientCall* completed_call = static_cast<AsyncClientCall*>(got_tag);
					if (completed_call->status.ok()) {
						std::lock_guard<std::mutex> lock(result_mutex);
						results.emplace_back(std::make_pair(completed_call->reply.price(), completed_call->reply.vendor_id()));
					} else {
						std::cerr << "RPC error: " << completed_call->status.error_message() << std::endl;
					}
					break;
				}
			}
		}
		
	private:
		std::vector<VendorClient> vendors;  
		CompletionQueue cq;
	};


class storeServer {
	public:
		storeServer(size_t max_threads, std::vector<std::string>& vendor_addresses)
			: max_threads(max_threads), pool(max_threads), vendor_addresses_(vendor_addresses){
		}

		void run_server(const std::string server_address)   {		  
			ServerBuilder builder;
    		builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    		builder.RegisterService(&service_);
    		cq_ = builder.AddCompletionQueue();
    		server_ = builder.BuildAndStart();
    		std::cout << "Server listening on " << server_address << std::endl;
				
    		HandleRpcs();
		}
	private:
		size_t max_threads;
		threadpool pool;
		Store::AsyncService service_;
		std::unique_ptr<ServerCompletionQueue> cq_;
		std::unique_ptr<Server> server_;
		std::vector<std::string>& vendor_addresses_;

		class CallData {
			public:
			 	CallData(Store::AsyncService* service, ServerCompletionQueue* cq, threadpool* pool, std::vector<std::string>& vendor_addresses)
					: service_(service), cq_(cq), responder_(&ctx_), status_(CREATE), pool_(pool), vendor_addresses_(vendor_addresses) {
						Proceed();
				}
		   
				void Proceed() {
				   	if (status_ == CREATE) {
						status_ = PROCESS;
					 	service_->RequestgetProducts(&ctx_, &request_, &responder_, cq_, cq_, this);
				   	} else if (status_ == PROCESS) {
					 	pool_->submit([this] {
							std::mutex result_mutex;
							std::vector<std::pair<int, std::string>> results;
							
							// Create the VendorsClient instance and request bids for the product
							VendorsClient vc(vendor_addresses_);
							vc.asyncRequestProductBidsFromAllVendors(request_.product_name(), results, result_mutex);
							
							// After processing, aggregate results into the reply
							for (const auto& result : results) {
								ProductInfo* product_info = reply_.add_products();
								product_info->set_price(result.first);  // Set the price from the vendor bid
								product_info->set_vendor_id(result.second);  // Set the vendor ID
							}
				
							// Once processing is complete, send the response back to the client
							status_ = FINISH;
							responder_.Finish(reply_, Status::OK, this);
						});
				   	} else {
					 	CHECK_EQ(status_, FINISH);
					 	delete this;
				   	}
				}
			
			private:
			 	Store::AsyncService* service_;
				ServerCompletionQueue* cq_;
			 	ServerContext ctx_;
			 	ProductQuery request_;
			 	ProductReply reply_;
				std::vector<std::string>& vendor_addresses_;
			 	ServerAsyncResponseWriter<ProductReply> responder_;
			 	enum CallStatus { CREATE, PROCESS, FINISH };
			 	CallStatus status_;
			 	threadpool* pool_; 
		};
		

		void HandleRpcs() {
			void* tag;
			bool ok;
			while (true) {

			  new CallData(&service_, cq_.get(), &pool, vendor_addresses_);
			  CHECK(cq_->Next(&tag, &ok));
			  CHECK(ok);
			  static_cast<CallData*>(tag)->Proceed();
			}
		}
		
	};


int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "Usage: " << argv[0] << " <vendor_addresses_filepath> <server_ip:port> <max_threads>" << std::endl;
        return EXIT_FAILURE;
    }

    std::string vendor_addresses_filepath = argv[1];
    std::string server_address = argv[2];
    size_t max_threads = std::stoi(argv[3]);

    std::vector<std::string> ip_addrresses;
    std::cout << "Reading vendor addresses from file: " << vendor_addresses_filepath << std::endl;

	std::ifstream myfile (vendor_addresses_filepath);
	if (myfile.is_open()) {
	  	std::string ip_addr;
	  	while (getline(myfile, ip_addr)) {
			ip_addrresses.push_back(ip_addr); 
	  	}
	 	myfile.close();
	}
	else {
	  	std::cerr << "Failed to open file " << vendor_addresses_filepath << std::endl;
	  	return EXIT_FAILURE;
	}

	std::cout << "Loaded " << ip_addrresses.size() << " vendor addresses." << std::endl;

	storeServer ss(max_threads, ip_addrresses);
	ss.run_server(server_address);

	return EXIT_SUCCESS;
}
