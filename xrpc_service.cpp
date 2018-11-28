#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <atomic>

#include <grpcpp/grpcpp.h>
#include "xrpc.grpc.pb.h"

#include "xrpc_service.hpp"
#include "xrpc_handler.hpp"
#include "xrpc_utility.hpp"

using namespace std;

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;
using top::xrpc_request;
using top::xrpc_reply;
using top::xrpc_service;

namespace top {
    namespace rpc {

        class xrpc_serviceimpl final : public top::xrpc_service::Service {

            Status call(ServerContext *context, const xrpc_request *request,
                        xrpc_reply *reply) override
            {                
                string req = request->body();
                // cout << "request:" << endl;
                // cout << req << endl;                
                auto handler (std::make_shared<xrpc_handler> (req));
                handler->handle_request();
                string rsp = handler->get_response();
                reply->set_body(rsp);
                // cout << "response:" << endl;
                // cout << rsp << endl;              

                m_call_num++;
                if(m_call_num %1000 == 0)
                {
                    uint64_t new_timestamp = xrpc_utility::get_timestamp_ms();
                    double rate = 1000000/(new_timestamp - m_last_timestamp);
                    m_last_timestamp = new_timestamp;
		            cout << "server api rate: " << to_string(rate) <<  endl;
                }
                return Status::OK;                    
            }

            private:
             uint32_t m_call_num;
             uint64_t m_last_timestamp;
             bool debug;
        };


        std::atomic<int> grpc_recv_num(0);
        uint64_t grpc_last_timestamp;       

        class xrpc_async_serviceimpl final {
        public:
        xrpc_async_serviceimpl(const std::string & addr) :m_server_address(addr)
        {

        }
        ~xrpc_async_serviceimpl() {
            server_->Shutdown();
            // Always shutdown the completion queue after the server.
            cq_->Shutdown();
        }

        // There is no shutdown handling in this code.
        void Run() {
            ServerBuilder builder;
            // Listen on the given address without any authentication mechanism.
            builder.AddListeningPort(m_server_address, grpc::InsecureServerCredentials());
            // Register "service_" as the instance through which we'll communicate with
            // clients. In this case it corresponds to an *asynchronous* service.
            builder.RegisterService(&service_);
            // Get hold of the completion queue used for the asynchronous communication
            // with the gRPC runtime.
            cq_ = builder.AddCompletionQueue();
            // Finally assemble the server.
            server_ = builder.BuildAndStart();
            std::cout << "Server listening on " << m_server_address << std::endl;

            // Proceed to the server's main loop.
            std::thread t1(&top::rpc::xrpc_async_serviceimpl::HandleRpcs, this);
            std::thread t2(&top::rpc::xrpc_async_serviceimpl::HandleRpcs, this);
            std::thread t3(&top::rpc::xrpc_async_serviceimpl::HandleRpcs, this);
            std::thread t4(&top::rpc::xrpc_async_serviceimpl::HandleRpcs, this);
            std::thread t5(&top::rpc::xrpc_async_serviceimpl::HandleRpcs, this);
            std::thread t6(&top::rpc::xrpc_async_serviceimpl::HandleRpcs, this);      
            t1.join();
            t2.join();
            t3.join();
            t4.join();
            t5.join();
            t6.join();
            
        }

        private:
        // Class encompasing the state and logic needed to serve a request.
        class CallData {
        public:
            // Take in the "service" instance (in this case representing an asynchronous
            // server) and the completion queue "cq" used for asynchronous communication
            // with the gRPC runtime.
            CallData(top::xrpc_service::AsyncService* service, ServerCompletionQueue* cq)
                : service_(service), cq_(cq), responder_(&ctx_), status_(CREATE) {
            // Invoke the serving logic right away.
            Proceed();
            }

