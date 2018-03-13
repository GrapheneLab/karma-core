#!/bin/bash

./karma --create-genesis-json my-genesis.json
./karma --data-dir data --genesis-json my-genesis.json 2> new_start_node.txt 1> new_start_node.txt &

karma_pid=$(echo $!)

chain_id=""

while [ -z "$chain_id" ]
do
chain_id=$(grep "Chain ID is" new_start_node.txt | grep -Eo '.{64}$')

done

echo $chain_id > ID.txt

echo "before karma kill"
kill -9 $karma_pid

echo "after karma kill"

sed 's/# rpc-endpoint =/rpc-endpoint = 127.0.0.1:11011/' -i ./data/config.ini
sed 's/# genesis-json =/genesis-json = my-genesis.json/' -i ./data/config.ini
sed 's/enable-stale-production = false/enable-stale-production = true/' -i ./data/config.ini
sed 's/# witness-id = /witness-id = "1.6.1"\nwitness-id = "1.6.2"\nwitness-id = "1.6.3"\nwitness-id = "1.6.4"\nwitness-id = "1.6.5"\nwitness-id = "1.6.6"\nwitness-id = "1.6.7"\nwitness-id = "1.6.8"\nwitness-id = "1.6.9"\nwitness-id = "1.6.10"\nwitness-id = "1.6.11"\n/' -i ./data/config.ini

#create cli start script
start_str="./cli_wallet --wallet-file=my-wallet.json --chain-id "$chain_id" --server-rpc-endpoint=ws://127.0.0.1:11011"
echo $start_str > start_cli.sh
chmod +x start_cli.sh

./karma --data-dir data --genesis-json my-genesis.json

