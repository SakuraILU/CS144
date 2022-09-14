# CS144 LAB: Write Your Own TCP/IP/Ethernet Protocol Stack

Lab0 ~ Lab7 have been fully implemented and passed all test cases, including:

1. byte_stream in lab0;
2. stream_reassember in lab1;
3. tcp_receiver in lab2;
4. tcp_sender in lab3;
5. tcp_connection in lab4;
6. network_interface in lab5;
7. router in lab6;
8. experience applications based on your TCP/IP/Ethernet full protocol stack (nothing need to implement, but may find few small bugs not exposed in test cases before) in lab7.

Since the cs144 lab provides us with sufficient infrastructure, we don't need to worry about the reading and writing of the underlying virtual network card tun or tap device, so that we only need to complete some classes offering key functions for our network protocal stack. Although the entire code we need to write is less than 2k lines, it completely involves the key content of HTTP, TCP, IP, and Ethernet (it is a pity that there is no lab for routing table estalishment algorithm). Applications can use our own network protocol stack to communicate with the real outside world (just like netcat and telnet), it is very worthwhile to have a try!!

The lab instruction manuals are very detailed, you can find them in <a href="https://cs144.github.io/">cs144 assignment website </a>.

---

# Below is the origin README of cs144 lab

---

For build prereqs, see [the CS144 VM setup instructions](https://web.stanford.edu/class/cs144/vm_howto).

## Sponge quickstart

To set up your build directory:

    $ mkdir -p <path/to/sponge>/build
    $ cd <path/to/sponge>/build
    $ cmake ..

**Note:** all further commands listed below should be run from the `build` dir.

To build:

    $ make

You can use the `-j` switch to build in parallel, e.g.,

    $ make -j$(nproc)

To test (after building; make sure you've got the [build prereqs](https://web.stanford.edu/class/cs144/vm_howto) installed!)

    $ make check_labN *(replacing N with a checkpoint number)*

The first time you run `make check_lab...`, it will run `sudo` to configure two
[TUN](https://www.kernel.org/doc/Documentation/networking/tuntap.txt) devices for use during
testing.

### build options

You can specify a different compiler when you run cmake:

    $ CC=clang CXX=clang++ cmake ..

You can also specify `CLANG_TIDY=` or `CLANG_FORMAT=` (see "other useful targets", below).

Sponge's build system supports several different build targets. By default, cmake chooses the `Release`
target, which enables the usual optimizations. The `Debug` target enables debugging and reduces the
level of optimization. To choose the `Debug` target:

    $ cmake .. -DCMAKE_BUILD_TYPE=Debug

The following targets are supported:

- `Release` - optimizations
- `Debug` - debug symbols and `-Og`
- `RelASan` - release build with [ASan](https://en.wikipedia.org/wiki/AddressSanitizer) and
  [UBSan](https://developers.redhat.com/blog/2014/10/16/gcc-undefined-behavior-sanitizer-ubsan/)
- `RelTSan` - release build with
  [ThreadSan](https://developer.mozilla.org/en-US/docs/Mozilla/Projects/Thread_Sanitizer)
- `DebugASan` - debug build with ASan and UBSan
- `DebugTSan` - debug build with ThreadSan

Of course, you can combine all of the above, e.g.,

    $ CLANG_TIDY=clang-tidy-6.0 CXX=clang++-6.0 .. -DCMAKE_BUILD_TYPE=Debug

**Note:** if you want to change `CC`, `CXX`, `CLANG_TIDY`, or `CLANG_FORMAT`, you need to remove
`build/CMakeCache.txt` and re-run cmake. (This isn't necessary for `CMAKE_BUILD_TYPE`.)

### other useful targets

To generate documentation (you'll need `doxygen`; output will be in `build/doc/`):

    $ make doc

To format (you'll need `clang-format`):

    $ make format

To see all available targets,

    $ make help
