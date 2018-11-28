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
#include "xrpc_utility.hpp"
#include "xrpc_base.hpp"

using namespace std;

namespace top {
    namespace rpc {

        uint64_t get_timestamp()
        {
            struct timeval val;
            gettimeofday(&val, NULL);
            return (uint64_t)val.tv_sec;
        }

        uint64_t get_transaction_unique_id()
        {
            return get_timestamp() + (uint64_t)rand();
        }

        xrpc_handler::xrpc_handler(const std::string &req) : m_request_http(req)
        {
            xJson::Reader reader;
            if (!reader.parse(m_request_http, m_root)) {
                set_result_fail(xrpc_server_json_parse_err);
                return;
            }
            xkinfo("rpc request:%s", m_request_http.c_str());
        }

        xrpc_handler::xrpc_handler(const xJson::Value &req) {
            xJson::FastWriter writer;
            m_request_http = writer.write(req);
            m_root = req;
        }

        std::string xrpc_handler::get_action() {
            return m_root["action"].asString();
        }
        uint256_t xrpc_handler::get_tx_digest() {
            return tx.m_tx_digest;
        }

        xJson::Value xrpc_handler::get_response_ex() {
            return m_js_rsp;
        }

        void xrpc_handler::handle_request()
        {
            string action = m_root["action"].asString();
            //bool nohash = m_root["digest"].asString().empty() ? true : false;
            bool nohash = no_hash(m_root["account"].asString());

            if (action == "account_balance")
            {
                account_balance();
            }
            else if (action == "account_create")
            {
                account_create(nohash);
            }
            else if (action == "account_info")
            {
                account_info();
            }
            else if (action == "transfer_out")
            {
                nohash = no_hash(m_root["source"].asString());
                transfer_out(nohash);
            }
            else if (action == "transfer_in")
            {
                transfer_in(nohash);
            }
            else if (action == "account_history")
            {
                account_history();
            }
            else if (action == "account_pending")
            {
                account_pending();
            }
            else if (action == "last_digest")
            {
                last_digest();
            }
			else if (action == "tps_query")
			{
				query_tps();
			}
            else if (action == "set_property")
			{
				set_property(nohash);
			}
            else if (action == "query_property")
			{
				query_property();
			}
            else if (action == "query_all_property")
			{
				query_all_property();
			}
            else if (action == "give")
            {
                give();
            }
            else if (action == "query_online_tx")
            {
                query_online_tx();
            }
            else if (action == "account_delete")
            {
                account_delete();
            }
            else if (action == "account_snap_delete")
            {
                account_snap_delete();
            }
            else if (action == "account_sync")
            {
                account_sync();
            }
            else if (action == "trigger_elect")
            {
                trigger_elect();
            }
            else
            {
                set_result_fail(xrpc_server_action_invalid);
            }
        }

        std::string xrpc_handler::get_response()
        {
            if(m_js_rsp.empty())
            {
                set_result_fail(xrpc_server_unkown_err);
            }

            return m_js_rsp.toStyledString();
        }

        void xrpc_handler::account_balance()
        {
            string account = m_root["account"].asString();
            string asset_key = m_root["property_key"].asString();

            std::string balance;
            top::store::xstore_base* store_intf = xsingleton<top::store::xstore_manager>::Instance();
			int32_t ret = store_intf->get_account_balance(account, asset_key, balance);
            if(store::store_intf_return_type::account_not_exist == ret)
            {
                set_result_fail(xrpc_server_account_not_found);
                return;
            }
			else if(store::store_intf_return_type::no_property_key == ret)
			{
				set_result_fail(xrpc_no_property_key);
                return;
			}

            set_result_ok();
            m_js_rsp["balance"] = balance;
        }

