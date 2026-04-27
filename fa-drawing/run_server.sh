#!/bin/bash

cd ..
if [ ! -d "build" ]; then
     mkdir build
     cd build
     cmake -G Ninja -DCMAKE_BUILD_TYPE=Release ..
 else
     cd build
     echo "Build directory already exists, skipping mkdir and config."
fi


SERVER_EXE="fa-drawing/server"
if [ ! -f "$SERVER_EXE" ]; then
    echo "Server executable not found. Starting build..."
    cmake --build build --target server -j $(nproc)
else
    echo "Server executable already exists, skipping build."
fi
cd ..
if [ -f "build/$SERVER_EXE" ]; then
    echo "Starting server..."
    # Execute using the variable
    ./"build/$SERVER_EXE"
else
    echo "Error: build/$SERVER_EXE not found."
    exit 1
fi