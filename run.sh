#!/bin/bash

# Usage:
# ./run.sh server  -> kills port 8080 and runs server
# ./run.sh client  -> runs client without killing port

if [ "$1" == "server" ]; then
    echo "Killing any process using port 8080..."
    sudo /usr/bin/fuser -k 8080/tcp
    g++ -std=c++20 -pthread server.cpp -o server
    ./server
elif [ "$1" == "client" ]; then
    g++ -std=c++20 -pthread client.cpp -o client
    ./client
else
    echo "Usage: $0 [server|client]"
fi

