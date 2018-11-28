#include <unistd.h>
#include <string>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <msgpack.hpp>
#include "xserialize.h"
#include "xnet.h"
#include "xmessage_dispatcher.hpp"
#include "xstore_manager.h"
#include "xckey.h"
#include "xhash.h"
#include "xchaincore.h"
#include <sys/time.h>
#include "stdlib.h"
#include "xconfig.hpp"
#include "xlog.h"
#include "xstring.h"
#include "xtimer.h"

#include "xrpc_server.hpp"
#include "xrpc_handler.hpp"

using namespace std;
using namespace top;
using namespace top::rpc;

using asio::ip::tcp;

void xrpc_connection::do_read_http_body()
{    
    auto self(shared_from_this());
    // TODO need read the complete body
    async_read_until(m_socket, m_request, "}", [this, self](std::error_code ec, std::size_t bytes_transferred){
        if(!ec)
        {
            asio::streambuf::const_buffers_type cbt = m_request.data();
            string request_data(asio::buffers_begin(cbt), asio::buffers_end(cbt));
            xinfo("rpcserver: http request %d", bytes_transferred);
            //m_request.consume(bytes_transferred);
            
            auto handler (std::make_shared<xrpc_handler> (request_data));
            handler->handle_request();
            string rsp = handler->get_response();
            do_write(rsp);
        }
        else
        {
            xwarn("rpcserver: socket async_read_until error");
        }
    });
}


bool xrpc_connection::parse_method()
{
    auto self(shared_from_this());
    async_read_until(m_socket, m_request, " ", [this, self](std::error_code ec, std::size_t bytes_transferred){
        if(!ec)
        {
            asio::streambuf::const_buffers_type cbt = m_request.data();
            string method(asio::buffers_begin(cbt), asio::buffers_begin(cbt) + bytes_transferred - 1);
            if (!method.empty())
            {
                if (method.compare("OPTIONS") == 0)
                {
                on_options();
                return false;
                }
                else if (method.compare("POST") == 0)
                {
                on_post();
                return false;
                }
            }
            return true;
        }
        else
        {
            xwarn("xrpc_connection: socket async_read_until parse_method error");
        }
    });
}

void xrpc_connection::on_post()
{
  auto self(shared_from_this());

  async_read_until(m_socket, m_request, "\r\n\r\n", [this, self](std::error_code ec, std::size_t bytes_transferred) {
    if (!ec)
    {
#if 0
      asio::streambuf::const_buffers_type cbt = m_request.data();
      string request_data(asio::buffers_begin(cbt), asio::buffers_begin(cbt) + bytes_transferred);
      cout << "http header: " << bytes_transferred << endl;
      cout << request_data << endl;
#endif
      m_request.consume(bytes_transferred);

      do_read_http_body();
    }
    else
    {
        xwarn("xrpc_connection: socket async_read_until on_post error");
    }
  });
}

void xrpc_connection::on_options()
{
  std::ostream rsp_stream(&m_response);
  rsp_stream << "HTTP/1.1 200 OK\r\n";
  rsp_stream << "Access-Control-Allow-Origin:*\r\n";
  rsp_stream << "Access-Control-Request-Method:POST\r\n";
  rsp_stream << "Access-Control-Allow-Headers:Origin,X-Requested-With,Content-Type,Accept\r\n";
  rsp_stream << "Access-Control-Request-Headers:Content-type\r\n";
  // rsp_stream << "Content-Type:application/json;charset=utf-8\r\n";
  auto self(shared_from_this());
  asio::async_write(m_socket, m_response, [this, self](std::error_code ec, std::size_t bytes_transferred) {

  });
}

void xrpc_connection::on_error()
{
  std::ostream rsp_stream(&m_response);
  rsp_stream << "HTTP/1.0 403 Forbidden\r\n";
  auto self(shared_from_this());
  asio::async_write(m_socket, m_response, [this, self](std::error_code ec, std::size_t bytes_transferred) {

  });
}

void xrpc_connection::do_read()
{
  if (!parse_method())
  {
    return;
  }
  xwarn("rpcserver: socket async_read_until error");
  on_error();
}

void xrpc_connection::do_write(string &rsp)
{
    std::ostream rsp_stream(&m_response);

    if(rsp.empty())
    {
        rsp_stream << "HTTP/1.1 201 Error\r\n";
    }   
    else
    {
        rsp_stream << "HTTP/1.1 200 OK\r\n";
        rsp_stream << "Access-Control-Allow-Origin: *\r\n";
        rsp_stream << "Content-Type: " << "application/json" << "\r\n";
        rsp_stream << "Content-Length: " << rsp.length() << "\r\n";
        rsp_stream << "\r\n";
        rsp_stream << rsp << "\r\n";
    } 
        
    auto self(shared_from_this());    
    asio::async_write(m_socket, m_response, [this, self](std::error_code ec, std::size_t bytes_transferred)
    {
        //self->handle_accept();
        //return;
    });
}

