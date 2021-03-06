# unix-shell

**实验环境：** centos
**提交文件：** main.c ; a.out ; 实验文档

## 总体设计框架

### 目的

I/O 重定向使得程序可以自由地指定数据的流向，不一定从键盘读取数据或输出结果到屏幕上。

管道使得一条命令的输出可以作为另一条命令的输入，多条命令可以配合完成一项任务。

```C
$ pwd
$ ls > y
$ ls | sort | uniq | wc
$ cat < y | sort | uniq | wc > y1
$ cat y1
$ rm y1
$ rm y
```

我们需要实现 $< > |$的基本功能。

### 实验原理

所有的系统调用都通过文件描述符对文件进行 I/O 操作，每个进程都维护自己的一组文件描述符。

程序的使用三个标准的文件描述符 0、1、2，分别对应着标准输入、标准输出和标准错误。shell 会一直保持这三个描述符是打开的。

程序从标准输入读取数据，输出到标准输出。shell 默认从键盘读取数据，然后运行，输出到屏幕上。

因此我们要进行重定向 I/O 和管道操作，就需要操作文件描述符。

### 代码思路

我们得到输入的指令，首先处理 **cd** 的指令，执行了以后。

fork 一个子进程运行用户输入的命令。子进程首先检查命令中是否包含 **<，>或|** 符号，以确定命令的类型，然后用做相应的处理。

程序将指令分为三类，即直接执行的指令 **execcmd**，包含 **< >** 的重定向指令 **redircmd**，以及 **|** 的管道指令 **pipecmd**。

在运行指令的时候，当我们遇到类型为 **execcmd**的时候，直接执行，如下所示。

```C
execvp(ecmd->argv[0], ecmd->argv)
```

如果是 **redircmd** 和 **pipecmd** 那么需要操作文件描述符，更改输入和输出。

## 实验过程

### 指令的解析——bonus

我们需要提供一定的容错性，也就是说当用户输入的时候，前后多加了空格，这种也需要识别出来。

原来直接处理的 **cd** 指令，是根据字符的位置进行解析，当 cd 命令前面有空格的时候，就错误了，因此我们需要针对 **cd**重新解析一遍。

首先如果指令中存在 cd 字符，去除所有的空格，拿到指令再处理。

加入了新的操作，如果直接 **cd** ，或者 **cd~**，那么就回到 **home** 目录。

```C
if (strstr(buf, "cd"))
        {
            parse(buf, arg); //parse arguments

            if (arg[1] == NULL || strcmp("~", arg[1]) == 0)
            {
                chdir(getenv("HOME")); //cd home if no argument
            }
            else if (arg[1] != NULL)
            {
                if (chdir(arg[1]) != 0) //cd dir
                {
                    fprintf(stderr, "cannot cd %s\n", arg[1]);
                }
            }
            continue;
        }

```

### I/O 重定向 < >

#### 分析

**输入重定向 <**

程序从标准输入读取数据，如果将文件描述符 0 定位到一个文件上，那么此文件就成了标准输入的源。实现上述功能要用到 dup2 函数：

```C
int dup2(int oldfd, int newfd);
```

**输出重定向 >**
输出重定向同理，如果将文件描述符 1 定位到一个文件上，那么此文件就成了标准输出。

我们解析的指令中已经封装了需要定位的文件 **file 和 mode** ，以及文件描述符 **fd**，下一个指令 **cmd** 。指令首先匹配 **<** 。

输入和输出的操作过程除了文件描述符不一样以外，其余的一模一样，因此直接操作封装好的变量。如下所示。

#### 结果

```C
    case '>':
    case '<':
        rcmd = (struct redircmd *)cmd;
        fd = open(rcmd->file, rcmd->mode);// 打开文件，描述符fd对应文件
        if (fd < 0)
        {
            perror("error opening file or files does not exist\n");
            _exit(-1);
        }
        dup2(fd, rcmd->fd);// 将fd复制到0或1，此时0或1和fd都指向文件
        close(fd);// 关闭fd，此时只有0或1指向文件

        runcmd(rcmd->cmd);
```

### 管道

#### 分析

管道（pipe）是进程间通信的重要手段之一。调用 pipe 函数创建一个管道，并将其两端连接到两个文件描述符，其中 p[0]为读数据端的文件描述符，p[1]为写数据端的文件描述符：

```C
int pipe(int p[2])
```

当进程创建一个管道之后，该进程就有了连向管道两端的连接（即为两个文件描述符）。当该进程 fork 一个子进程时，子进程也继承了这两个连向管道的连接，如下面左图所示。父进程和子进程都可以将数据写到管道的写数据端口，并从读数据端口将数据读出。两个进程都可以读写管道，但当一个进程读，另一个进程写时，管道的使用效率是最高的，因此，每个进程最好关闭管道的一端，如下面右图所示。

![管道1](shell/pipe1.jpg)

Shell 要实现管道功能，需将前一条命令的输出作为后一条命令的输入。那么以上面右图为基础，还需将前一进程的标准输出重定向到管道的写数据端，将后一进程的标准输入重定向到管道的读数据端，如下图所示：

![管道2](shell/pipe2.jpg)

我们将运行整条命令的子进程称为进程 A，本文 shell 的实现中，进程 A 并不执行命令，而是再 fork 两个进程，称之为进程 left 和 right，分别执行两条命令。两个进程都从进程 A 继承了管道两端的连接，可通过该管道通信。进程 A 不再需要管道连接，于是关闭两个文件描述符，然后等待进程 left 和 right 执行完毕。

#### 结果

```C
// 创建管道
        if (pipe(p) == -1)
        {
            perror("pipe");
            exit(EXIT_FAILURE);
        }

        //创建left进程
        if (fork1() == 0)
        {
            // 关闭管道的读数据端
            if (close(p[0]) == -1)
            {
                perror("close");
                exit(EXIT_FAILURE);
            }

            if (p[1] != STDOUT_FILENO)
            { // 防御性编程
                // 将标准输出重定向到管道的写数据端
                if (dup2(p[1], STDOUT_FILENO) == -1)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }
            runcmd(pcmd->left);
        }
```

```C
        //创建right进程
        if (fork1() == 0)
        {
            // 关闭管道的写数据端
            if (close(p[1]) == -1)
            {
                perror("close");
                exit(EXIT_FAILURE);
            }

            if (p[0] != STDIN_FILENO)
            { // 防御性编程
                // 将标准输出重定向到管道的读数据端
                if (dup2(p[0], STDIN_FILENO) == -1)
                {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
            }
            runcmd(pcmd->right);
        }
```

## 实验感悟

对于重定向和管道的操作，都是对于文件描述的操作。
unix shell
