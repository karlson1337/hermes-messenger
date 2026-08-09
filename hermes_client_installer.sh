#!/usr/bin/env bash

echo "Installing dependencies for debian/ubuntu"
echo "Will ask for password to install packages"

sudo apt install -y libsodium-dev libsqlcipher-dev libncurses-dev libreadline-dev tar

echo "Installing Hermes client"
curl -L -O https://github.com/karlson1337/hermes-messenger/releases/download/v1.0.0/hermes_client-linux-x86_64.tar.gz
tar -xvf hermes_client-linux-x86_64.tar.gz
rm -f hermes_client-linux-x86_64.tar.gz

curl -L -O https://raw.githubusercontent.com/karlson1337/hermes-messenger/aws_host_scripts_depreciated/server_pk.key
mkdir -p ~/.config/hermes/
mv server_pk.key ~/.config/hermes/
echo "hermes.karlson1337.me 8080" > ~/.config/hermes/host

./hermes_client
