#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>

// Simplifed shell

#define MAXARGS 10

// All commands have at least a type. Have looked at the type, the code
// typically casts the *cmd to some specific cmd type.
struct cmd
{
    int type; //  ' ' (exec), | (pipe), '<' or '>' for redirection
};

struct execcmd
{
    int type;            // ' '
    char *argv[MAXARGS]; // arguments to the command to be exec-ed
};

struct redircmd
{
    int type;        // < or >
    struct cmd *cmd; // the command to be run (e.g., an execcmd)
    char *file;      // the input/output file
    int mode;        // the mode to open the file with
    int fd;          // the file descriptor number to use for the file
};

struct pipecmd
{
    int type;          // |
    struct cmd *left;  // left side of pipe
    struct cmd *right; // right side of pipe
};

int fork1(void); // Fork but exits on failure.
struct cmd *parsecmd(char *);

// Execute cmd.  Never returns.
void runcmd(struct cmd *cmd)
{
    int fd;
    int p[2], r;
    struct execcmd *ecmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;
    if (cmd == 0)
        exit(0);

    switch (cmd->type)
    {
    default:
        fprintf(stderr, "unknown runcmd\n");
        exit(-1);

    case ' ':
        ecmd = (struct execcmd *)cmd;
        if (ecmd->argv[0] == 0)
            exit(0);

        // Your code here ...
        // fprintf(stderr, " 32\n");

        // printf("command=%s value=%s \n", ecmd->argv[0], ecmd->argv[1]);
        if (execvp(ecmd->argv[0], ecmd->argv) == -1)
        {
            perror("Shell Program");
            _exit(-1);
        }
        break;

    case '>':
        // Your code here ...
        // fprintf(stderr, "> 62\n");

    case '<':
        rcmd = (struct redircmd *)cmd;

        // Your code here ...
        // fprintf(stderr, "< 60\n");
        fd = open(rcmd->file, rcmd->mode); // 打开文件，描述符fd对应文件
        if (fd < 0)
        {
            perror("error opening file or files does not exist\n");
            _exit(-1);
        }
        dup2(fd, rcmd->fd); // 将fd复制到0或1，此时0或1和fd都指向文件
        close(fd);          // 关闭fd，此时只有0或1指向文件

        runcmd(rcmd->cmd);
        break;

    case '|':
        pcmd = (struct pipecmd *)cmd;
        //fprintf(stderr, "pipe\n");
        // Your code here ...

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
                if (close(p[1]) == -1)
                {
                    perror("close");
                    exit(EXIT_FAILURE);
                }
            }
            runcmd(pcmd->left);
        }

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
                if (close(p[0]) == -1)
                {
                    perror("close");
                    exit(EXIT_FAILURE);
                }
            }
            runcmd(pcmd->right);
        }

        // 进程不需要管道通信，关闭管道的两端
        if (close(p[0]) == -1)
        {
            perror("close");
            exit(EXIT_FAILURE);
        }
        if (close(p[1]) == -1)
        {
            perror("close");
            exit(EXIT_FAILURE);
        }

        // 进程等待两个子进程执行完毕
        if (wait(NULL) == -1)
        {
            perror("wait");
            exit(EXIT_FAILURE);
        }
        if (wait(NULL) == -1)
        {
            perror("wait");
            exit(EXIT_FAILURE);
        }
        break;
    }
    exit(0);
}

int getcmd(char *buffer, int nbuf)
{

    if (isatty(fileno(stdin)))
        fprintf(stdout, "myShell > ");
    memset(buffer, 0, nbuf);
    fgets(buffer, nbuf, stdin);
    if (buffer[0] == 0) // EOF
        return -1;
    return 0;
}
int main(void)
{
    static char buf[100];
    int fd, r;
    char *arg[20];

    // Read and run input commands.
    while (getcmd(buf, sizeof(buf)) >= 0)
    {
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

        if (fork1() == 0)
            runcmd(parsecmd(buf));
        wait(&r);
    }
    exit(0);
}

int parse(char *buf, char **arg)
{
    int count = 0;
    char *separator = " \n\t";

    arg[count] = strtok(buf, separator);

    while ((arg[count] != NULL) && (count + 1 < MAXARGS))
    {
        arg[++count] = strtok((char *)0, separator);
    }
    return count;
}

int fork1(void)
{
    int pid;

    pid = fork();
    if (pid == -1)
        perror("fork");
    return pid;
}

