# Async Store Server with gRPC

This project builds an **asynchronous gRPC server** called “store” that receives queries from clients and, **asynchronously** requests bids from multiple vendor servers. The server uses a **thread pool** to handle multiple client requests concurrently.

## Overview

1. **Client → Store**  
   A client calls the `Store::getProducts` RPC to ask for product prices.
2. **Store → Vendors**  
   "store" server then calls each vendor server **asynchronously** to fetch bids (prices) for the requested product.
3. **Respond to Client**  
   The server waits for all responses from vendor servers, collates them, and sends a list of bids back to the client.

## The Communication Pipelines
"store" server initializes a fixed thread pool and upstream vendor servers that are identified by `vendor_addresses.txt`. It launches an asynchronous gRPC server using Store::AsyncService, which listens for incoming client requests.

When a client sends a getProducts request, the server uses a `CompletionQueue` to accept it and creates a `CallData` instance to manage its lifecycle. This request is then handed off to the thread pool. Inside the thread, the store issues asynchronous gRPC calls to each vendor to request price bids for the given product. These calls are non-blocking, allowing the thread to wait for all vendor responses without stalling the system.

Once all responses are received, the server collates them into a ProductReply message and sends it back to the client using a `ServerAsyncResponseWriter`. The worker thread then returns to the pool, ready for new tasks. This design allows multiple client requests to be handled concurrently.


## Thread Pool Implementation 

We implemented **a fixed-size thread pool**, motivated by [Java 8's  Executors.newFixedThreadPool](https://docs.oracle.com/javase/8/docs/api/java/util/concurrent/Executors.html#newFixedThreadPool-int-). When the thread pool is created, it spawns max_threads worker threads. Each worker enters an indefinite loop, waiting for tasks to execute. Upon destruction of the thread pool, all internal threads are gracefully terminated using a ‘stop’ flag combined with notify_all().

To submit a task, the function ‘f’ is wrapped using `std::packaged_task<return_type()>` for proper lifetime and result management. A lambda wrapping the task is pushed into the internal task queue. The submit function returns a `std::future`, allowing the caller to retrieve the result once the task is completed.

Idle worker threads wait on a condition variable until a new task is available in the queue. When a task arrives, a worker thread dequeues and executes it immediately. This ensures efficient background processing while minimizing idle CPU usage.

## How gRPC Works

- **Server-Side Asynchronous**:  
  - `Store::AsyncService` + a `ServerCompletionQueue` allow the server to handle multiple calls simultaneously without blocking.  

- **Client-Side Asynchronous**:  
  - For each vendor, the store server creates a stub (`Vendor::Stub`) and issues an **asynchronous** call to `getProductBid`.  

## Summary

This asynchronous Store server uses gRPC in two ways:
- **Downstream** (from **user** client to **store**).
- **Upstream** (from **store** to **vendor servers**).

A **thread pool** manages concurrency, ensuring that multiple client requests can be processed without blocking. Using **gRPC’s asynchronous APIs**, each thread can fan out calls to vendors, await responses, and collect bids. Once all bids are joined, the store returns a comprehensive product bid list to the user.