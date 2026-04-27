基于 C 语言实现的 TCP Socket 客户端与服务端通信，客户端发送序列化后的 `helloworld` 消息，服务端接收并打印。

## 运行环境
- 两个 Ubuntu 虚拟机（桥接模式，IP 互通）
- 服务端 IP：`192.168.56.103`
- 客户端 IP：`192.168.56.104`

## 使用说明
1.  启动服务端：
   ```bash
 gcc server.c -o server
./server
  ```

2.  启动客户端（先修改 `client.c` 里的服务端 IP）：
 ```bash
  gcc client.c -o client
 ./client
  ```

3.  服务端会打印出收到的 `helloworld` 消息。