        void xrpc_handler::account_info()
        {
            string account = m_root["account"].asString();
            string asset_key = "$$";

            chain::xaccount_snap_t account_snap;
            top::store::xstore_base* store_intf = xsingleton<top::store::xstore_manager>::Instance();
            if(!store_intf->get_account_snap_info(account, account_snap))
            {
                set_result_fail(xrpc_server_account_not_found);
                return;
            }
            string balance = "0";
            account_snap.get_balance(asset_key, balance);

            set_result_ok();
            m_js_rsp["balance"] = balance;
            m_js_rsp["sequence"] = std::to_string(account_snap.m_sequence_id);
            uint256_t last_hash = account_snap.m_unit_snap.back()->m_associated_event_hash;
            m_js_rsp["lasthash"] = string_utl::base64_encode(last_hash.data(),last_hash.size());
            m_js_rsp["createtime"] = std::to_string(account_snap.m_create_time);
            //set property value
            string property_value;
            account_snap.get_property("@name", property_value);
            m_js_rsp["property_value"] = property_value;
            m_js_rsp["give"] = 1;
            //Response will start with the latest block for the account
            std::vector<std::shared_ptr<chain::xtransaction_t>> settle_list;
            store_intf->get_settle_blocks(account, settle_list);
            for(auto iter=settle_list.begin(); iter<settle_list.end();iter++)
            {
                if((*iter)->m_transaction_type == chain::xtransbase_t::enum_xtransaction_type::enum_xtransaction_type_transfer_asset_in)
                {
                    m_js_rsp["give"] = 0;
                    return;
                }
            }
            std::vector<std::shared_ptr<chain::xtransaction_t>> receive_list;
            store_intf->get_receive_blocks(account, receive_list);
            if(receive_list.size() > 0)
			{
				 m_js_rsp["give"] = 0;
                return;
            }

        }

        void xrpc_handler::set_property_value(std::unordered_map<std::string, std::string>& user_tag)
        {
            top::store::xstore_base* store_intf = xsingleton<top::store::xstore_manager>::Instance();
            auto iter = user_tag.find(m_js_item["source"].asString());
            if(iter != user_tag.end())
            {
                m_js_item["source_tag"] = iter->second;
            }
            else
            {
                m_js_item["source_tag"] = "";
                chain::xaccount_snap_t account_snap;
                std::string property_value;
                if(store_intf->get_account_snap_info(m_js_item["source"].asString(), account_snap))
                {
                    if(account_snap.get_property(TAG_NAME, property_value))
                    {
                        m_js_item["source_tag"] = property_value;
                    }
                }
                user_tag.insert(std::make_pair(m_js_item["source"].asString(), property_value));
            }

            iter = user_tag.find(m_js_item["destination"].asString());
            if(iter != user_tag.end())
            {
                m_js_item["destination_tag"] = iter->second;
            }
            else
            {
                m_js_item["destination_tag"] = "";
                chain::xaccount_snap_t account_snap;
                std::string property_value;
                if(store_intf->get_account_snap_info(m_js_item["destination"].asString(), account_snap))
                {
                    if(account_snap.get_property(TAG_NAME, property_value))
                    {
                        m_js_item["destination_tag"] = property_value;
                    }
                }
                user_tag.insert(std::make_pair(m_js_item["destination"].asString(), property_value));
            }
            set_result_ok();
        }

