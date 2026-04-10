#!/bin/bash
sudo apt-get install make libelf-dev libssl-dev bison flex libncurses5-dev zstd
sudo make menuconfig
sudo vim .config
sudo make menuconfig
sudo make
sudo make modules_install
make bzImage
sudo make install
