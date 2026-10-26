#!/bin/sh
set -e
cp kernel/config.h.sg2002 kernel/config.h
trap 'git checkout -- kernel/config.h' EXIT # restore qemu config on exit and failure
make kernel/kernel.bin
cp kernel/kernel.bin imgtools/
(
  cd imgtools
  mkimage -f xv6.its boot.sd
  mkdir genimage.root
  genimage --rootpath genimage.root --tmppath genimage.tmp \
           --inputpath . --outputpath . --config xv6.cfg
  rm -rf genimage.root genimage.tmp
)