        //查询账户下的历史交易，指的是区块链，还是交易链？
        void xrpc_handler::account_history()
        {
            string account = m_root["account"].asString();
            int count = m_root["count"].asInt();
            int per_page = m_root["per_page"].asInt();
            int start_num = count * per_page;
            int end_num = (count + 1) * per_page;
            int num = 0;

            std::vector<std::shared_ptr<chain::xtransaction_t>> settle_list;
            top::store::xstore_base* store_intf = xsingleton<top::store::xstore_manager>::Instance();
            store_intf->get_settle_blocks(account, settle_list);

            set_result_ok();
            std::unordered_map<std::string, std::string> user_tag;
            //Response will start with the latest block for the account
            for(auto iter=settle_list.begin(); iter<settle_list.end();iter++)
            {
                if((*iter)->m_transaction_type != chain::xtransbase_t::enum_xtransaction_type::enum_xtransaction_type_transfer_asset_in &&
                (*iter)->m_transaction_type != chain::xtransbase_t::enum_xtransaction_type::enum_xtransaction_type_transfer_asset_out)
                {
                    continue;
                }
                
                if(num >= start_num && num < end_num)
                {
                    m_js_item["transaction_type"] = (*iter)->m_transaction_type;
                    m_js_item["source"] = (*iter)->m_sender_addr;
                    m_js_item["destination"] = (*iter)->m_receiver_addr;
                    m_js_item["property_key"] = (*iter)->m_property_key.empty() ? "$$" : (*iter)->m_property_key;
                    if((*iter)->m_transaction_type == 1 || (*iter)->m_transaction_type == 0)
                    {//todo
                            m_js_item["amount"] = (*iter)->m_transaction_params;
                    }
                    else if((*iter)->m_transaction_type == chain::xtransbase_t::enum_xtransaction_type::enum_xtransaction_type_transfer_asset_in)
                    {
                            m_js_item["amount"] = (*iter)->m_origin_amount;
                            m_js_item["source"] = (*iter)->m_origin_sender;
                    }
                    set_property_value(user_tag);
                    m_js_item["tx_digest"] = string_utl::base64_encode((*iter)->m_tx_digest.data(),(*iter)->m_tx_digest.size());
                    m_js_item["last_digest"] = string_utl::base64_encode((*iter)->m_last_msg_digest.data(),(*iter)->m_last_msg_digest.size());
                    m_js_item["rpc_time"] = std::to_string((*iter)->m_edge_timestamp);
                    m_js_arrayObj.append(m_js_item);
                }
                ++num;
            }
            m_js_rsp["blocks"] = m_js_arrayObj;
            m_js_rsp["count"] = std::to_string(num);
        }

        void xrpc_handler::last_digest()
        {
            string account = m_root["account"].asString();
            int num = 0;

            top::store::xstore_base* m_store_mgr = xsingleton<top::store::xstore_manager>::Instance();
            chain::xaccount_snap_t local_account;
            bool find = m_store_mgr->get_account_snap_info(account, local_account);
            if(!find)
            {
                set_result_fail(xrpc_server_account_not_found);
                return;
            }

            set_result_ok();
            uint256_t last_hash = local_account.m_unit_snap.back()->m_associated_event_hash;
            m_js_rsp["last_digest"] = string_utl::base64_encode(last_hash.data(),last_hash.size());
        }

        void xrpc_handler::account_pending()
        {
            string account = m_root["account"].asString();
            int count = m_root["count"].asInt();
            int num = 0;

            std::vector<std::shared_ptr<chain::xtransaction_t>> receive_list;
            top::store::xstore_base* store_intf = xsingleton<top::store::xstore_manager>::Instance();
            store_intf->get_receive_blocks(account, receive_list);

            set_result_ok();
            //Response will start with the latest block for the account
            for(auto iter=receive_list.begin(); iter<receive_list.end();iter++)
            {
                if(num++ >= count)
                {
                    break;
                }

                m_js_item["sender"] = (*iter)->m_sender_addr;
                m_js_item["property_key"] = (*iter)->m_property_key;
                m_js_item["amount"] = (*iter)->m_transaction_params;
                m_js_item["block"] = string_utl::base64_encode((*iter)->m_tx_digest.data(), (*iter)->m_tx_digest.size());
                m_js_arrayObj.append(m_js_item);
            }
            m_js_rsp["blocks"] = m_js_arrayObj;
            m_js_rsp["count"] = std::to_string(receive_list.size());
        }

