#ifndef rpc_server_hpp
#define rpc_server_hpp

#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE
#endif//ASIO_STANDALONE

#include <iostream>
#include <string>
#include "xchaincore.h"
#include "xnet.h"
#include "asio.hpp"
#include "json/json.h"

using asio::ip::tcp;
namespace top {
    class xrpc_connection : public std::enable_shared_from_this<xrpc_connection>
    {
    public:
        xrpc_connection(asio::io_service &m_io): m_io_service(m_io), m_socket(m_io)
        {
        }
        void handle_accept();
        asio::ip::tcp::socket m_socket;

    private:
        void do_read();
        bool parse_method();
        void on_options();
        void on_post();
        void on_error();
        void do_read_http_body();
        void do_write(std::string &rsp);
    private:
        asio::io_service &m_io_service;
        asio::streambuf m_request;
        asio::streambuf m_response;
    };

    class xrpc_server
    {
    public:
        xrpc_server(const std::string & host, const uint16_t port);
        virtual ~xrpc_server();
        uint32_t run();
    private:
        void do_accept();
        asio::io_service m_io_service;
        tcp::acceptor m_acceptor;
    };
}

#endif /* rpc_server_hpp */
