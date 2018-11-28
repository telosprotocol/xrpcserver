#pragma once
#include <string>

namespace top {
    namespace rpc {
        enum xrpc_return_en {
            xrpc_ok = 0,
            xrpc_server_json_parse_err,
            xrpc_server_action_invalid,
            xrpc_server_account_not_found,
            xrpc_server_hash_signature_check_fail,
            xrpc_server_account_balance_not_enough,
            xrpc_server_unkown_err,

            xrpc_client_json_parse_err,
            xrpc_client_unkown_err,
            xrpc_call_err,
            xrpc_no_pending,
			xrpc_no_property_key,
            xrpc_no_stat_info,
            xrpc_property_key_not_valid,
            xrpc_server_account_exist,
            xrpc_already_give,
            xrpc_last_hash_error,
            xrpc_sender_receiver_the_same,

        };

        #define TO_STR(val) #val

        inline string xrpc_return_str(int code) {
            const char* names[] = {
                TO_STR(xrpc_ok),
                TO_STR(xrpc_server_json_parse_err),
                TO_STR(xrpc_server_action_invalid),
                TO_STR(xrpc_server_account_not_found),
                TO_STR(xrpc_server_hash_signature_check_fail),
                TO_STR(xrpc_server_account_balance_not_enough),
                TO_STR(xrpc_server_unkown_err),

                TO_STR(xrpc_client_json_parse_err),
                TO_STR(xrpc_client_unkown_err),
                TO_STR(xrpc_call_err),
                TO_STR(xrpc_no_pending),
				TO_STR(xrpc_no_property_key),
                TO_STR(xrpc_no_stat_info),
                TO_STR(xrpc_property_key_not_valid),
                TO_STR(xrpc_server_account_exist),
                TO_STR(xrpc_already_give),
                TO_STR(xrpc_last_hash_error),
                TO_STR(xrpc_sender_receiver_the_same)
                };
            return names[code];
        }
    }
}
