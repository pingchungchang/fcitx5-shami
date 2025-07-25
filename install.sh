#!/bin/sh
clear
cd $(dirname $0)
mkdir -p build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Debug # use Debug for easy debugging with gdb
make && sudo make install && fcitx5 -rd # or sudo ninja install
