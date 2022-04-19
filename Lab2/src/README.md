# httpserver

- version 1
  - `update time`：2022/4/13
  - 编译命令：`make`
  - 基本实现`GET`和`POST`的`basic`和`advanced`功能，文件请求的相关文件在`/static/`文件夹目录下，需要数据验证的请求（`GET`的`search`和`POST`）的相关文件在`/data/`文件夹目录下
  - 用`read_line`的方式一行一行地读取`socket`接收缓冲区的数据
  - 用`while`循环实现`pipeline`，但性能下降不少
  - `proxy`功能未实现
  - 性能未优化
  
- version 2
  - `update time`：2022/4/18
  - 编译命令：`make v2`
  - 一次性地读取`socket`接收缓冲区的数据，再以`read_line`的方式分析数据
  - 通过分析流水线报文实现`pipeline`
  - 实现`proxy`功能，但是存在域名和解析的`ip`地址不对应的情况，以及直接访问某些网站会被直接拒绝访问`forbidden`的情况
  - 性能有待进一步提升

- version 3

  - `update time`：2022/4/19

  - 编译命令：`make v3`

  - 在`version 2`的功能基础上进行性能优化

    - 使用epoll+threadpool的并发编程方法
    - 服务器启动后，对于涉及到的文件请求，第一次请求某文件时将该文件读取到内存中，下一次请求同一文件时直接从内存中读取返回，减少IO（仅限于文件相对较小的情况）

    