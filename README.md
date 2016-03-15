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

# How to run a learner (server)

In the Release directory, run:

```
./learner ../libperf.conf
```

# How to run the proposer (client)

```
./proposer ../libperf.conf
```