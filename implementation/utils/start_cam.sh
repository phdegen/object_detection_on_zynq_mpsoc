#! /bin/bash

echo 1000 > /sys/module/usbcore/parameters/usbfs_memory_mb

./camera_inti.elf
