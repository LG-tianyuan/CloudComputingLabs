# KV store

The KV store in lab3 is built based on raft protocol, refering to [Nuraft](https://github.com/eBay/NuRaft). A C++ implementation.

- A kvstore consists of several servers connected with one client.
- The current implementation is single thread and supports single client service. When the client's request is not received for more than 60 seconds, the connection is automatically disconnected and the socket is re-entered to accept new client's connection requests.
- The port of each server to accept client request is set by default.

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

> **Attention 1**
>
>  You should create a configuration file like kvstore/server.conf  and store it in kvstore/ before you build the kvstore.

```sh
$ mkdir build
$ cd build
build$ cmake ../
build$ make
```

#### 4. To run kvstore:

##### 4.1 run server

Run kvserver server instances in different terminals. 

> **Attiontion 2**
>
> - The order of running the server, the first server in the configuration file should be the last one to run.
> - The ip and port of each server to run should be consistent with the configuration file (kvstore/server.conf).

```shell
build/kvstore$ ./kvstoresystem 2 127.0.0.1:8002
```

```shell
build/kvstore$ ./kvstoresystem 3 127.0.0.1:8003
```

```shell
build/kvstore$ ./kvstoresystem 4 127.0.0.1:8004
```

```shell
build/kvstore$ ./kvstoresystem 1 127.0.0.1:8001 --config-file server.conf
```

##### 4.2 run client

> **Attention 3**
>
>  The second para in the command line should be consistent with the number of the server you run.

```shell
build/kvstore$ ./client 4 server.conf
```

## About the client prompt

Only support 4 types of command.

```
Usage:
@ set value: SET key value
	e.g. SET CS06142 "Cloud Computing"
@ get value: GET key
	e.g. GET CS06142
@ delete value: DELETE key1 key2 ...
	e.g. DELETE CS06142
@ to quit: exit
```

