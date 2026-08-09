#!/usr/bin/env bash

echo "Installing dependencies for debian/ubuntu"

echo "Installing Hermes client"
curl -L -O https://github.com/karlson1337/hermes-messenger/releases/download/v1.0.0/hermes_client-linux-x86_64.tar.gz
tar -xvf hermes_client-linux-x86_64.tar.gz
rm -f hermes_client-linux-x86_64.tar.gz

curl -L -O https://raw.githubusercontent.com/karlson1337/hermes-messenger/aws_host_scripts_depreciated/server_pk.key
mkdir -p ~/.config/hermes/
mv server_pk.key ~/.config/hermes/

echo "Enter 'hermes.karlson1337.me as host and 8080 as port'"
./hermes_client
