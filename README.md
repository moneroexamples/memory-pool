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
// bits and pieces here and there. One of them is
// tx_blob in cryptonote::tx_info structure
// thus I check if I run my version, or just
// generic one
DEFINE_MEMBER_CHECKER(tx_blob);

// define getter to get tx_blob, i.e., get_tx_blob function
// as string if exists. the getter return empty string if
// tx_blob does not exist
DEFINE_MEMBER_GETTER(tx_blob, string);

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

        print("Fee: {:0.10f} xmr, size {:d} bytes\n",
               XMR_AMOUNT(_tx_info.fee),
              _tx_info.blob_size);

        print("Receive time: {:s}\n",
              xmreg::timestamp_to_str(_tx_info.receive_time));


        // if we have tx blob disply more.
        // this info can also be obtained from json that is
        // normally returned by the RCP call (see below in detailed view)
        if (HAVE_TX_BLOB)
        {
            // get tx_blob if exists
            string tx_blob = get_tx_blob(_tx_info);

            if (tx_blob.empty())
            {
                cerr << "tx_blob is empty. Probably its not a custom deamon." << endl;
                continue;
            }

            cryptonote::transaction tx;

            if (!cryptonote::parse_and_validate_tx_from_blob(
                    tx_blob, tx))
            {
                cerr << "Cant parse tx from blob" << endl;
                continue;
            }

            print("No of inputs {:d} (mixin {:d}) for total {:0.10f} xmr\n",
                  tx.vin.size(),
                  xmreg::get_mixin_no(tx),
                  XMR_AMOUNT(xmreg::sum_money_in_inputs(tx)));

            // get key images
            vector<cryptonote::txin_to_key> key_imgs
                    = xmreg::get_key_images(tx);

            print("Input key images:\n");

            for (const auto& kimg: key_imgs)
            {
                print(" - {:s}, {:0.8f} xmr\n",
                      kimg.k_image,
                      XMR_AMOUNT(kimg.amount));
            }

            print("No of outputs {:d} for total {:0.10f} xmr\n",
                  tx.vout.size(),
                  XMR_AMOUNT(xmreg::sum_money_in_outputs(tx)));

            // get outputs
            vector<pair<cryptonote::txout_to_key, uint64_t>> outputs =
                    xmreg::get_ouputs(tx);

            print("Outputs:\n");

            for (const auto& txout: outputs)
            {
                print(" - {:s}, {:0.10f} xmr\n",
                      txout.first.key,
                      XMR_AMOUNT(txout.second));
            }

        }
        else
        {
            // if dont have tx_block, then just use json of tx
            // to get the details.

            rapidjson::Document json;

            if (json.Parse(_tx_info.tx_json.c_str()).HasParseError())
            {
                cerr << "Failed to parse JSON" << endl;
                continue;
            }

            // get information about inputs
            const rapidjson::Value& vin = json["vin"];


            if (vin.IsArray())
            {
                // get total xmr in inputs
                uint64_t total_xml {0};

                for (rapidjson::SizeType i = 0; i < vin.Size(); ++i)
                {
                    if (vin[i].HasMember("key"))
                    {
                        total_xml +=  vin[i]["key"]["amount"].GetUint64();
                    }
                }

                print("No of inputs {:d} (mixin {:d}) for total {:0.10f} xmr\n",
                      vin.Size(), vin[0]["key"]["key_offsets"].Size(),
                      XMR_AMOUNT(total_xml));

                // print out individual key images
                print("Input key images:\n");

                for (rapidjson::SizeType i = 0; i < vin.Size(); ++i)
                {
                    if (vin[i].HasMember("key"))
                    {
                         print(" - {:s}, {:0.10f} xmr\n",
                              vin[i]["key"]["k_image"].GetString(),
                              XMR_AMOUNT(vin[i]["key"]["amount"].GetUint64()));
                    }
                }
            }

            // get information about outputs
            const rapidjson::Value& vout = json["vout"];

            if (vout.IsArray())
            {

                // get total xmr in outputs
                uint64_t total_xml {0};

                for (rapidjson::SizeType i = 0; i < vout.Size(); ++i)
                {
                        total_xml +=  vout[i]["amount"].GetUint64();
                }

                print("No of outputs {:d} for total {:0.10f} xmr\n",
                      vout.Size(), XMR_AMOUNT(total_xml));

                // print out individual output public keys
                print("Outputs:\n");

                for (rapidjson::SizeType i = 0; i < vout.Size(); ++i)
                {
                    print(" - {:s}, {:0.10f} xmr\n",
                          vout[i]["target"]["key"].GetString(),
                          XMR_AMOUNT(vout[i]["amount"].GetUint64()));
                }
            }

            cout << endl;

        } // else if (HAVE_TX_BLOB)

        if (*detailed_opt)
        {
            cout<< "kept_by_block: " << (_tx_info.kept_by_block ? 'T' : 'F') << std::endl
                << "max_used_block_height: " << _tx_info.max_used_block_height << std::endl
                << "max_used_block_id: " << _tx_info.max_used_block_id_hash << std::endl
                << "last_failed_height: " << _tx_info.last_failed_height << std::endl
                << "last_failed_id: " << _tx_info.last_failed_id_hash << endl
                << "json: " << _tx_info.tx_json
                << std::endl;

            // key images are also returned by RPC call
            // so just for the sake of the example, we print them
            // as well
            if (!res.spent_key_images.empty())
            {
                size_t key_imgs_no = res.spent_key_images.size();

                for (size_t ki = 0; ki < key_imgs_no; ++ki)
                {
                    cryptonote::spent_key_image_info kinfo
                            = res.spent_key_images.at(ki);

                    cout << "key image value: " << kinfo.id_hash << endl;
                    for (const string& tx_hash: kinfo.txs_hashes)
                    {
                        print(" - tx hash: {:s}\n", tx_hash);
                    }
                }
            }
        } // if (*detailed_opt)

        cout << endl;

    }

    cout << "\nEnd of program." << endl;

    return 0;
}
```
## Program options

```bash
./mpool -h
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
Fee: 0.0100000000 xmr, size 459 bytes
Receive time: 2016-04-08 10:07:03
No of inputs 1 (mixin 2) for total 300.0000000000 xmr
Input key images:
 - a2e1f563129bddc921ae9058d115db18e4af684a4c20ae836b6cdc0abc736a03, 300.0000000000 xmr
