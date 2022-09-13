---
title: CS144 LAB0 实验记录
zhihu-title-image: /home/sakura/OneDrive/Pictures/Genshin/GanQing/493ebee9-45ef-4403-b911-859c41f727dc.png
zhihu-tags: CS144, 计算机网络, 实验
zhihu-url: https://zhuanlan.zhihu.com/p/558611672
---

## Lab 的个人实现的代码的 Github 仓库（目前更新至 LAB5, 通过了所有测试用例，代码仅供交流）

My CS144_lab Github Repository：https://github.com/SakuraILU/CS144_lab

随缘写实验记录，预计在 Lab4 完成后会完整写一下 TCP 的部分。
目前思路写在注释里，之后写了实验记录后会适当删除一些 comments

## 实验资料

主要资料如下

1. Code: https://github.com/cs144/sponge
2. CS144 Web: https://cs144.github.io/assignments
3. CS144 Lab 0 Guide: https://cs144.github.io/assignments/lab0.pdf
4. CS144 API Man Page(TCPSocket): https://cs144.github.io/doc/lab0/class_t_c_p_socket.html
5. Cpp reference Man Page: https://en.cppreference.com/w/cpp

## 热身 Telnet

### What Is Telnet

Telnet 这个命令行工具可以用于各类网络通讯，指导手册让我们玩玩 http 和 stmp，因为后文实验要写一个 webget 程序，所以主要就玩了玩 telnet 进行 http 通讯的功能。
手册上介绍了最基本的用法，可以通过如下命令与 hostname:service 进程建立 TCP 连接。

```bash
telnent hostname service
```

hostname 是服务器网址域名，service 是服务类型，例如访问百度的网站(http)服务话就是 telnent www.baidu.com http

### Connect a Website

手册上让我们 telnet 课程网站的 http 服务 telnent cs144.keithw.org http，得到

```bash
Trying 104.196.238.229...
Connected to cs144.keithw.org.
Escape character is '^]'.
```

接下来就可以输入我们的 http 请求报文了。按照手册上的指示，输入

```
GET /hello HTTP/1.1     #请求http服务的/hello资源，http协议是1.1版本
Host: cs144.keithw.org  #请求的服务器域名是cs144.keithw.org，主要让中途可能的代理服务器进行识别，如果有该网站的代理服务器且代理服务器向网站确认到代理存储的资源是最新的，那么会代替网站发送资源，起到加速作用
Connection: close       #让服务器发送完资源后立即关闭TCP连接，无需继续维护
                        #空行，告诉telnet我们输入完毕了，可以发送这些字节流啦
```

然后可以得到输出

```
HTTP/1.1 200 OK
Date: Wed, 31 Aug 2022 15:14:43 GMT
Server: Apache
Last-Modified: Thu, 13 Dec 2018 15:45:29 GMT
ETag: "e-57ce93446cb64"
Accept-Ranges: bytes
Content-Length: 14
Connection: close
Content-Type: text/plain

Hello, CS144!
Connection closed by foreign host.
```

得到了一些 http 回复报文再加文本 Hello, CS144!

再用 chrome 浏览器看看这个 cs144.keithw.org/hello 到底是啥玩意儿。可以看到网址对应的页面确实就只有文本 Hello, CS144!

### telnet 的其他使用格式

从道理上讲 hostname 需要被翻译成 IP 地址，service 需要被翻译成服务进程的端口号
尝试一下 telnet hostname/IP service/PORT 发现均可支持。

http service 对应的端口号是 80, IP 的话有个 nslookup 的命令行工具可以用，例如

```
$ nslookup cs144.keithw.org
Server:         127.0.0.53
Address:        127.0.0.53#53

Non-authoritative answer:
Name:   cs144.keithw.org
Address: 104.196.238.229
```

于是 telnet 104.196.238.229 80 也是 ok 的。

### Conclusion

从 telnet 实验可以看出来，就是 telnet (cs144.keithw.org:http)=(104.196.238.229:80)建立连接后，给这个服务进程发送了一段字符串

```
GET /hello HTTP/1.1     #请求http服务的/hello资源，http协议是1.1版本
Host: cs144.keithw.org  #请求的服务器域名是cs144.keithw.org，主要让中途可能的代理服务器进行识别，如果有该网站的代理服务器且代理服务器向网站确认到代理存储的资源是最新的，那么会代替网站发送资源，起到加速作用
Connection: close       #让服务器发送完资源后立即关闭TCP连接，无需继续维护
                        #空行，告诉telnet我们输入完毕了，可以发送这些字节流啦
```

