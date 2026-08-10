#!/usr/bin/env bash
set -e

echo "Installing dependencies"
echo "Will ask for password to install packages"

if [ -f /etc/os-release ]; then
    . /etc/os-release
    ID_LIKE="${ID_LIKE:-}"
else
    echo "Cannot detect distro (/etc/os-release missing)"
    exit 1
fi

case "$ID $ID_LIKE" in
    *debian*|*ubuntu*)
        sudo apt update
        sudo apt install -y libsodium-dev libsqlcipher-dev libncurses-dev libreadline-dev tar git make build-essential
        ;;
    *arch*)
        sudo pacman -Sy --needed --noconfirm libsodium sqlcipher ncurses readline tar git make gcc
        ;;
    *fedora*|*rhel*)
        sudo dnf install -y libsodium-devel sqlcipher-devel ncurses-devel readline-devel tar git make gcc gcc-c++
        ;;
    *)
        echo "Unknown distro. Please install dependencies manually (Given in README.md). $ID"
        exit 1
        ;;
esac

echo "Installing Hermes client"

git clone -b deprecated --single-branch https://github.com/karlson1337/hermes-messenger
cd hermes-messenger
make linux
mv bin/client/hermes-client ../
cd ..

curl -L -O https://raw.githubusercontent.com/karlson1337/hermes-messenger/aws_host_scripts_depreciated/server_pk.key
mkdir -p ~/.config/hermes/
mv server_pk.key ~/.config/hermes/
echo "hermes.karlson1337.me 8080" > ~/.config/hermes/host
chmod +x hermes-client
./hermes-client