No of outputs 6 for total 299.9900000000 xmr
Outputs:
 - 6d5101198a9a7a04587d3ae0741baa09328ddf66fbf4ffb2b95311795c0e75e5, 0.0900000000 xmr
 - 84d90734310ca002423397724970186d258474dc901a0324bb01e34329c2faef, 0.1000000000 xmr
 - 64c7b60af446ddfa5712d52b912722e19f3111d38b4afb3d369c174a1bd4a9b1, 0.8000000000 xmr
 - 483549d6b184688c5fc2a96bf6298d956ee053e257d4a2f27ce58bbbaa37da1e, 9.0000000000 xmr
 - fb21d786e2bea604245385d027286f6e19c228e43e1c9fde5771a3b427339cae, 90.0000000000 xmr
 - 7c79c51259a57a23bd2994f5f0fe6d32501f0b3f9cef478e7fa78926b5b27b11, 200.0000000000 xmr
```

## Example  output 2
Default output with more than one transactions in mempool.
```bash
./mpool
No of transactions in memory pool: 2

Tx hash: 3d32acc7359c1fd8ac585f927582d0e4f5a0ce97733da3f2cfcabe11b28912e1
Fee: 0.0100000000 xmr, size 459 bytes
Receive time: 2016-04-08 10:07:03
No of inputs 1 (mixin 2) for total 300.0000000000 xmr
Input key images:
 - a2e1f563129bddc921ae9058d115db18e4af684a4c20ae836b6cdc0abc736a03, 300.0000000000 xmr