        bool xrpc_handler::last_digest_set(bool nohash, const string & account, uint256_t & last_hash)
        {
            if(nohash)
            {
                top::store::xstore_base* m_store_mgr = xsingleton<top::store::xstore_manager>::Instance();
                chain::xaccount_snap_t local_account;
                bool find = m_store_mgr->get_account_snap_info(account, local_account);
                if(find)
                {
                    last_hash = local_account.m_unit_snap.back()->m_associated_event_hash;
                }
                else
                {
                    return false;
                }
            }
            else
            {
                last_hash = uint256_t((uint8_t*)string_utl::base64_decode(m_root["last_digest"].asString()).c_str());
            }
            return true;
        }

        void xrpc_handler::tx_digest_signature_set(bool nohash)
        {
            if(nohash)
            {
                //tx.m_tx_digest = xrpc_utility::json_string_to_hash(m_root["digest"].asString());
                transaction_digest_add();
            }
            else
            {
                tx.m_tx_signature = xrpc_utility::json_string_to_signature(m_root["signature"].asString());
            }
            tx.m_edge_timestamp = top::base::xtimer_t::timestamp_now_ms();
        }

        void xrpc_handler::account_create(bool nohash)
        {
            xJson::Value resultJson;
            if(!check_input(m_root, resultJson))
            {
                return;
            }

            xrpc_utility::transaction_make_account_create(m_root["account"].asString(), m_root["amount"].asString(), m_root["timestamp"].asInt64(), tx);
            tx_digest_signature_set(nohash);

            if(false == tx.tx_hash_and_sign_verify())
            {
                // TODO 具体原因再细化，还有很多地方
                set_result_fail(xrpc_server_hash_signature_check_fail);
                xwarn("hash and signature check fail");
                return;
            }

            do_transaction();

            transaction_retry_timer_start();

            set_result_ok();
        }

        void xrpc_handler::transfer_out(bool nohash)
        {
            uint256_t last_hash;
            string src_account = m_root["source"].asString();
            string dst_account = m_root["destination"].asString();
            string asset_key = m_root["property_key"].asString();
            string ammount =  m_root["amount"].asString();
            uint64_t timestamp = m_root["timestamp"].asInt64();

            if(src_account == dst_account)
            {
                set_result_fail(xrpc_sender_receiver_the_same);
                return;
            }
            if(false == last_digest_set(nohash, src_account, last_hash))
            {
                set_result_fail(xrpc_server_account_not_found);
                return;
            }
            xrpc_utility::transaction_make_transfer_out(src_account, dst_account, asset_key,
                                        ammount, timestamp, last_hash, tx);
            tx_digest_signature_set(nohash);

            if(false == tx.tx_hash_and_sign_verify())
            {
                set_result_fail(xrpc_server_hash_signature_check_fail);
                return;
            }

            std::string balance;
            top::store::xstore_base* store_intf = xsingleton<top::store::xstore_manager>::Instance();
			int32_t ret = store_intf->get_account_balance(src_account, asset_key, balance);
            if(store::store_intf_return_type::account_not_exist == ret)
            {
                set_result_fail(xrpc_server_account_not_found);
                return;
            }
			else if(store::store_intf_return_type::no_property_key == ret)
			{
				set_result_fail(xrpc_no_property_key);
                return;
			}

            if(std::stoll(balance) < std::stoll(ammount))
            {
                set_result_fail(xrpc_server_account_balance_not_enough);
                return;                
            }

            //check last hash
            uint256_t acct_last_hash;
            if(store_intf->get_last_hash(src_account, acct_last_hash) && acct_last_hash != tx.m_last_msg_digest)
            {
                set_result_fail(xrpc_last_hash_error);
                return;   
            }

            do_transaction();

            transaction_retry_timer_start();

            set_result_ok();
        }


