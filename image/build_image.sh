#!/usr/bin/env bash

rm -rf ./image-mount
dd if=/dev/zero of=image.dat bs=512 count=68
mkfs.vfat -I -n "image" -f1 -r16 -R1 -s1 -v image.dat
sudo mkdir image-mount
sudo mount image.dat ./image-mount
sudo cp in/* ./image-mount
sudo umount ./image-mount