No of outputs 6 for total 299.9900000000 xmr
Outputs:
 - 6d5101198a9a7a04587d3ae0741baa09328ddf66fbf4ffb2b95311795c0e75e5, 0.0900000000 xmr
 - 84d90734310ca002423397724970186d258474dc901a0324bb01e34329c2faef, 0.1000000000 xmr
 - 64c7b60af446ddfa5712d52b912722e19f3111d38b4afb3d369c174a1bd4a9b1, 0.8000000000 xmr
 - 483549d6b184688c5fc2a96bf6298d956ee053e257d4a2f27ce58bbbaa37da1e, 9.0000000000 xmr
 - fb21d786e2bea604245385d027286f6e19c228e43e1c9fde5771a3b427339cae, 90.0000000000 xmr
 - 7c79c51259a57a23bd2994f5f0fe6d32501f0b3f9cef478e7fa78926b5b27b11, 200.0000000000 xmr


Tx hash: bcdd2147b21ca4f77ba9c68ab1b7a84951552182528dc2cdfabafc09e8bc168e
Fee: 0.1000000000 xmr, size 1194 bytes
Receive time: 2016-04-08 14:55:14
No of inputs 1 (mixin 3) for total 100.0000000000 xmr
Input key images:
 - fa97ccd850f0594f23180af914aef05628f75fac275a5122c7d31feb25bbb52a, 100.0000000000 xmr
No of outputs 24 for total 99.9000000000 xmr
Outputs:
 - 6f0f79a82a9666d1fda8d9e28e2b93aff735d57efa6bb2e0f1260bfcaad42ad6, 0.0000000000 xmr
 - cfd2ddca469f223fe45cea16e6d4d5c4e548866157e7e1ccc4ebca0b0d857f40, 0.0000000000 xmr
 - 434cc293c9402c2aa6943521ab02f7657ae1879f131ec260d155dfd5eb9ea8c1, 0.0000000000 xmr
 - 431f643b1452ad1788abb29b9aa9f052cac5e32841f61530786113a4e52a2807, 0.0000000001 xmr
 - 167f8f7681fc25e8e08c623f1ce0f8852b9335fe94c64c1fa38546fa3bc731cf, 0.0000000004 xmr
 - e420e388ec66d6d7ccdecc7cb3d485e3f6a57e9525df330810a7e8b556944da6, 0.0000000005 xmr
 - dfaeb1a5cf1d846283c31c86b99b6a51b3b958f15b22e0ae311abccaa3ebaa0d, 0.0000000040 xmr
 - 70e448c8243dd73894a069cd9eeed329061aed4a25491d5676e5c71c401ebf55, 0.0000000050 xmr
 - cb46ec49617fa192f6d6f8531b359a27d22a09832fbb353d1417587d0d2d924f, 0.0000000900 xmr
 - 9d9b82993a02489ddb71bb2c722afed77799977e6d93810769567966232fb9fb, 0.0000002000 xmr
 - f44fee23dd911a80fd41e309947ad41273a91bb2906bbd4f40c27264853996ec, 0.0000007000 xmr
 - e798306cd30a7f014b31634b35478ea311acdc9e5014131c99011c288b7e31fd, 0.0000090000 xmr
 - e49641e98a2651982e827793fe11df7c22715c1d8e50ffbc2035c36114f3c1dc, 0.0000300000 xmr
 - b1e18916ec80780ad73d1100c43944f28f839d4126c69783dae2bc2874782c69, 0.0000600000 xmr
 - b3746068c3c2c0eca654edb664eba48d99eb6626cafd2b5b6951d05e06a5d779, 0.0002000000 xmr
 - fca87eddc12111de205b18180cef18bc1385be513683cc021e3ff75f7fd96994, 0.0007000000 xmr
 - 8afd9f65b376fc1990e1ee147b260d66ea5bf15b444fa1bcdd87bbaf6407373c, 0.0010000000 xmr
 - 365ede1f576540774c50979400452f391092ec38375e5ee05e4f15567b4634c9, 0.0080000000 xmr
 - c982dfa44ac2ce4494a56c8890e529f08ab9dbb3496a93004c2c7d9b5e7ae7ad, 0.0400000000 xmr
 - e43b7631d38d58a60c919982146c709173cb5894528e6369b60a2e7e4316070c, 0.0500000000 xmr
 - 01bdfaac0a4e3c41e3fe87b43b7f10ce15f53c634c8d5b8dd848dd79f1555938, 0.2000000000 xmr
 - a0cd363df72b8e0d1d23b1927fef52d24f92277c202bb573be32456403603788, 0.6000000000 xmr
 - d2a165776bc0a5ccfc8088d5fe9818c937c756f8b06c42791f94484357109cb8, 9.0000000000 xmr
 - 1a4271b6ae3ecaf52686f9b7ff47a5197a383a9caed8b75ac0f68e89172f68f6, 90.0000000000 xmr
