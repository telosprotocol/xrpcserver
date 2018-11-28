#pragma once

#include <iostream>
#include <string>
#include <unordered_map>
#include "xchaincore.h"
#include "xnet.h"
#include "asio.hpp"
#include "json/json.h"
#include "xaccount_sync.h"
#include "xelect2/xelect_intf.h"

namespace top {
    namespace rpc {
#define TAG_NAME "@name"
        class xrpc_handler : public std::enable_shared_from_this<xrpc_handler>
        {
        public:
            xrpc_handler(const std::string &req);
            xrpc_handler(const xJson::Value &req);
            xrpc_handler(const xrpc_handler &) = delete;
            xrpc_handler &operator=(const xrpc_handler &) = delete;
            void handle_request();
            std::string get_response();
            xJson::Value get_response_ex();
            std::string get_action();
            uint256_t get_tx_digest();
        private:
            void account_balance();
            void account_info();
            void account_create(bool nohash);
            void transfer_out(bool nohash);
            void transfer_in(bool nohash);
            void account_history();
            void account_pending();
            void last_digest();
			void query_tps();
            void set_property(bool nohash);
            void query_property();
            void query_all_property();
            void account_delete();
            void account_snap_delete();
            void account_sync();
            bool check_input(const xJson::Value& inputJson, xJson::Value& resultJson);
            void give();
            void query_online_tx();
            bool no_hash(std::string account);
            void trigger_elect();
        private:
            bool transaction_digest_add();
            bool last_digest_set(bool nohash, const std::string & account, uint256_t & last_hash);
            void tx_digest_signature_set(bool nohash);
            int32_t do_transaction();
            int32_t do_transaction(const top::chain::xtransaction_t& tx);
            void timeout_handle();
            void transaction_retry_timer_start();
            void set_result_ok();
            void set_result_fail(int err);
            void set_property_value(std::unordered_map<std::string, std::string>& user_tag);
            std::string m_request_http;
            xJson::Value m_root;
            xJson::Value m_js_rsp;
            xJson::Value m_js_arrayObj;
            xJson::Value m_js_item;
            top::chain::xtransaction_t tx;
        };
    }
}
