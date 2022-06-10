# KV store

The KV store is built based on raft protocol, refering to [Nuraft](https://github.com/eBay/NuRaft). A C++ implementation.

## How to Build

> **For Ubuntu**

### 1. Build

> **For Ubuntu**

#### 1.1 Install `cmake` and `openssl`: ####

```sh
$ sudo apt-get install cmake openssl libssl-dev libz-dev
```

#### 1.2 Fetch [Asio](https://github.com/chriskohlhoff/asio) library: ####

```sh
$ ./prepare.sh
```

#### 1.3 Build static library and kvstore: ####

> **Attention 1**
>
> You should create configuration files for each server of each cluster like kvstore/configs/, prepare courses and students data files with the correct data format lisk kvstore/data/, and store it in the project directory of kvstore.

```sh
$ mkdir build
$ cd build
build$ cmake ../
build$ make
```

### 2. Run

> **Attiontion 2**
>
> - The order of running the server, the coordinator server in each cluster should be the last one to run.

```shell
#for cluster 1
build/kvstore$ ./kvstoresystem --config_path ./configs/cluster1/server2.conf
build/kvstore$ ./kvstoresystem --config_path ./configs/cluster1/server3.conf
build/kvstore$ ./kvstoresystem --config_path ./configs/cluster1/server4.conf
build/kvstore$ ./kvstoresystem --config_path ./configs/cluster1/server1.conf
#for cluster 2
build/kvstore$ ./kvstoresystem --config_path ./configs/cluster2/server2.conf
build/kvstore$ ./kvstoresystem --config_path ./configs/cluster2/server3.conf
build/kvstore$ ./kvstoresystem --config_path ./configs/cluster2/server4.conf
build/kvstore$ ./kvstoresystem --config_path ./configs/cluster2/server1.conf
```