```

## Example  output 3
Detailed output with more than one transactions in mempool.
```bash
./mpool -d
No of transactions in memory pool: 3

Tx hash: 142aec28ff8020131b738bcc3d60c73baa6a4287269499855b7f92dd15ebf2f1
Fee: 0.1000000000 xmr, size 586 bytes
Receive time: 2016-04-11 08:56:43
No of inputs 1 (mixin 3) for total 80.0000000000 xmr
Input key images:
 - de54af59ca664b8d158b9a8b7c6e4a8483e1875308c3be5a4ccbf8745337db77, 80.0000000000 xmr
No of outputs 7 for total 79.9000000000 xmr
Outputs:
 - aa555ddc8aa70efcfa5108dea7133ea6e113577a8fb6a9a965640dc98d722e00, 0.0100000000 xmr
 - 74ff68e9e76e6f36e73d7777abd5bda7f4b304f0b41d0245ff0968f674557457, 0.0900000000 xmr
 - ad2bcff6d435b2b402e930eac3b3b2cbee8e86ecd13191627296ab7de14b10cf, 0.3000000000 xmr
 - b4563a1598a40c87300c1bb1ba424479907d9134d2ec2256e9345f6c63c47a22, 0.5000000000 xmr
 - 340d16b9349b69a799ea205ab06a088bef926f498e64df27737fbd058dd7844e, 2.0000000000 xmr
 - 09f2998fc8694f8f85b9c76145454b8a53e8869f77ae29cfb46213f287a5984e, 7.0000000000 xmr
 - 7fc6b9edc88397daff9f92688f867814c22299cba8e0573ba5c0e2c5c5a0051a, 70.0000000000 xmr