       void xrpc_handler::give()
        {
            uint256_t last_hash;
            string src_account = "T-Um45LZ8HZYUpm5jdWi6bh99mdNjEHkz6w";
            string dst_account = m_root["destination"].asString();
            string asset_key = "$$";
            string ammount =  "100000";
            uint64_t timestamp = m_root["timestamp"].asInt64();
            
            if(false == last_digest_set(true, src_account, last_hash))
            {
                set_result_fail(xrpc_server_account_not_found);
                return;
            }
            xrpc_utility::transaction_make_transfer_out(src_account, dst_account, asset_key,
                                        ammount, timestamp, last_hash, tx);
            tx_digest_signature_set(true);
            
            if(false == tx.tx_hash_and_sign_verify())
            {
                set_result_fail(xrpc_server_hash_signature_check_fail);
                return;
            }
           
            top::store::xstore_base* store_intf = xsingleton<top::store::xstore_manager>::Instance();

            //
            if(!store_intf->exist_account(dst_account))
            {
                set_result_fail(xrpc_server_account_not_found);
                return;
            }
            std::vector<std::shared_ptr<chain::xtransaction_t>> receive_list;
            store_intf->get_receive_blocks(dst_account, receive_list);
            if(receive_list.size() > 0)
			{
				set_result_fail(xrpc_already_give);
                return;
            }
            std::vector<std::shared_ptr<chain::xtransaction_t>> settle_list;
            store_intf->get_settle_blocks(dst_account, settle_list);

            //Response will start with the latest block for the account
            for(auto iter=settle_list.begin(); iter<settle_list.end();iter++)
            {
                if((*iter)->m_transaction_type == chain::xtransbase_t::enum_xtransaction_type::enum_xtransaction_type_transfer_asset_in)
                {
				    set_result_fail(xrpc_already_give);
                    return;
                }
            }

            do_transaction();

            transaction_retry_timer_start();

            set_result_ok();
        }

        void xrpc_handler::query_online_tx()
        {
            int count = m_root["count"].asInt();
            int per_page = m_root["per_page"].asInt();
            int start_num = count * per_page;
            int end_num = (count + 1) * per_page;
            int num = 0;
            top::store::xstore_base* store_intf = xsingleton<top::store::xstore_manager>::Instance();
            std::deque<shared_ptr<chain::xtransaction_t>> tx_list;
            if(!store_intf->query_online_tx(tx_list))
            {
                set_result_fail(xrpc_server_unkown_err);
                return;
            }
            //Response will start with the latest block for the account
            for(auto iter=tx_list.begin(); iter<tx_list.end();iter++)
            {
                if(num >= start_num && num < end_num)
                {
                m_js_item["sender"] = (*iter)->m_sender_addr;
                m_js_item["receiver"] = (*iter)->m_receiver_addr;
                m_js_item["property_key"] = (*iter)->m_property_key;
                m_js_item["amount"] = (*iter)->m_transaction_params;
                m_js_item["timestamp"] = std::to_string((*iter)->m_timestamp);
                m_js_item["last_digest"] = string_utl::base64_encode((*iter)->m_last_msg_digest.data(),(*iter)->m_last_msg_digest.size());
                m_js_item["tx_digest"] = string_utl::base64_encode((*iter)->m_tx_digest.data(),(*iter)->m_tx_digest.size());
                m_js_item["rpc_time"] = std::to_string((*iter)->m_edge_timestamp);
                m_js_arrayObj.append(m_js_item);
            }
                ++num;
            }
            m_js_rsp["blocks"] = m_js_arrayObj;
            m_js_rsp["count"] = std::to_string(tx_list.size());
            set_result_ok();
        }


