# Show transactions in memory pool

In this example, it is shown how to query Monero daemon using RPC call
to get transactions in memory pool, and to display their details.

## Prerequisites

The code was written and tested on Ubuntu 16.04 Beta x86_64.

Instruction for Monero compilation:
 - [Ubuntu 16.04 x86_64](https://github.com/moneroexamples/compile-monero-09-on-ubuntu-16-04/)

The Monero C++ development environment was set as shown in the above link.

Obviously, running and synchronized Monero deamon/node is also required.


## C++ code
The main part of the example is `main.cpp`.

```c++
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
```
## Program options

```
./showmixins -h
mpool, show information about transactions in memory pool:
  -h [ --help ] [=arg(=1)] (=0)         produce help message
  -a [ --address ] arg (=http:://127.0.0.1:18081)
                                        monero daemon address
  -d [ --detailed ] [=arg(=1)] (=0)     show more detailed info about
                                        transatcions
  --testnet [=arg(=1)] (=0)             is the address from testnet network
```

## Example  output 1
Default output with just one transaction in the mempool.
```bash
./mpool
No of transactions in memory pool: 1

Tx hash: 3d32acc7359c1fd8ac585f927582d0e4f5a0ce97733da3f2cfcabe11b28912e1
Fee: 0.01000000 xmr, size 459 bytes
Receive time: 2016-04-09 16:06:00
No of outputs 6 for total 299.99000000 xmr
No of inputs 1 (mixin 2) for total 300.00000000 xmr
```

## Example  output 2
Default output with more than one transactions in mempool.
```bash
./mpool
No of transactions in memory pool: 2

Tx hash: 94fc3a32ab2ba0508ac8441702a7281ad3b8a661955201791e9aa486346509f2
Fee: 0.10000000 xmr, size 510 bytes
Receive time: 2016-04-09 17:29:31
No of outputs 5 for total 19.90000000 xmr
No of inputs 1 (mixin 3) for total 20.00000000 xmr

Tx hash: 3d32acc7359c1fd8ac585f927582d0e4f5a0ce97733da3f2cfcabe11b28912e1
Fee: 0.01000000 xmr, size 459 bytes
Receive time: 2016-04-09 16:06:00
No of outputs 6 for total 299.99000000 xmr
No of inputs 1 (mixin 2) for total 300.00000000 xmr
```

## Example  output 3

```bash
./mpool -d
No of transactions in memory pool: 2

Tx hash: 94fc3a32ab2ba0508ac8441702a7281ad3b8a661955201791e9aa486346509f2
Fee: 0.10000000 xmr, size 510 bytes
Receive time: 2016-04-09 17:29:31
No of outputs 5 for total 19.90000000 xmr
No of inputs 1 (mixin 3) for total 20.00000000 xmr
kept_by_block: F
max_used_block_height: 1021807
max_used_block_id: 9930f3e419cf493856a3d1c2f683294eef35ddc65455a2ecc57ac3be12930d27
last_failed_height: 0
last_failed_id: 0000000000000000000000000000000000000000000000000000000000000000json: {
  "version": 1,
  "unlock_time": 0,
  "vin": [ {
      "key": {
        "amount": 20000000000000,
        "key_offsets": [ 12566, 631, 39314
        ],
        "k_image": "1bf250c3c4d8dd156178e62c80cb21d3b34afe3adebb52c7dc36cd66707950bf"
      }
    }
  ],
  "vout": [ {
      "amount": 300000000000,
      "target": {
        "key": "b71eefc4c1aa5e42bff8fb7482bf4c8995d9ec4b934428eda0f7d97d967cbfd4"
      }
    }, {
      "amount": 600000000000,
      "target": {
        "key": "930b2a1436215101a0493d13bd67c535923914a56c03cdfe92e5d30e817fc010"
      }
    }, {
      "amount": 4000000000000,
      "target": {
        "key": "9a6732360940761b978bca864ac5a9ddced19124caf964b2fcb23c590d706baf"
      }
    }, {
      "amount": 5000000000000,
      "target": {
        "key": "a0868a73e93ba8a4a65d7b3ee4e31f434bc7a3324b5add362fc3958d2fc03a36"
      }
    }, {
      "amount": 10000000000000,
      "target": {
        "key": "a052ca6a3c26f244e837a2b0d7e27b9a978c4cf2c163149830d5a238ab838ce6"
      }
    }
  ],
  "extra": [ 2, 33, 0, 240, 239, 197, 10, 103, 225, 196, 1, 107, 233, 156, 0, 226, 5, 62, 63, 169, 3, 36, 34, 1, 42, 37, 222, 67, 238, 67, 71, 186, 94, 104, 220, 1, 251, 129, 50, 222, 32, 239, 59, 158, 106, 205, 168, 163, 183, 183, 181, 154, 148, 235, 117, 183, 8, 182, 208, 237, 163, 221, 88, 25, 227, 236, 236, 101
  ],
  "signatures": [ "e569575bdd583a188b793114dffa5e2c2c7bc1cafb28ac0ca2e473f4f826d9090639e8193af39cc8e556707f12e7e3239d2001aa43f6cfaf9bb210033e765e0369e2f643cf153265b4256614e06e0b9d882fb9a3a101b0f639594dc92db30c05d395050ce40fad3e0c758193897e3dcd1c9d34e8fad25c02edc6e3b69d7d180787b3f46f2a2ba32a0be93e9f61359b484e449ff2be299e44400099e05e20730ead5a709e748496ea7c3bbb16e069ebaec791b9c9daef77d6a98891fb2968e70a"]
}

Tx hash: 3d32acc7359c1fd8ac585f927582d0e4f5a0ce97733da3f2cfcabe11b28912e1
Fee: 0.01000000 xmr, size 459 bytes
Receive time: 2016-04-09 16:06:00
No of outputs 6 for total 299.99000000 xmr
No of inputs 1 (mixin 2) for total 300.00000000 xmr
kept_by_block: T
max_used_block_height: 0
max_used_block_id: 0000000000000000000000000000000000000000000000000000000000000000
last_failed_height: 0
last_failed_id: 0000000000000000000000000000000000000000000000000000000000000000json: {
  "version": 1,
  "unlock_time": 0,
  "vin": [ {
      "key": {
        "amount": 300000000000000,
        "key_offsets": [ 5412, 2551
        ],
        "k_image": "a2e1f563129bddc921ae9058d115db18e4af684a4c20ae836b6cdc0abc736a03"
      }
    }
  ],
  "vout": [ {
      "amount": 90000000000,
      "target": {
        "key": "6d5101198a9a7a04587d3ae0741baa09328ddf66fbf4ffb2b95311795c0e75e5"
      }
    }, {
      "amount": 100000000000,
      "target": {
        "key": "84d90734310ca002423397724970186d258474dc901a0324bb01e34329c2faef"
      }
    }, {
      "amount": 800000000000,
      "target": {
        "key": "64c7b60af446ddfa5712d52b912722e19f3111d38b4afb3d369c174a1bd4a9b1"
      }
    }, {
      "amount": 9000000000000,
      "target": {
        "key": "483549d6b184688c5fc2a96bf6298d956ee053e257d4a2f27ce58bbbaa37da1e"
      }
    }, {
      "amount": 90000000000000,
      "target": {
        "key": "fb21d786e2bea604245385d027286f6e19c228e43e1c9fde5771a3b427339cae"
      }
    }, {
      "amount": 200000000000000,
      "target": {
        "key": "7c79c51259a57a23bd2994f5f0fe6d32501f0b3f9cef478e7fa78926b5b27b11"
      }
    }
  ],
  "extra": [ 1, 245, 112, 123, 239, 74, 41, 77, 30, 172, 168, 6, 158, 207, 179, 77, 42, 107, 21, 47, 133, 218, 72, 188, 56, 104, 131, 59, 185, 241, 186, 13, 57, 2, 9, 1, 52, 8, 78, 20, 150, 45, 25, 249
  ],
  "signatures": [ "cfa64d926b17024c906cc5b007a2832a5cd47ad93cd000975ded0722f36be706329bf2364e1ff61f1aa444132a9f5cab028492441c8b5721af2330c6fa211704b9555b53b947023b5c9479c766989da475405d94e215971d9c1e613e119c7d02421b3616fafd370adc694975cc1e2cb548d220e7d271a52839f7a7b86945e10d"]
}
```


## Compile this example

If so then to download and compile this example, the following
steps can be executed:

```bash
# download the source code
git clone https://github.com/moneroexamples/memory-pool.git

# enter the downloaded sourced code folder
cd memory-pool

# create the makefile
cmake .

# compile
make
```

The Monero C++ development environment was set as shown in the following link:
- [Ubuntu 16.04 x86_64](https://github.com/moneroexamples/compile-monero-09-on-ubuntu-16-04/)

## How can you help?

Constructive criticism, code and website edits are always good. They can be made through github.

Some Monero are also welcome:
```
48daf1rG3hE1Txapcsxh6WXNe9MLNKtu7W7tKTivtSoVLHErYzvdcpea2nSTgGkz66RFP4GKVAsTV14v6G3oddBTHfxP6tU
```
