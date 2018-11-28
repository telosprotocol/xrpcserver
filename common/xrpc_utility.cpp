#include <iostream>
#include <sys/time.h>
#include "xstring.h"
#include "xrpc_utility.hpp"

namespace top {
    namespace rpc {
        void xrpc_utility::transaction_make_account_create(const string & account, const string & amount, uint64_t timestamp, xtransaction_t &tx)
        {
            tx.m_transaction_type = top::chain::xtransbase_t::enum_xtransaction_type_create_account;
            tx.m_sender_addr = account;
            tx.m_receiver_addr = account;
            tx.m_property_key = "$$";
			if (!amount.empty())
			{
				tx.m_transaction_params = amount;
			}
			else
			{
            tx.m_transaction_params = "0";
			}
            
            tx.m_timestamp = timestamp;
            memset(tx.m_last_msg_digest.data(), 0, tx.m_last_msg_digest.size());
            tx.m_random_nounce = 0;
            tx.m_work_proof = 0;
            tx.m_tx_digest = tx.sha256();
        }

        void xrpc_utility::transaction_make_transfer_out(const string & src, const string & dst, const string & property_key,
                                            const string & amount, const uint64_t timestamp, const uint256_t & last_hash, xtransaction_t &tx)
        {
            tx.m_transaction_type = top::chain::xtransbase_t::enum_xtransaction_type_transfer_asset_out;
            tx.m_sender_addr = src;
            tx.m_receiver_addr = dst;
            tx.m_property_key = property_key;
            tx.m_transaction_params = amount;
            tx.m_timestamp = timestamp;
            memcpy(tx.m_last_msg_digest.data(), last_hash.data(), tx.m_last_msg_digest.size());
            tx.m_random_nounce = 0;
            tx.m_work_proof = 0;
            tx.m_tx_digest = tx.sha256();
        }

        void xrpc_utility::transaction_make_transfer_in(const string & account, const uint64_t timestamp, const uint256_t & tx_hash, const uint256_t & last_hash, xtransaction_t &tx)
        {
            tx.m_transaction_type = top::chain::xtransbase_t::enum_xtransaction_type_transfer_asset_in;
            tx.m_sender_addr = account;
            tx.m_receiver_addr = account;
            tx.m_property_key = "";
            tx.m_transaction_params = string((char*)tx_hash.data(), 32);
            tx.m_timestamp = timestamp;
            memcpy(tx.m_last_msg_digest.data(), last_hash.data(), tx.m_last_msg_digest.size());
            tx.m_random_nounce = 0;
            tx.m_work_proof = 0;
            tx.m_tx_digest = tx.sha256();
        }

        void xrpc_utility::transaction_set_property(const xJson::Value& txJson, top::chain::xtransaction_t& tx, const uint256_t& last_hash)
        {
            tx.m_transaction_type = top::chain::xtransbase_t::enum_xtransaction_type_data_property_operation;
            tx.m_sender_addr = txJson["account"].asString();
            tx.m_receiver_addr = txJson["account"].asString();
            tx.m_property_key = txJson["property_key"].asString();
            tx.m_transaction_params = txJson["property_value"].asString();
            tx.m_timestamp = txJson["timestamp"].asInt64();
            tx.m_last_msg_digest = last_hash;
            tx.m_random_nounce = 0;
            tx.m_work_proof = 0;
            tx.m_tx_digest = tx.sha256();
        }

        void xrpc_utility::transaction_hash_to_signature(const top::utl::xecprikey_t & pri_key_obj, xtransaction_t &tx)
        {
            tx.sign_calc(pri_key_obj);
        }

        string xrpc_utility::hash_to_json_string(const uint256_t & hash)
        {
            return string_utl::base64_encode(hash.data(), hash.size());
        }

        uint256_t xrpc_utility::json_string_to_hash(const string & hash)
        {
            string decode_str = string_utl::base64_decode(hash.c_str());
            return uint256_t((uint8_t*)decode_str.c_str());
        }

        string xrpc_utility::signature_to_json_string(const string & signature)
        {
            return string_utl::base64_encode((uint8_t*)signature.c_str(), signature.length());
        }

        string xrpc_utility::json_string_to_signature(const string & signature)
        {
            return string_utl::base64_decode(signature);
        }

        void xrpc_utility::hex_print(string prefix, const uint8_t* data, int size)
        {
            cout<< prefix;
            for(int i=0;i<size;i++)
            {
                printf("%02x",data[i]);
            }
            std::cout << std::endl;
        }
        void xrpc_utility::hash_print(string name, const uint256_t & hash)
        {
            cout << name << " ";
            uint8_t *pData = hash.data();
            for(int i=0;i<hash.size();i++)
            {
                printf("%02x",pData[i]);
            }
            std::cout << std::endl;
        }

        uint64_t xrpc_utility::get_timestamp()
        {
            struct timeval val;
            gettimeofday(&val, NULL);
            return (uint64_t)val.tv_sec;
        }

        uint64_t xrpc_utility::get_timestamp_ms()
        {
            struct timeval val;
            gettimeofday(&val, NULL);
            return (uint64_t)(val.tv_sec * 1000 + val.tv_usec / 1000);
        }
    }
}