        void xrpc_handler::transfer_in(bool nohash)
        {
            uint64_t timestamp = m_root["timestamp"].asInt64();
            uint256_t last_hash;
            if(false == last_digest_set(nohash, m_root["account"].asString(), last_hash))
            {
                set_result_fail(xrpc_server_account_not_found);
                return;
            }
            uint256_t tx_digest = uint256_t((uint8_t*)string_utl::base64_decode(m_root["tx_digest"].asString()).c_str());

            xrpc_utility::transaction_make_transfer_in(m_root["account"].asString(), timestamp, tx_digest, last_hash, tx);
            tx_digest_signature_set(nohash);
            tx.m_origin_sender = m_root["sender"].asString();
            tx.m_origin_amount = m_root["amount"].asString();

            if(false == tx.tx_hash_and_sign_verify())
            {
                set_result_fail(xrpc_server_hash_signature_check_fail);
                xwarn("hash and signature check fail");
                return;
            }
            //check last hash
            uint256_t acct_last_hash;
            top::store::xstore_base* store_intf = xsingleton<top::store::xstore_manager>::Instance();
            if(store_intf->get_last_hash(tx.m_sender_addr, acct_last_hash) && acct_last_hash != tx.m_last_msg_digest)
            {
                set_result_fail(xrpc_last_hash_error);
                return;   
            }

            do_transaction();
            //transaction_retry_timer_start();
            set_result_ok();
        }

		void xrpc_handler::query_tps()
		{
			top::store::xstore_base* store_intf = xsingleton<top::store::xstore_manager>::Instance();
			xJson::Value result =std::move(store_intf->get_stat_info());
			if (result.empty())
			{
				set_result_fail(xrpc_no_stat_info);
			}
			else
			{
				m_js_rsp = std::move(result);
				set_result_ok();
			}
		}

        void xrpc_handler::account_delete()
        { 
            //first, delete account on local node           
            top::store::xstore_base* store_base = xsingleton<top::store::xstore_manager>::Instance();
            store_base->delete_account(m_root["account"].asString());
            //second, broad msg to delete account on network nodes
            top::store::xsync_account::get_instance().account_delete(m_root["account"].asString());
            set_result_ok();
        }

        void xrpc_handler::account_snap_delete()
        { 
            //first, delete account on local node           
            top::store::xstore_base* store_base = xsingleton<top::store::xstore_manager>::Instance();
            store_base->account_snap_delete(m_root["account"].asString());
            set_result_ok();
        }

        void xrpc_handler::account_sync()
        { 
            top::store::xsync_account::get_instance().account_sync(m_root["account"].asString());
            set_result_ok();
        }

        bool xrpc_handler::check_input(const xJson::Value& inputJson, xJson::Value& resultJson)
        {
            if("set_property" == m_root["action"].asString())
            {
                std::string property_key = m_root["property_key"].asString();
                   //note: the key_name of any data must start with "@" but NOT allow have "#"or "$"
                if((property_key.find_first_of("@") != 0) || (property_key.find_first_of("#$") != std::string::npos))
                {
                    set_result_fail(xrpc_property_key_not_valid);
                    return false;
                }
            }
            if("account_create" ==  m_root["action"].asString())
            {
                top::store::xstore_base* store_intf = xsingleton<top::store::xstore_manager>::Instance();
                if(store_intf->exist_account(m_root["account"].asString()))
                {
                    set_result_fail(xrpc_server_account_exist);
                    return false;
                }

            }
            return true;
        }

        void xrpc_handler::set_property(bool nohash)
        {
            //top::chain::xtransaction_t send_tx;
            uint256_t last_hash;
            xJson::Value resultJson;
            if(!check_input(m_root, resultJson))
            {
                return;
            }

            if(false == last_digest_set(nohash, m_root["account"].asCString(), last_hash))
            {
                set_result_fail(xrpc_server_account_not_found);
                return;
            }
            xrpc_utility::transaction_set_property(m_root, tx, last_hash);
            tx_digest_signature_set(nohash);

            if(false == tx.tx_hash_and_sign_verify())
            {
                set_result_fail(xrpc_server_hash_signature_check_fail);
                return;
            }

            do_transaction();
            //transaction_retry_timer_start();
            set_result_ok();
        }

        void xrpc_handler::query_property()
        {
            top::store::xstore_base* store_intf = xsingleton<top::store::xstore_manager>::Instance();
            std::string account = m_root["account"].asString();
            chain::xaccount_snap_t account_snap;
			if (store_intf->get_account_snap_info(account, account_snap))
			{
                std::string property_value;
				if(account_snap.get_property(m_root["property_key"].asString(), property_value))
                {
                    m_js_rsp["property_value"] = property_value;
                    set_result_ok();
                }
                else
                {
                    set_result_fail(xrpc_no_property_key);
                }
			}
			else
			{
                set_result_fail(xrpc_server_account_not_found);
			}
        }

