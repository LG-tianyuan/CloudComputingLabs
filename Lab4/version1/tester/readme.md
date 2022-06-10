# Course system Tester

This is a CLI test program for [HNU Cloud Computing Lab 4](https://github.com/1989chenguo/CloudComputingLabs/tree/master/Lab4).

> Sorry, the advanced version is still not implemented.

## Install

For most student, you don't neet to rebuild this project.

We provide the release versions for main platforms. 

**Check your OS & goto [Release Page](https://github.com/LabCloudComputing/course-system-tester/releases) to download the correct zip.**

> If you cannot find the target zip, or the binary file cannot execute correctly, check [build](#build) introduction.

Unzip it, then you can find 3 folders: `bin/`, `configs/` & `data/`, and 1 binary file: `course-system-tester`.

Move the whole program folder to anywhere you like, but **DON'T CHANGE** the relative path between files.

## Use

**ATTENTION**: Note that, unless otherwise specified, all relative paths of tester are relative to **the directory where you execute `./course-system-tester`**.

### Configure

There are 3 folders & 2 files in `config/`: 
    - folders: `web/`, `store/`, `load-balancer`;
    - files: `tester-config.json` & `log-config.yaml`.

The files in folders are config files you need to run your servers, modify them according to your implementation and requirements.

`tester-config.json` defines commands and test items of the test program.

`log-config.yaml` defines how to output to the console & the log file.

What you need to do first is to modify the values of this keys in `tester-config.json`: 

- `directory` The directroy of your project,
- `build` The command you compile your project,
- `clean` The command you clean your project,
- `web` The arguments for running your web server,
- `stores` The arguments for running your store servers,
- `load-balancer` The arguments for running your load balancer,

`web` is similar to Lab 2 course-system-tester.

For example:

```json
{
    "directory": "/home/user/projects/course-system",
    "build": "make",
    "clean": "make clean",
    "web": {
        "bin": "./bin/web-server",
        "arguments": {
            "ip": "127.0.0.1",
            "port": 8080,
            "config_path": "./configs/web/stores.conf"
        }
    },
    ...
}
```

**ATTENTION**: `bin` is relative to `directory`.

> It's better to use absolute path for the key `directory`. 
> Relative path is OK, but don't use environment variables like `$HOME` or `~`.

> If you want to pass `build` or `clean`, just use a empty string `""`.

> For `web.arguments.ip`, use `"127.0.0.1"` instead of `"localhost"`. Because `ab` don't support it.

Tester will go to `/home/user/projects/course-system` and run the command `./bin/web-server --ip 127.0.0.1 --port 8080 --config_path ./configs/web/stores.conf`.

If you don't understand how the meanings of keys, check [Configure Tester](#configure-tester).

`stores` is a little more complicated.

For example:

```json
{
    ...
    "stores": [
        {
            "bin": "./bin/2pc/store-server",
            "arguments": [
                "./configs/store/cluster-0/coordinator.conf",
                "./configs/store/cluster-0/participant-0.conf",
                ...
            ],
            "vnics": [
                "192.168.22.101/24",
                "192.168.22.102/24",
                ...
            ]
        },
        {
            "bin": "./bin/raft/store-server",
            "arguments": [
                "./configs/store/cluster-1/participant-0.conf",
                "./configs/store/cluster-1/participant-1.conf",
                ...
            ],
            "vnics": [
                "192.168.22.201/24",
                "192.168.22.202/24",
                ...
            ]
        }
    ],
    ...
}
```

There are `vnics` arrays, which represents the *virtual network interface controller* (vnic) used by the store servers.

Before running your store server, the tester sets up a vnic, and your server is able to bind a ip address to it. 

The tester can disable the network of your server by removing the vnic instead of stopping your server.

> Controlling vnics need sudo privilege, and that's the reason why we add a arguments `password`.

So if you modify the ip address of your store server, don't forget to modify the `vnics` array too.

> The advanced version is still not implemented. So let's pass the `load-balancer`.

### Check Tools & Files

We provide a special binary executable file `ab-go` for the tester to use, it's in `bin/`.

> If you can't run it, use `chmod`.

Files in the directory `./data/requests` are the postdata which should be sent during the performance test.

They are specified in `./config/tester-config.json`.

### Run

There are some subcommands, **your most commonly used subcommand should be `run`.**

It has two arguments, `--password` & `--mode`:
  - use `--password <Password>` to input your sudo password,
  - use `--mode basic` or `--mode advanced` to select tester work mode.

For example:

```bash
user@linux:~/course-system-tester$ ./course-system-tester run --password **** --mode basic
```

Use `run` subcommand, tester will check `bin/ab-go` tools , rebuild your projects, run your web servers & store servers, and send requests to test.

You can also use other subcommands, like `build`, to help you to develop the system.

> The `build` part will not print any messages unless it builds failed.

> The output of your server will be printed to the console, but not the log file.

Use `course-system-tester --help` for more help information.

```bash
user@linux:~/course-system-tester$ ./course-system-tester --help
course-tester x.x.x
A CLI test program for HNU Cloud Computing Lab 4, built with Rust.

USAGE:
    course-tester <SUBCOMMAND>

OPTIONS:
    -h, --help       Print help information
    -V, --version    Print version information

SUBCOMMANDS:
    build    Build the course system
    check    Check if tools have been installed and if get sudo privilege
    clean    Clean the project directory
    help     Print this message or the help of the given subcommand(s)
    run      Test the http server including rebuilding and starting
```

### Check Results

The output will be shown in console & be stored in the log file `./logs/tester.log`.

It will show the log level & messages.

You just need to pay attention to info and error levels.

For example:

```bash
user@linux:~/course-system-tester$ ./course-system-tester run --password **** --mode basic
...
[TRACE] Waiting in 1s for server to stop...
[TRACE] The server is stopped.
[WARN ] Removing up all vnics...
[INFO ] -------TESTER RESULTS------
[INFO ] Perfermance test 1 times.
[INFO ] No.1 choose:
#AB-GO testing tool. 

 Testing app.  
 Results: 
 
        Requests:                100000
        Failed requests:         0
        Duration:                14693.232ms
        Rps:                     6806rps
        Min:                     2.263ms
        Max:                     1064.350ms
        Avg:                     77.148ms
[INFO ] No.1 drop:
#AB-GO testing tool. 

 Testing app.  
 Results: 
 
        Requests:                100000
        Failed requests:         0
        Duration:                13521.312ms
        Rps:                     7396rps
        Min:                     3.673ms
        Max:                     1067.890ms
        Avg:                     77.101ms
[INFO ] -------TESTER RESULTS------
```

> Sorry, the log file will be replaced if you rerun tester.

## Build

You need to install [Rust](https://www.rust-lang.org/) toolchains.

In the `course-system-teseter/`, `cargo build --release` for release version.

The excutable file will be generated in `./target/release/`.

> Rust is hard but interesting. 😘

## More

### Configure Tester

#### tester-config.json

It is a JSON file, so please pay attention to the format when you modify it.

> If you are not familiar with JSON, check [background.md](https://github.com/1989chenguo/CloudComputingLabs/tree/master/Lab2/background.md) of Lab 2. 

It's a bit long, so please read it carefully.

##### root

| key | value type | description |
| --- | --- | --- |
| directroy | String | The directroy of your project |
| build | String | The command you compile your project |
| clean | String | The command you clean your project |
| web | Object | The arguments for running your web server |
| stores | Object | The arguments for running your store server |
| wait_seconds | integer | The time for the tester to wait for your server to start |
| items | Object | Test items |

##### web

| key | value type | description |
| --- | --- | --- |
| bin | String | The command you run your web server |
| arguments.ip | String | The IP your server tend to bind |
| arguments.port | Integer | The port your server tend to bind |
| arguments.config_path | String | The config file your server tend to use |

##### stores

| key | value type | description |
| --- | --- | --- |
| bin | String | The command you run your web server |
| arguments | Array | The config files tend to use for each server |
| vnics | Array | The vnic your server tend to bind |

##### items

| key | value type | description |
| --- | --- | --- |
| basic / advanced | Object | Test items of basic / advanced version |

> You can extend the waiting time appropriately if testing always starts before your server startups completely.

##### basic & advanced

| key | value type | description |
| --- | --- | --- |
| performance | Array | Specific test items for perfermance |

- `perfromance[i]`
  - It will run to choose & drop courses at the same time
  - `path`: The url path that `ab-go` will request.
  - `requests`: The `-n` argument of `ab-go`, number of requests to perform.
  - `concurrency`: The `-c` argument of `ab-go`, number of multiple requests to make at a time.
  - `postdata`: The `-p` argument of `ab-go`, the file contains the postdata.
  - `port`: `ab-go` will occupy a port for a feature, so change the port to any other one instead of errors.

> If you modify the items, don't forget to move the correct file into `data/`.

#### log-config.yaml

If you modify the file incorrectly, the output format may be wrong, please try not to modify.

I use `log4rs` to print log messages & generate log files.

For more infomation, you can check [docs of log4rs](https://docs.rs/log4rs/latest/log4rs/).

### Why use Rust?

Just like it. 😎 Rust YYDS.

If you have any problems about this program, please write a issue.
