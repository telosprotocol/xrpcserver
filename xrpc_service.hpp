#pragma once

#include <string>

namespace top {
    namespace rpc {
        class xrpc_service
        {
        public:
            xrpc_service(const std::string & host, const uint16_t port):m_address(host+":"+std::to_string(port))
            {
            }
            virtual ~xrpc_service()
            {                
            }
            uint32_t run();
        private:        
            uint32_t sync_run();
            uint32_t async_run();
            std::string m_address;
        };
    }
}