        void xrpc_handler::query_all_property()
        {
            top::store::xstore_base* store_intf = xsingleton<top::store::xstore_manager>::Instance();
            std::string account = m_root["account"].asString();
            chain::xaccount_snap_t account_snap;
			if (store_intf->get_account_snap_info(account, account_snap))
			{
                std::unordered_map<std::string, std::string> property_map = std::move(account_snap.get_all_property());
				for(auto iter = property_map.begin(); iter != property_map.end(); ++iter)
                {
                    m_js_rsp[iter->first] = iter->second;
                }
                set_result_ok();
			}
			else
			{
                set_result_fail(xrpc_server_account_not_found);
			}
        }

        void xrpc_handler::trigger_elect()
        {
            m_js_rsp = std::move(elect2::xelect_manager_intf::get_instance()->trigger_elect(m_root));
        }

        bool xrpc_handler::no_hash(std::string account)
        {
            if(account == "T-Um45LZ8HZYUpm5jdWi6bh99mdNjEHkz6w" || account == "T-cEcYfbpbmGvwQbHL349G8khFMcqLmpxig" || account == "T-c1ayVXVmMPajuCTj1ahgWLPyU4m9XUBGk")
            {
                return true;
            }
            return false;
        }

        bool xrpc_handler::transaction_digest_add()
        {
            top::utl::xecprikey_t pri_key_obj;
            uint8_t  test_genesis_account_pri_key[32] = {0x83,0xab,0x81,0xe3,0xcf,0xe1,0x44,0x25,0xe5,0x30,0xea,0xb1,0xd3,0x87,0xc6,0xff,0x80,0x50,0x63,0x73,0xc0,0xa1,0xcf,0x5c,0x63,0xc7,0x55,0x1c,0x68,0x40,0x06,0xed};
            std::string test_genesis_account_address = "T-Um45LZ8HZYUpm5jdWi6bh99mdNjEHkz6w";
            uint8_t  user_a_pri_key[32] = {0x78,0x09,0xab,0xc8,0xb4,0xa4,0x73,0x88,0x55,0xbd,0xd0,0xfa,0xe4,0x24,0x0b,0x6c,0x66,0xd2,0x00,0x17,0xf1,0x93,0x2c,0x30,0x67,0x15,0x39,0xef,0xf2,0xb8,0x4b,0xa7};
            std::string user_a_account_address = "T-cEcYfbpbmGvwQbHL349G8khFMcqLmpxig";
            uint8_t  user_b_pri_key[32] = {0xb2,0xec,0x24,0x76,0x58,0x03,0x40,0xbe,0x98,0x02,0x33,0xaa,0x74,0xf1,0x36,0x7a,0x69,0x4c,0x60,0xc5,0x3b,0x78,0xc3,0x32,0x15,0x73,0xc2,0x2d,0xc7,0x8e,0x7c,0x60};
            std::string user_b_account_address = "T-c1ayVXVmMPajuCTj1ahgWLPyU4m9XUBGk";

            if(tx.m_sender_addr == test_genesis_account_address)
            {
                pri_key_obj = top::utl::xecprikey_t(test_genesis_account_pri_key);
            }
            else if(tx.m_sender_addr == user_a_account_address)
            {
                pri_key_obj = top::utl::xecprikey_t(user_a_pri_key);
            }
            else if(tx.m_sender_addr == user_b_account_address)
            {
                pri_key_obj = top::utl::xecprikey_t(user_b_pri_key);
            }
            else
            {
                //set_result_fail(xrpc_server_unkown_err);
                return false;
            }

            tx.tx_hash_and_sign_calc(pri_key_obj);

            return true;
        }