然后就得到了网站的/hello 资源咯。写成 C++的字符串形式，不就是

```c
std::string REQUEST = "GET /hello HTTP/1.1\nHost: cs144.keithw.org\nConnection: close\n\n"
```

## 实验须知

### 环境配置

实验手册推荐官方虚拟机镜像或者 Ubuntu18.04。不过看着环境需求基本上也没啥特别的地方，就用自己的 kubuntu20.04 做咯，基本没啥障碍。
照着实验手册 3.1 操作即可，中途遇到没的 cmake 的问题，sudo apt-get install cmake 即可。编译的话，lscpu 看看多少核，我的 16 核，就 make -j16 了。

### 代码要求

非常幸运的是这个 Lab 是基于 C++的，之前做 NJU ICS2021 PA Lab 的时候写了好多好多 C，面向过程用得太多了，这次终于改成 C++了，而且实验还特别要求使用"Modern C++"。规定里需要注意的是：

1. 多看 cpp reference;
2. 不要使用 pair 操作，例如 malloc/free、new/delete;
3. 不要用原始指针，必要的话采用基于<a href="Scope-Bound Resource Management">RAII 理念的</a>智能指针，自动释放指针。就是把指针 wrap 成一个 class，构造函数里 malloc，析构函数里 free，因为析构是离开作用域后自动调用的，所以不需要手动释放，这个被叫做 Scope-Bound Resource Management 更好;
4. 多用 const，无论是变量，函数参数还是成员函数;
5. 尽量避免全局变量。
6. ...

## Webget

### 代码框架

main 函数挺好理解的，就调用 get_URL(host,service)并分别把 argv[1]和 argv[2]传给它。
这个函数里有一段注释很重要

```
// You will need to connect to the "http" service on
// the computer whose name is in the "host" string,
// then request the URL path given in the "path" string.

// Then you'll need to print out everything the server sends back,
// (not just one call to read() -- everything) until you reach
// the "eof" (end of file).
```

因此 简而言之在 get_URL(host,service)里我们需要补充代码完成 telnet 的功能：

1. 连接 hostname:service;
2. 发送 GET URL 的请求
3. 接受 hostname:service 发回来的资源并打印出来。

### TCPSocket API

手册特别提醒了我们 TCPSocket，这个正是我们分析出来需要用的，手册给我们了<a href="https://cs144.github.io/doc/lab0/class_t_c_p_socket.html">一个相关链接<a>，里面有 TCPSocket 的 API 接口，而且还有个宝藏例子！里面涉及了 Address 类，TCPSocket 的 socket()、bind()、listen()、accept()、read()、write()、close()等 API。

有之前<a href="https://zhuanlan.zhihu.com/p/558714709">研读 C/C++ Socket 的系统调用</a>的经验，这些 wrapper class 看着简单多了。不过大概看看 TCPSocekt 的代码也是不错的。

分析了一下，是文件描述符 fd 被封装成了 FileDescriptor 类，然后通过 public 继承扩展成了 Socekt，又进一步扩展成了 TCPSocekt。

```
fd->FileDescriptor->Socekt->TCPSocekt
```

仔细想想，socket 实际上就是个 fd，TCP 和 UDP socket 又均有一些公有的 Socket API，FileDescriptor->Socket->TCPSocket 的设计是很合理的，此外将 fd 封装成 class FileDescriptor 只是为了更好得写 Modern C++的面向对象代码。

FileDescriptor class 的实例接受一个文件描述符 fd 初始化，然后提供了对这个 fd 的读写、终止检查和关闭等重要操作。

```c
//! Read up to `limit` bytes
std::string read(const size_t limit = std::numeric_limits<size_t>::max());

//! Write a string, possibly blocking until all is written
size_t write(const char *str, const bool write_all = true);

//! Close the underlying file descriptor
void close();

//! EOF flag state
bool eof() const;
```

可以看到代码里对理论上不会改变成员变量的成员函数 eof()标记了 const，强制禁止函数修改成员函数，通过编译器提前爆出错误以避免潜在 bug。