kept_by_block: F
max_used_block_height: 1022689
max_used_block_id: 81a23ad616d14c8cca6f6b123d4a3563a8e856cdbe47395f1fb9e60fb6e1860a
last_failed_height: 0
last_failed_id: 0000000000000000000000000000000000000000000000000000000000000000
json: {
  "version": 1,
  "unlock_time": 0,
  "vin": [ {
      "key": {
        "amount": 80000000000000,
        "key_offsets": [ 756, 5740, 6562
        ],
        "k_image": "de54af59ca664b8d158b9a8b7c6e4a8483e1875308c3be5a4ccbf8745337db77"
      }
    }
  ],
  "vout": [ {
      "amount": 10000000000,
      "target": {
        "key": "aa555ddc8aa70efcfa5108dea7133ea6e113577a8fb6a9a965640dc98d722e00"
      }
    }, {
      "amount": 90000000000,
      "target": {
        "key": "74ff68e9e76e6f36e73d7777abd5bda7f4b304f0b41d0245ff0968f674557457"
      }
    }, {
      "amount": 300000000000,
      "target": {
        "key": "ad2bcff6d435b2b402e930eac3b3b2cbee8e86ecd13191627296ab7de14b10cf"
      }
    }, {
      "amount": 500000000000,
      "target": {
        "key": "b4563a1598a40c87300c1bb1ba424479907d9134d2ec2256e9345f6c63c47a22"
      }
    }, {
      "amount": 2000000000000,
      "target": {
        "key": "340d16b9349b69a799ea205ab06a088bef926f498e64df27737fbd058dd7844e"
      }
    }, {
      "amount": 7000000000000,
      "target": {
        "key": "09f2998fc8694f8f85b9c76145454b8a53e8869f77ae29cfb46213f287a5984e"
      }
    }, {
      "amount": 70000000000000,
      "target": {
        "key": "7fc6b9edc88397daff9f92688f867814c22299cba8e0573ba5c0e2c5c5a0051a"
      }
    }
  ],
  "extra": [ 2, 33, 0, 60, 226, 87, 179, 17, 229, 172, 248, 73, 153, 47, 90, 103, 81, 136, 232, 167, 15, 102, 253, 160, 72, 179, 248, 147, 130, 89, 161, 57, 142, 170, 139, 1, 12, 168, 253, 204, 59, 34, 221, 252, 28, 248, 117, 198, 186, 212, 32, 33, 180, 53, 104, 213, 81, 34, 209, 50, 152, 139, 25, 114, 159, 164, 231, 243
  ],
  "signatures": [ "9f2bf0d73163b999d3db387df6deb539c86250a91754aada8e0205fdffa2a000d772591a7d3464b139f7ab129249721942d5774befbb8605e38f6a8c7f6bb00fb6ae2e46cea2233a39bdd2e27a06ea27de539aeb879f8f132745d370ca1137063ccfa83d9b1cd633063845367fa93a64fc83dfbfc412d0bca25fca1ec249ae061cf6c1b94ff3c12e7024e09e68e39fe8dee04e384d71b80060302a832ae93d0bc3f5034ecf79fa49629c7df0bed392f5cce7628cca49d34ac9f081f59970be05"]
}
key image value: de54af59ca664b8d158b9a8b7c6e4a8483e1875308c3be5a4ccbf8745337db77
 - tx hash: 142aec28ff8020131b738bcc3d60c73baa6a4287269499855b7f92dd15ebf2f1
key image value: fa97ccd850f0594f23180af914aef05628f75fac275a5122c7d31feb25bbb52a
 - tx hash: bcdd2147b21ca4f77ba9c68ab1b7a84951552182528dc2cdfabafc09e8bc168e
key image value: a2e1f563129bddc921ae9058d115db18e4af684a4c20ae836b6cdc0abc736a03
 - tx hash: 3d32acc7359c1fd8ac585f927582d0e4f5a0ce97733da3f2cfcabe11b28912e1

Tx hash: 3d32acc7359c1fd8ac585f927582d0e4f5a0ce97733da3f2cfcabe11b28912e1
Fee: 0.0100000000 xmr, size 459 bytes
Receive time: 2016-04-08 10:07:03
No of inputs 1 (mixin 2) for total 300.0000000000 xmr
Input key images:
 - a2e1f563129bddc921ae9058d115db18e4af684a4c20ae836b6cdc0abc736a03, 300.0000000000 xmr
No of outputs 6 for total 299.9900000000 xmr
Outputs:
 - 6d5101198a9a7a04587d3ae0741baa09328ddf66fbf4ffb2b95311795c0e75e5, 0.0900000000 xmr
 - 84d90734310ca002423397724970186d258474dc901a0324bb01e34329c2faef, 0.1000000000 xmr
 - 64c7b60af446ddfa5712d52b912722e19f3111d38b4afb3d369c174a1bd4a9b1, 0.8000000000 xmr
 - 483549d6b184688c5fc2a96bf6298d956ee053e257d4a2f27ce58bbbaa37da1e, 9.0000000000 xmr
 - fb21d786e2bea604245385d027286f6e19c228e43e1c9fde5771a3b427339cae, 90.0000000000 xmr
 - 7c79c51259a57a23bd2994f5f0fe6d32501f0b3f9cef478e7fa78926b5b27b11, 200.0000000000 xmr