void xrpc_connection::handle_accept()
{
    do_read();
}

xrpc_server::xrpc_server(const std::string &host, const uint16_t port)
    : m_acceptor(m_io_service, tcp::endpoint(asio::ip::address_v4::from_string(host), port))
{
    std::cout << "xrpc http server listening on " << host+":"+to_string(port) << std::endl;
}

xrpc_server::~xrpc_server()
{
}

void xrpc_server::do_accept()
{
    auto connection (std::make_shared<xrpc_connection> (m_io_service));
    m_acceptor.async_accept(connection->m_socket, [this, connection](const asio::error_code &ec) {		
		if (!ec)
        {
			connection->handle_accept ();
            do_accept ();
		}
		else
		{
            xwarn("rpcserver: async_accept error");
		}
	});
}

void test()
{
    uint8_t* pData;

    //创建几个测试账户
    top::utl::xecprikey_t pri_key_obj;
    std::cout << "private key: ";
    pData = pri_key_obj.data();
    for(int i=0;i<pri_key_obj.size();i++)
    {
        printf("%02x",pData[i]);
    }
    std::cout << std::endl;

    top::utl::xecpubkey_t pub_key_obj = pri_key_obj.get_public_key();
    std::cout << "public key: ";
    pData = pub_key_obj.data();
    for(int i=0;i<pub_key_obj.size();i++)
    {
        printf("%02x",pData[i]);
    }
    std::cout << std::endl;

    std::string address = pub_key_obj.to_address();
    std::cout << "address: " << address << std::endl;
}

bool genesis_account_create()
{
    uint8_t  test_genesis_account_pri_key[32] = {0x83,0xab,0x81,0xe3,0xcf,0xe1,0x44,0x25,0xe5,0x30,0xea,0xb1,0xd3,0x87,0xc6,0xff,0x80,0x50,0x63,0x73,0xc0,0xa1,0xcf,0x5c,0x63,0xc7,0x55,0x1c,0x68,0x40,0x06,0xed};
    top::utl::xecprikey_t pri_key_obj(test_genesis_account_pri_key);

    std::string test_genesis_account_address = "T-Um45LZ8HZYUpm5jdWi6bh99mdNjEHkz6w";    

    shared_ptr<top::chain::xtransaction_t> tx = make_shared<top::chain::xtransaction_t>();
    tx->m_transaction_type = top::chain::xtransbase_t::enum_xtransaction_type_create_account;
    tx->m_sender_addr = test_genesis_account_address;
    tx->m_receiver_addr = test_genesis_account_address;
    tx->m_property_key = "$$";
    tx->m_transaction_params = "200000000000";
    tx->m_timestamp = 0;    
    //tx->m_last_msg_digest;
    tx->m_random_nounce = 0;
    tx->m_work_proof = 0;

    tx->tx_hash_and_sign_calc(pri_key_obj);
    tx->m_edge_timestamp = top::base::xtimer_t::timestamp_now_ms();
    //对hash和sign进行校验
    if(false == tx->tx_hash_and_sign_verify())
    {
        xwarn("rpcserver: genesis_account_create tx_hash_and_sign_verify error");
        return false;
    }
	shared_ptr<top::chain::xfullunit_t> fullunit = make_shared<top::chain::xfullunit_t>();
	fullunit->m_asset_properties.insert(std::make_pair(tx->m_property_key, std::stoll(tx->m_transaction_params)));
	fullunit->m_unit_type = chain::x_unit_type::full_unit_type;
	fullunit->m_seq_id = 1;
	fullunit->m_associated_event_hash = tx->m_tx_digest;
    fullunit->create_unit_digest_and_signature(pri_key_obj);
	
    top::store::xstore_base* m_store_mgr = xsingleton<top::store::xstore_manager>::Instance();
	
	chain::pbft_store_params_t event;
	event.m_transbase = tx;
	event.m_unit = fullunit;
	
    auto ret = m_store_mgr->update_event(event);
    if(ret != top::store::enum_store_return_type::store_ok && ret != top::store::enum_store_return_type::create_account_is_exist)
    {
        xwarn("rpcserver: genesis_account_create update_event error");
        return false;
    }

    return true;
}

uint32_t xrpc_server::run()
{
    if(false == genesis_account_create())
    {
        return 0;
    }

    while(1)
    {
        try
        {       
            do_accept();
            m_io_service.run();
        }
        catch (std::exception& e)
        {
            std::cerr << "xrpc http server exception " << e.what() << std::endl;
        }    
    }

    std::cout << "rpc server exit" << std::endl;
    return 0;
}
