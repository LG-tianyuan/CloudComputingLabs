# KV store

The KV store of version 1 is pretty simple, it is built based on raft protocol, refering to the examples of [Nuraft](https://github.com/eBay/NuRaft). A C++ implementation.

- A kvstore consists of several servers  with no client.
- We can directly input the command on the terminal where you launch the server to run. We can set, get or delete `key-value`  of the kvstore on the terminal of the leader server.  We can also show the list of the cluster and the status of each server by simple command, `list` or `stat`.

## How to Build

> **For Ubuntu**

#### 1. Install `cmake` and `openssl`: ####

```sh
$ sudo apt-get install cmake openssl libssl-dev libz-dev
```

#### 2. Fetch [Asio](https://github.com/chriskohlhoff/asio) library: ####

```sh
$ ./prepare.sh
```
#### 3. Build static library and kvstore: ####

```sh
$ mkdir build
$ cd build
build$ cmake ../
build$ make
```

#### 4. To run kvstore:

Run kvserver server instances in different terminals. 

```shell
build/kvstore$ ./kvstoresystem 1 127.0.0.1:8001
```

```shell
build/kvstore$ ./kvstoresystem 2 127.0.0.1:8002
```

```shell
build/kvstore$ ./kvstoresystem 3 127.0.0.1:8003
```

```shell
build/kvstore$ ./kvstoresystem 4 127.0.0.1:8004
```

Choose a server that will be the initial leader, and add the other three.

```shell
kvstore_server 1>add 2 127.0.0.1:8002
async request is in progress (check with `list` command)
kvstore_server 1>add 3 127.0.0.1:8003
async request is in progress (check with `list` command)
kvstore_server 1>add 4 127.0.0.1:8004
async request is in progress (check with `list` command)
```

Now 4 servers organize a Raft group.

## About the server prompt

Support 7 types of command.

```
Usage:
@ set value: SET key value
	e.g. SET CS06142 "Cloud Computing"
@ get value: GET key
	e.g. GET CS06142
@ delete value: DELETE key1 key2 ...
	e.g. DELETE CS06142
@ add server: add <server id> <address>:<port>
	e.g. add 2 127.0.0.1:8002
@ get current server status: st (or stat)
@ get the list of members: ls (or list)
@ to quit: q (or exit)
```

