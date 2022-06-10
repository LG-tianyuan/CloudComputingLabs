# Distributed KV Store

The KV store in lab3 is built based on raft protocol. This is a Golang implementation.

- A kvstore consists of several servers connected with one client.

## Build and run

### 1. For the servers

#### 1.1 Build

```shell
Raft$ go build main.go
```

#### 1.2 Run

Run kvstore server in different terminals. 

> **Attention**
>
> You should create a configuration file like config/server001.conf  for each server before you run the server.

```shell
Raft$ ./main --config_path ./config/server001.conf
Raft$ ./main --config_path ./config/server002.conf
Raft$ ./main --config_path ./config/server003.conf
Raft$ ./main --config_path ./config/server004.conf
```

### 2. For the client

#### 2.1 Build

```shell
Client$ go build client.go
```

#### 2.2 Run

```shell
Client$ ./client --config_path ./server.conf
```