struct cmd *
execcmd(void)
{
    struct execcmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = ' ';
    return (struct cmd *)cmd;
}

struct cmd *
redircmd(struct cmd *subcmd, char *file, int type)
{
    struct redircmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = type;
    cmd->cmd = subcmd;
    cmd->file = file;
    cmd->mode = (type == '<') ? O_RDONLY : O_WRONLY | O_CREAT | O_TRUNC;
    cmd->fd = (type == '<') ? 0 : 1;
    return (struct cmd *)cmd;
}

struct cmd *
pipecmd(struct cmd *left, struct cmd *right)
{
    struct pipecmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = '|';
    cmd->left = left;
    cmd->right = right;
    return (struct cmd *)cmd;
}

// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>";

int gettoken(char **ps, char *es, char **q, char **eq)
{
    char *s;
    int ret;

    s = *ps;
    while (s < es && strchr(whitespace, *s))
        s++;
    if (q)
        *q = s;
    ret = *s;
    switch (*s)
    {
    case 0:
        break;
    case '|':
    case '<':
        s++;
        break;
    case '>':
        s++;
        break;
    default:
        ret = 'a';
        while (s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
            s++;
        break;
    }
    if (eq)
        *eq = s;

    while (s < es && strchr(whitespace, *s))
        s++;
    *ps = s;
    return ret;
}

int peek(char **ps, char *es, char *toks)
{
    char *s;

    s = *ps;
    while (s < es && strchr(whitespace, *s))
        s++;
    *ps = s;
    return *s && strchr(toks, *s);
}

struct cmd *parseline(char **, char *);
struct cmd *parsepipe(char **, char *);
struct cmd *parseexec(char **, char *);

// make a copy of the characters in the input buffer, starting from s through es.
// null-terminate the copy to make it a string.
char
    *
    mkcopy(char *s, char *es)
{
    int n = es - s;
    char *c = malloc(n + 1);
    assert(c);
    strncpy(c, s, n);
    c[n] = 0;
    return c;
}

struct cmd *
parsecmd(char *s)
{
    char *es;
    struct cmd *cmd;

    es = s + strlen(s);
    cmd = parseline(&s, es);
    peek(&s, es, "");
    if (s != es)
    {
        fprintf(stderr, "leftovers: %s\n", s);
        exit(-1);
    }
    return cmd;
}

struct cmd *
parseline(char **ps, char *es)
{
    struct cmd *cmd;
    cmd = parsepipe(ps, es);
    return cmd;
}

struct cmd *
parsepipe(char **ps, char *es)
{
    struct cmd *cmd;

    cmd = parseexec(ps, es);
    if (peek(ps, es, "|"))
    {
        gettoken(ps, es, 0, 0);
        cmd = pipecmd(cmd, parsepipe(ps, es));
    }
    return cmd;
}

struct cmd *
parseredirs(struct cmd *cmd, char **ps, char *es)
{
    int tok;
    char *q, *eq;

    while (peek(ps, es, "<>"))
    {
        tok = gettoken(ps, es, 0, 0);
        if (gettoken(ps, es, &q, &eq) != 'a')
        {
            fprintf(stderr, "missing file for redirection\n");
            exit(-1);
        }
        switch (tok)
        {
        case '<':
            cmd = redircmd(cmd, mkcopy(q, eq), '<');
            break;
        case '>':
            cmd = redircmd(cmd, mkcopy(q, eq), '>');
            break;
        }
    }
    return cmd;
}

struct cmd *
parseexec(char **ps, char *es)
{
    char *q, *eq;
    int tok, argc;
    struct execcmd *cmd;
    struct cmd *ret;

    ret = execcmd();
    cmd = (struct execcmd *)ret;

    argc = 0;
    ret = parseredirs(ret, ps, es);
    while (!peek(ps, es, "|"))
    {
        if ((tok = gettoken(ps, es, &q, &eq)) == 0)
            break;
        if (tok != 'a')
        {
            fprintf(stderr, "syntax error\n");
            exit(-1);
        }
        cmd->argv[argc] = mkcopy(q, eq);
        argc++;
        if (argc >= MAXARGS)
        {
            fprintf(stderr, "too many args\n");
            exit(-1);
        }
        ret = parseredirs(ret, ps, es);
    }
    cmd->argv[argc] = 0;
    return ret;
}