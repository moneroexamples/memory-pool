#include "src/MicroCore.h"
#include "src/CmdLineOptions.h"
#include "src/tools.h"


#include "ext/member_checker.h"

#include "ext/minicsv.h"
#include "ext/format.h"

#include "ext/rapidjson/document.h"

#include <iostream>
#include <string>
#include <vector>

using boost::filesystem::path;
using request = cryptonote::COMMAND_RPC_GET_TRANSACTION_POOL::request;
using response = cryptonote::COMMAND_RPC_GET_TRANSACTION_POOL::response;
using http_simple_client = epee::net_utils::http::http_simple_client;

using namespace fmt;
using namespace std;


// without this it wont work. I'm not sure what it does.
// it has something to do with locking the blockchain and tx pool
// during certain operations to avoid deadlocks.

namespace epee {
    unsigned int g_test_dbg_lock_sleep = 0;
}


// define a checker to test if a structure has "tx_blob"
// member variable. I use modified daemon with few extra
// bits and peaces here and there. One of them is
// tx_blob in cryptonote::tx_info structure
// thus I check if I run my version, or just
// generic one like most ppl will.
DEFINE_MEMBER_CHECKER(tx_blob);

int main(int ac, const char* av[]) {

    // get command line options
    xmreg::CmdLineOptions opts {ac, av};

    auto help_opt = opts.get_option<bool>("help");

    // if help was chosen, display help text and finish
    if (*help_opt)
    {
        return 0;
    }

    auto daemon_address_opt = opts.get_option<string>("address");
    auto testnet_opt        = opts.get_option<bool>("testnet");
    auto detailed_opt       = opts.get_option<bool>("detailed");

    // check if we have tx_blob member in tx_info structure
    bool HAVE_TX_BLOB {HAS_MEMBER(cryptonote::tx_info, tx_blob)};



    // perform RPC call to deamon to get
    // its transaction pull
    boost::mutex m_daemon_rpc_mutex;

    request req;
    response res;

    http_simple_client m_http_client;

    m_daemon_rpc_mutex.lock();

    bool r = epee::net_utils::invoke_http_json_remote_command2(
              *daemon_address_opt + "/get_transaction_pool",
              req, res, m_http_client, 200000);

    m_daemon_rpc_mutex.unlock();

    if (!r)
    {
        cerr << "Error connecting to Monero deamon at "
             << *daemon_address_opt << endl;
        return 1;
    }

    cout << "No of transactions in memory pool: "
         << res.transactions.size() << "\n" << endl;


    // for each transaction in the memory pool
    for (size_t i = 0; i < res.transactions.size(); ++i)
    {

        // get transaction info of the tx in the mempool
        cryptonote::tx_info _tx_info = res.transactions.at(i);


        // display basic info
        print("Tx hash: {:s}\n", _tx_info.id_hash);

        print("Fee: {:0.8f} xmr, size {:d} bytes\n",
              static_cast<double>(_tx_info.fee) / 1e12,
              _tx_info.blob_size);

        print("Receive time: {:s}\n",
              xmreg::timestamp_to_str(_tx_info.receive_time));


        // if we have tx blob disply more.
        // this info can also be obtained from json that is
        // normally returned by the RCP call (see below in detailed view)
        if (HAVE_TX_BLOB)
        {
            cryptonote::transaction tx;

            if (!cryptonote::parse_and_validate_tx_from_blob(
                    _tx_info.tx_blob, tx))
            {
                cerr << "Cant parse tx from blob" << endl;
                continue;
            }

            print("No of outputs {:d} for total {:0.8f} xmr\n",
                  tx.vout.size(),
                  static_cast<double>(xmreg::sum_money_in_outputs(tx)) / 1e12);

            print("No of inputs {:d} (mixin {:d}) for total {:0.8f} xmr\n",
                  tx.vin.size(),
                  xmreg::get_mixin_no(tx),
                  static_cast<double>(xmreg::sum_money_in_inputs(tx)) / 1e12);
        }


        if (*detailed_opt)
        {
            cout<< "kept_by_block: " << (_tx_info.kept_by_block ? 'T' : 'F') << std::endl
                << "max_used_block_height: " << _tx_info.max_used_block_height << std::endl
                << "max_used_block_id: " << _tx_info.max_used_block_id_hash << std::endl
                << "last_failed_height: " << _tx_info.last_failed_height << std::endl
                << "last_failed_id: " << _tx_info.last_failed_id_hash
                << "json: " << _tx_info.tx_json
                << std::endl;
        }

        cout << endl;

    }

    cout << "\nEnd of program." << endl;

    return 0;
}
