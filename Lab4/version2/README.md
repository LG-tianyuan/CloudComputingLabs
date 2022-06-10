# Lab4: A Course Selection System

- A implementation of basic version with c++.  It's a personal work.
- The System consists of two kvstore clusters with one httpserver.  The kvstore clusters are build based on Lab3 and the httpserver is built based on Lab2.
- We have stored all the course and student data into the two clusters to keep consistence. When adding or dropping courses, the httpserver will send the command to both and receive from one of them. (Assumption: both the cluster will execute successfully.)
- A low-performance implementation. Should be optimized with concurrency skills.

## Build to Run

### 1 For the kvstore

#### 1.1 Build

> **For Ubuntu**

##### 1.1.1 Install `cmake` and `openssl`: #####

```sh
$ sudo apt-get install cmake openssl libssl-dev libz-dev
```

##### 1.1.2 Fetch [Asio](https://github.com/chriskohlhoff/asio) library: #####

```sh
$ ./prepare.sh
```

##### 1.1.3 Build static library and kvstore: #####

> **Attention 1**
>
> You should create configuration files for each server of each cluster like kvstore/configs/, prepare courses and students data files with the correct data format lisk kvstore/data/, and store it in the project directory of kvstore.

```sh
$ mkdir build
$ cd build
build$ cmake ../
build$ make
```

#### 1.2 Run

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

> **Attention**
>
> When you use the tester to test, you should change the data path(in the kvstore/kvstore/kvsystem.cxx, or you can set up a --data_path arg) to load the kv data and rebuild the kvstore project.

------

> last but not least, manage your ip address assignment correctly! Good luck!