#!/bin/bash

if [ "$EUID" -ne 0 ]
  then echo "Please run as root"
  exit
fi

if [ -f /etc/debian_version ]; then
    OS="Debian"
    VER=$(cat /etc/debian_version)
    apt-get install -y libsnappy1 libsnappy-dev libevent-dev
elif [ -f /etc/redhat-release ]; then
    OS="Red Hat"
    VER=$(cat /etc/redhat-release)
    yum install -y snappy snappy-devel libevent libevent-devel
fi

cd /tmp && wget https://github.com/google/leveldb/archive/v1.18.tar.gz
mkdir leveldb && tar -xf v1.18.tar.gz -C leveldb --strip-components 1
cd leveldb
make
cp --preserve=links libleveldb.* /usr/local/lib
cp -r include/leveldb /usr/local/include/
ldconfig