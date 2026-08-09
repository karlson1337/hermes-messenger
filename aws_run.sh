#!/usr/bin/env bash

if [ -d "hermes-messenger/bin/server" ]; then
  cd hermes-messenger/bin/server
  echo 8080 > port
  killall hermes_server
  ./hermes_server &
else
  git clone -b depreciated --single-branch https://github.com/karlson1337/hermes-messenger && cd hermes-messenger
  make
  cd bin/server
  echo 8080 > port
  killall hermes_server
  ./hermes_server &
fi