        int32_t xrpc_handler::do_transaction()
        {
        #if 0
            top::xtop_message_t msg;
            msg.m_content = tx.serialize_no_type();
            msg.m_msg_type = top::enum_xtop_msg_type_proposal_message;
            msg.m_transaction_unique_seqno = get_transaction_unique_id();
        #else
            top::xtop_message_t msg;
            msg.m_msg_type = top::enum_xtop_msg_type_proposal_message;
            //msg.m_content = xserialize::serialize(&tx);
            msg.m_content = tx.serialize_no_type();
            //rpc server add a unique id in transaction msg for consensus leader selection
            msg.m_transaction_unique_seqno = get_transaction_unique_id();
            tx.to_log("rpc:" + get_message_uuid_string(msg));
        #endif
            if(elect::enum_node_type_rpc != xconfig::get_instance().m_node_type)
            {
                xmessage_dispatcher::get_instance().broadcast_ex(msg);
            }
            else
            {
                elect::xshard_info shard_info = std::move(elect::xelect_manager_intf::get_instance()->get_account_shard_info(tx.m_sender_addr));
                xmessage_dispatcher::get_instance().broadcast(msg, enum_xtop_node_type_consensus_node, shard_info);
            }

            return 0;
        }

        int32_t xrpc_handler::do_transaction(const top::chain::xtransaction_t& tx)
        {

            top::xtop_message_t msg;
            msg.m_msg_type = top::enum_xtop_msg_type_proposal_message;
            msg.m_content = tx.serialize_no_type();
            //rpc server add a unique id in transaction msg for consensus leader selection
            msg.m_transaction_unique_seqno = get_transaction_unique_id();
            tx.to_log("rpc:" + get_message_uuid_string(msg));

            if(elect::enum_node_type_rpc != xconfig::get_instance().m_node_type)
            {
                xmessage_dispatcher::get_instance().broadcast_ex(msg);
            }
            else
            {
                elect::xshard_info shard_info = std::move(elect::xelect_manager_intf::get_instance()->get_account_shard_info(tx.m_sender_addr));
                xmessage_dispatcher::get_instance().broadcast(msg, enum_xtop_node_type_consensus_node, shard_info);
            }

            return 0;
        }

        void xrpc_handler::timeout_handle()
        {
            top::store::xstore_base* m_store_mgr = xsingleton<top::store::xstore_manager>::Instance();
            std::string balance;
            m_store_mgr->get_account_balance(tx.m_sender_addr, tx.m_property_key, balance);
            //std::cout << "get_account_balance account: " <<tx.m_sender_addr << " balance " << balance << std::endl;
            m_store_mgr->get_account_balance(tx.m_receiver_addr, tx.m_property_key, balance);
            //std::cout << "get_account_balance account: " <<tx.m_receiver_addr << " balance " << balance << std::endl;

            chain::xaccount_snap_t local_account;
            bool find = m_store_mgr->get_account_snap_info(tx.m_sender_addr, local_account);
            if(!find)
            {
                xwarn("rpcserver: transaction time out to retry");
                tx.to_log("rpc time out to retry");
                do_transaction();
            }
        }

        void xrpc_handler::set_result_ok()
        {
            m_js_rsp["Result"] = 1;
        }

        void xrpc_handler::set_result_fail(int err)
        {
            m_js_rsp["Result"] = 0;
            m_js_rsp["ErrCode"] = err;
            m_js_rsp["Reason"] = xrpc_return_str(err);
        }

        void xrpc_handler::transaction_retry_timer_start()
        {
        #if 0
            auto self (shared_from_this ()); // add object ref count for async operation
            m_timer.expires_from_now(std::chrono::seconds(5));
            m_timer.async_wait([this, self](const asio::error_code& ec){
                if (!ec)
                {
                    timeout_handle();
                }
                else
                {
                    xwarn("rpcserver: timer async_wait error");
                }
            });
        #endif
        }

    }
}