kept_by_block: T
max_used_block_height: 0
max_used_block_id: 0000000000000000000000000000000000000000000000000000000000000000
last_failed_height: 1020861
last_failed_id: 3713edf56b46d006bebb230764512a7747a46ddc648b0ea5d276011e6a238bed
json: {
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
key image value: de54af59ca664b8d158b9a8b7c6e4a8483e1875308c3be5a4ccbf8745337db77
 - tx hash: 142aec28ff8020131b738bcc3d60c73baa6a4287269499855b7f92dd15ebf2f1
key image value: fa97ccd850f0594f23180af914aef05628f75fac275a5122c7d31feb25bbb52a
 - tx hash: bcdd2147b21ca4f77ba9c68ab1b7a84951552182528dc2cdfabafc09e8bc168e
key image value: a2e1f563129bddc921ae9058d115db18e4af684a4c20ae836b6cdc0abc736a03
 - tx hash: 3d32acc7359c1fd8ac585f927582d0e4f5a0ce97733da3f2cfcabe11b28912e1

Tx hash: bcdd2147b21ca4f77ba9c68ab1b7a84951552182528dc2cdfabafc09e8bc168e
Fee: 0.1000000000 xmr, size 1194 bytes
Receive time: 2016-04-08 14:55:14
No of inputs 1 (mixin 3) for total 100.0000000000 xmr
Input key images:
 - fa97ccd850f0594f23180af914aef05628f75fac275a5122c7d31feb25bbb52a, 100.0000000000 xmr
No of outputs 24 for total 99.9000000000 xmr
Outputs:
 - 6f0f79a82a9666d1fda8d9e28e2b93aff735d57efa6bb2e0f1260bfcaad42ad6, 0.0000000000 xmr
 - cfd2ddca469f223fe45cea16e6d4d5c4e548866157e7e1ccc4ebca0b0d857f40, 0.0000000000 xmr
 - 434cc293c9402c2aa6943521ab02f7657ae1879f131ec260d155dfd5eb9ea8c1, 0.0000000000 xmr
 - 431f643b1452ad1788abb29b9aa9f052cac5e32841f61530786113a4e52a2807, 0.0000000001 xmr
 - 167f8f7681fc25e8e08c623f1ce0f8852b9335fe94c64c1fa38546fa3bc731cf, 0.0000000004 xmr
 - e420e388ec66d6d7ccdecc7cb3d485e3f6a57e9525df330810a7e8b556944da6, 0.0000000005 xmr
 - dfaeb1a5cf1d846283c31c86b99b6a51b3b958f15b22e0ae311abccaa3ebaa0d, 0.0000000040 xmr
 - 70e448c8243dd73894a069cd9eeed329061aed4a25491d5676e5c71c401ebf55, 0.0000000050 xmr
 - cb46ec49617fa192f6d6f8531b359a27d22a09832fbb353d1417587d0d2d924f, 0.0000000900 xmr
 - 9d9b82993a02489ddb71bb2c722afed77799977e6d93810769567966232fb9fb, 0.0000002000 xmr
 - f44fee23dd911a80fd41e309947ad41273a91bb2906bbd4f40c27264853996ec, 0.0000007000 xmr
 - e798306cd30a7f014b31634b35478ea311acdc9e5014131c99011c288b7e31fd, 0.0000090000 xmr
 - e49641e98a2651982e827793fe11df7c22715c1d8e50ffbc2035c36114f3c1dc, 0.0000300000 xmr
 - b1e18916ec80780ad73d1100c43944f28f839d4126c69783dae2bc2874782c69, 0.0000600000 xmr
 - b3746068c3c2c0eca654edb664eba48d99eb6626cafd2b5b6951d05e06a5d779, 0.0002000000 xmr
 - fca87eddc12111de205b18180cef18bc1385be513683cc021e3ff75f7fd96994, 0.0007000000 xmr
 - 8afd9f65b376fc1990e1ee147b260d66ea5bf15b444fa1bcdd87bbaf6407373c, 0.0010000000 xmr
 - 365ede1f576540774c50979400452f391092ec38375e5ee05e4f15567b4634c9, 0.0080000000 xmr
 - c982dfa44ac2ce4494a56c8890e529f08ab9dbb3496a93004c2c7d9b5e7ae7ad, 0.0400000000 xmr
 - e43b7631d38d58a60c919982146c709173cb5894528e6369b60a2e7e4316070c, 0.0500000000 xmr
 - 01bdfaac0a4e3c41e3fe87b43b7f10ce15f53c634c8d5b8dd848dd79f1555938, 0.2000000000 xmr
 - a0cd363df72b8e0d1d23b1927fef52d24f92277c202bb573be32456403603788, 0.6000000000 xmr
 - d2a165776bc0a5ccfc8088d5fe9818c937c756f8b06c42791f94484357109cb8, 9.0000000000 xmr
 - 1a4271b6ae3ecaf52686f9b7ff47a5197a383a9caed8b75ac0f68e89172f68f6, 90.0000000000 xmr

kept_by_block: T
max_used_block_height: 1020945
max_used_block_id: 19a0de1b5f3d1c46b12ae44ebb8194992b89a6df628510cf9cb221aa1b57af6c
last_failed_height: 0
last_failed_id: 0000000000000000000000000000000000000000000000000000000000000000
json: {
  "version": 1,
  "unlock_time": 0,
  "vin": [ {
      "key": {
        "amount": 100000000000000,
        "key_offsets": [ 38, 2380, 30028
        ],
        "k_image": "fa97ccd850f0594f23180af914aef05628f75fac275a5122c7d31feb25bbb52a"
      }
    }
  ],
  "vout": [ {
      "amount": 4,
      "target": {
        "key": "6f0f79a82a9666d1fda8d9e28e2b93aff735d57efa6bb2e0f1260bfcaad42ad6"
      }
    }, {
      "amount": 6,
      "target": {
        "key": "cfd2ddca469f223fe45cea16e6d4d5c4e548866157e7e1ccc4ebca0b0d857f40"
      }
    }, {
      "amount": 20,
      "target": {
        "key": "434cc293c9402c2aa6943521ab02f7657ae1879f131ec260d155dfd5eb9ea8c1"
      }
    }, {
      "amount": 70,
      "target": {
        "key": "431f643b1452ad1788abb29b9aa9f052cac5e32841f61530786113a4e52a2807"
      }
    }, {
      "amount": 400,
      "target": {
        "key": "167f8f7681fc25e8e08c623f1ce0f8852b9335fe94c64c1fa38546fa3bc731cf"
      }
    }, {
      "amount": 500,
      "target": {
        "key": "e420e388ec66d6d7ccdecc7cb3d485e3f6a57e9525df330810a7e8b556944da6"
      }
    }, {
      "amount": 4000,
      "target": {
        "key": "dfaeb1a5cf1d846283c31c86b99b6a51b3b958f15b22e0ae311abccaa3ebaa0d"
      }
    }, {
      "amount": 5000,
      "target": {
        "key": "70e448c8243dd73894a069cd9eeed329061aed4a25491d5676e5c71c401ebf55"
      }
    }, {
      "amount": 90000,
      "target": {
        "key": "cb46ec49617fa192f6d6f8531b359a27d22a09832fbb353d1417587d0d2d924f"
      }
    }, {
      "amount": 200000,
      "target": {
        "key": "9d9b82993a02489ddb71bb2c722afed77799977e6d93810769567966232fb9fb"
      }
    }, {
      "amount": 700000,
      "target": {
        "key": "f44fee23dd911a80fd41e309947ad41273a91bb2906bbd4f40c27264853996ec"
      }
    }, {
      "amount": 9000000,
      "target": {
        "key": "e798306cd30a7f014b31634b35478ea311acdc9e5014131c99011c288b7e31fd"
      }
    }, {
      "amount": 30000000,
      "target": {
        "key": "e49641e98a2651982e827793fe11df7c22715c1d8e50ffbc2035c36114f3c1dc"
      }
    }, {
      "amount": 60000000,
      "target": {
        "key": "b1e18916ec80780ad73d1100c43944f28f839d4126c69783dae2bc2874782c69"
      }
    }, {
      "amount": 200000000,
      "target": {
        "key": "b3746068c3c2c0eca654edb664eba48d99eb6626cafd2b5b6951d05e06a5d779"
      }
    }, {
      "amount": 700000000,
      "target": {
        "key": "fca87eddc12111de205b18180cef18bc1385be513683cc021e3ff75f7fd96994"
      }
    }, {
      "amount": 1000000000,
      "target": {
        "key": "8afd9f65b376fc1990e1ee147b260d66ea5bf15b444fa1bcdd87bbaf6407373c"
      }
    }, {
      "amount": 8000000000,
      "target": {
        "key": "365ede1f576540774c50979400452f391092ec38375e5ee05e4f15567b4634c9"
      }
    }, {
      "amount": 40000000000,
      "target": {
        "key": "c982dfa44ac2ce4494a56c8890e529f08ab9dbb3496a93004c2c7d9b5e7ae7ad"
      }
    }, {
      "amount": 50000000000,
      "target": {
        "key": "e43b7631d38d58a60c919982146c709173cb5894528e6369b60a2e7e4316070c"
      }
    }, {
      "amount": 200000000000,
      "target": {
        "key": "01bdfaac0a4e3c41e3fe87b43b7f10ce15f53c634c8d5b8dd848dd79f1555938"
      }
    }, {
      "amount": 600000000000,
      "target": {
        "key": "a0cd363df72b8e0d1d23b1927fef52d24f92277c202bb573be32456403603788"
      }
    }, {
      "amount": 9000000000000,
      "target": {
        "key": "d2a165776bc0a5ccfc8088d5fe9818c937c756f8b06c42791f94484357109cb8"
      }
    }, {
      "amount": 90000000000000,
      "target": {
        "key": "1a4271b6ae3ecaf52686f9b7ff47a5197a383a9caed8b75ac0f68e89172f68f6"
      }
    }
  ],
  "extra": [ 1, 240, 237, 201, 86, 95, 204, 131, 192, 76, 61, 209, 70, 202, 33, 172, 41, 172, 181, 122, 176, 176, 5, 156, 56, 59, 244, 62, 6, 97, 9, 94, 131, 2, 33, 0, 253, 214, 166, 25, 99, 149, 74, 126, 107, 233, 224, 123, 232, 176, 6, 99, 164, 208, 193, 161, 133, 64, 190, 144, 23, 23, 162, 123, 124, 232, 34, 97
  ],
  "signatures": [ "6d5053ee928b2d2975ed855e2cd8b0745fba53bb571da84845d281354219610b92526fe330b84f58c7b569a0ca1c426b0d579a452fc7a42886d4b79bfaa33d0de74a1906934bcffaa51de21446bc93b81bf0115f5139e09c56bee9ca814212003d784aedf4e36bfd414c6e9afb7f844e294486df1e514d2a3febff7f679a67099c9d972a70130bf611e8e4608ad8601e98c117c7bdc34382750e6a948dddbe0d4c866c1c05df2b2685488ff7f83a0a808d44c4a84e53ba812326218599872206"]
}
key image value: de54af59ca664b8d158b9a8b7c6e4a8483e1875308c3be5a4ccbf8745337db77
 - tx hash: 142aec28ff8020131b738bcc3d60c73baa6a4287269499855b7f92dd15ebf2f1
key image value: fa97ccd850f0594f23180af914aef05628f75fac275a5122c7d31feb25bbb52a
 - tx hash: bcdd2147b21ca4f77ba9c68ab1b7a84951552182528dc2cdfabafc09e8bc168e
key image value: a2e1f563129bddc921ae9058d115db18e4af684a4c20ae836b6cdc0abc736a03
 - tx hash: 3d32acc7359c1fd8ac585f927582d0e4f5a0ce97733da3f2cfcabe11b28912e1
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