            void Proceed() {
            if (status_ == CREATE) {
                // Make this instance progress to the PROCESS state.
                status_ = PROCESS;

                // As part of the initial CREATE state, we *request* that the system
                // start processing SayHello requests. In this request, "this" acts are
                // the tag uniquely identifying the request (so that different CallData
                // instances can serve different requests concurrently), in this case
                // the memory address of this CallData instance.
                service_->Requestcall(&ctx_, &request_, &responder_, cq_, cq_,
                                        this);
            } else if (status_ == PROCESS) {
                // Spawn a new CallData instance to serve new clients while we process
                // the one for this CallData. The instance will deallocate itself as
                // part of its FINISH state.
                new CallData(service_, cq_);

                // The actual processing.
#if 1
                // std::thread::id this_id = std::this_thread::get_id();
                // std::cout << "thread " << this_id << endl;

                string req = request_.body();
                // cout << "request:" << endl;
                // cout << req << endl;                
                auto handler (std::make_shared<xrpc_handler> (req));
                handler->handle_request();
                string rsp = handler->get_response();
                reply_.set_body(rsp);

                int num = grpc_recv_num++;
                if(num % 1000 == 0)
                {
                    uint64_t new_timestamp = xrpc_utility::get_timestamp_ms();
                    double rate = 1000000/(new_timestamp - grpc_last_timestamp);
                    grpc_last_timestamp = new_timestamp;
		            cout << "asyn server api rate: " << to_string(rate) <<  endl;
                }
                // cout << "response:" << endl;
                // cout << rsp << endl  << endl;       
#endif
                // And we are done! Let the gRPC runtime know we've finished, using the
                // memory address of this instance as the uniquely identifying tag for
                // the event.
                status_ = FINISH;
                responder_.Finish(reply_, Status::OK, this);
            } else {
                GPR_ASSERT(status_ == FINISH);
                // Once in the FINISH state, deallocate ourselves (CallData).
                delete this;
            }
            }

        private:
            // The means of communication with the gRPC runtime for an asynchronous
            // server.
            top::xrpc_service::AsyncService* service_;
            // The producer-consumer queue where for asynchronous server notifications.
            ServerCompletionQueue* cq_;
            // Context for the rpc, allowing to tweak aspects of it such as the use
            // of compression, authentication, as well as to send metadata back to the
            // client.
            ServerContext ctx_;

            // What we get from the client.
            xrpc_request request_;
            // What we send back to the client.
            xrpc_reply reply_;            

            // The means to get back to the client.
            ServerAsyncResponseWriter<xrpc_reply> responder_;

            // Let's implement a tiny state machine with the following states.
            enum CallStatus { CREATE, PROCESS, FINISH };
            CallStatus status_;  // The current serving state.
        };

        // This can be run in multiple threads if needed.
        void HandleRpcs() {
            // Spawn a new CallData instance to serve new clients.
            new CallData(&service_, cq_.get());
            void* tag;  // uniquely identifies a request.
            bool ok;
            while (true) {
            // Block waiting to read the next event from the completion queue. The
            // event is uniquely identified by its tag, which in this case is the
            // memory address of a CallData instance.
            // The return value of Next should always be checked. This return value
            // tells us whether there is any kind of event or cq_ is shutting down.
            GPR_ASSERT(cq_->Next(&tag, &ok));
            GPR_ASSERT(ok);
            static_cast<CallData*>(tag)->Proceed();
            
#if 0
            string *req_str = static_cast<string*>(tag);
            cout << "req_str " << req_str << endl;

#endif

            }
        }

        std::unique_ptr<ServerCompletionQueue> cq_;
        top::xrpc_service::AsyncService service_;
        std::unique_ptr<Server> server_;
        std::string m_server_address; 
        };        

        uint32_t xrpc_service::sync_run()
        {
            xrpc_serviceimpl service;

            ServerBuilder builder;
            // Listen on the given address without any authentication mechanism.
            builder.AddListeningPort(m_address, grpc::InsecureServerCredentials());
            // Register "service" as the instance through which we'll communicate with
            // clients. In this case it corresponds to an *synchronous* service.
            builder.RegisterService(&service);
            // Finally assemble the server.
            std::unique_ptr<Server> server(builder.BuildAndStart());
            std::cout << "xrpc grpc server listening on " << m_address << std::endl;

            // Wait for the server to shutdown. Note that some other thread must be
            // responsible for shutting down the server for this call to ever return.
            server->Wait();
        }

        uint32_t xrpc_service::async_run()
        {
            xrpc_async_serviceimpl server(m_address);
            server.Run();            
        }

        uint32_t xrpc_service::run()
        {
            sync_run();           
            //async_run();
        }
    }
}
