The benchmark depends on the libevent, LevelDB libraries. Setup depedencies:
```
sudo ./install_deps.sh
```

# Build the microbenchmark

```
mkdir Release
cd Release
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

# How to run

In the Release directory, run:

## Coordinator

```
./coordinator ../libperf.conf 0
```
## Acceptor

```
./acceptor ../libperf.conf 0
```
## Learners (learner / kv_memory / db )

```
./kv_memory ../libperf.conf 0
```

# How to run clients

```
./client ../libperf.conf client-output
```