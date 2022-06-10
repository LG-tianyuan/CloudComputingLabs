# Lab4: A Course Selection System

- A implementation of basic version with c++ and golang.  It's a team work.
- The System consists of two kvstore clusters with one httpserver.  The kvstore clusters are build based on Lab3 and the httpserver is built based on Lab2.
- We have stored all the course and student data into the two clusters to keep consistence. When adding or dropping courses, the httpserver will send the command to both and receive from one of them. (Assumption: both the cluster will execute successfully.)
- A low-performance implementation. Should be optimized with concurrency skills.

## Build to Run

### 1 For the kvstore

#### 1.1 Build

```shell
KvRaft$ go build main.go
```

#### 1.2 Run

```shell
#for cluster 1
KvRaft$ ./main --config_path ./src1/server001.conf
KvRaft$ ./main --config_path ./src1/server002.conf
KvRaft$ ./main --config_path ./src1/server003.conf
KvRaft$ ./main --config_path ./src1/server004.conf
#for cluster 2
KvRaft$ ./main --config_path ./src2/server001.conf
KvRaft$ ./main --config_path ./src2/server002.conf
KvRaft$ ./main --config_path ./src2/server003.conf
KvRaft$ ./main --config_path ./src2/server004.conf
```

### 2 For the httpserver

#### 2.1 Build

```shell
httpserver$ make
```

#### 2.2 Run

> **Attention**
>
> You should create a configuration file like httpserver/server.conf  and store it in httpserver/ before you run the httpserver.

```shell
httpserver$ ./httpserver --ip 127.0.0.1 --port 8080 --config_path server.conf
#or
httpserver$ ./httpserver -i 127.0.0.1 -p 8080 -c server.conf
```

### 3 For the client

Just open the web browser and enter the correct url that has been show in the Lab4/README.md. Or using `curl` on the terminal is also ok.

### 4 For the tester

You can also test the system with the tester. For more details, please read the tester/readme.md.