#pragma once

#include <string>

#include "xchaincore.h"
#include "xckey.h"
#include "xhash.h"
#include "json/json.h"

using namespace std;
using top::chain::xtransaction_t;

namespace top {
    namespace rpc {
        class xrpc_utility {
        public:
            static void transaction_make_account_create(const string & account, const string & amount, uint64_t timestamp, xtransaction_t &tx);
            static void transaction_make_transfer_out(const string & src, const string & dst, const string & property_key,
                                            const string & amount, const uint64_t timestamp, const uint256_t & last_hash, xtransaction_t &tx);
            static void transaction_make_transfer_in(const string & account, const uint64_t timestamp, const uint256_t & tx_hash, const uint256_t & last_hash, xtransaction_t &tx);
            static void transaction_hash_to_signature(const top::utl::xecprikey_t & pri_key_obj, xtransaction_t &tx);

            static void transaction_set_property(const xJson::Value& txJson, top::chain::xtransaction_t& tx, const uint256_t& last_hash);

            static string hash_to_json_string(const uint256_t & hash);
            static uint256_t json_string_to_hash(const string & hash);
            static string signature_to_json_string(const string & signature);
            static string json_string_to_signature(const string & signature);

            static void hex_print(string prefix, const uint8_t* data, int size);
            static void hash_print(string name, const uint256_t & hash);

            static uint64_t get_timestamp();
            static uint64_t get_timestamp_ms();
        };

    }
}
