The benchmark depends on the libevent library. Setup libevent in ubuntu 14.04
```
sudo apt-get install -y libevent-dev
```

# Build the microbenchmark

```
mkdir Release
cd Release
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

# How to run a server

In the Release directory, run:

```
./netpaxos -l
```

# How to run the client

syntax: netpaxos -p -h <server-ip-address> -d <duration>

```
./netpaxos -p -h localhost -d 10000
```