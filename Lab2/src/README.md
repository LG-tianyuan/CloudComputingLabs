# httpserver

- version 1
  - `update time`：2022/4/13
  - 编译命令：`make`
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