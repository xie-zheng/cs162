#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "tokenizer.h"

/* Convenience macro to silence compiler warnings about unused function parameters. */
#define unused __attribute__((unused))

/* Whether the shell is connected to an actual terminal or not. */
bool shell_is_interactive;

/* File descriptor for the shell input */
int shell_terminal;

/* Terminal mode settings for the shell */
struct termios shell_tmodes;

/* Process group id for the shell */
pid_t shell_pgid;

int cmd_exit(struct tokens* tokens);
int cmd_help(struct tokens* tokens);
int cmd_current_dir(struct tokens* tokens);
int cmd_change_dir(struct tokens* tokens);

/* Built-in command functions take token array (see parse.h) and return int */
typedef int cmd_fun_t(struct tokens* tokens);

/* Built-in command struct and lookup table */
typedef struct fun_desc {
    cmd_fun_t* fun;
    char* cmd;
    char* doc;
} fun_desc_t;

fun_desc_t cmd_table[] = {
    {cmd_help, "?", "show this help menu"},
    {cmd_exit, "exit", "exit the command shell"},
    {cmd_current_dir, "pwd", "show the current working directory"},
    {cmd_change_dir, "cd", "into directory $arg"},
};

/* Prints a helpful description for the given command */
int cmd_help(unused struct tokens* tokens) {
    for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
        printf("%s - %s\n", cmd_table[i].cmd, cmd_table[i].doc);
    return 1;
}

/* Exits this shell */
int cmd_exit(unused struct tokens* tokens) { exit(0); }

/* Prints the current directory */
int cmd_current_dir(unused struct tokens* tokens) {
    char buf[MAXPATHLEN];
    getwd(buf);
    printf("%s\n", buf);
    return 1;   
}

int cmd_change_dir(struct tokens* tokens) {
    /* assert only one arguments(just assume now) */
    char* path = tokens_get_token(tokens, 1);
    if (chdir(path) == 0) {
        /* success */
    } else {
        printf("'cd %s' [fail - maybe path not exist]\n", path);
    };
    return 1;
}

/* Looks up the built-in command, if it exists. */
int lookup(char cmd[]) {
    for (unsigned int i = 0; i < sizeof(cmd_table) / sizeof(fun_desc_t); i++)
        if (cmd && (strcmp(cmd_table[i].cmd, cmd) == 0))
            return i;
    return -1;
}


/* 没有重定向和管道
 * 正常执行代码 */
int _run(int argc, char** argv, char* env_path) {
    char* path = argv[0];
    if (access(path, F_OK) == 0) {
        // file exists
        int res = execv(path, argv);
        return res;
    } else {
        char* dir = strsep(&env_path, ":");
        while (dir != NULL) {
            // concat program name and dir prefix
            char* new_path = (char*) malloc(strlen(dir) + strlen(path) + 2);
            strcpy(new_path, dir);
            strcat(new_path, "/");
            strcat(new_path, path);

            if (access(new_path, F_OK) == 0) {
                int res = execv(new_path, argv);
                return res;
            }
            dir = strsep(&env_path, ":");
            free(new_path);
        }
        // file doesn't exist
        fprintf(stderr, "no such program %s\n", path);
        return 0;
    }   
}

/* 在fork出的子进程中执行其他程序 */
int run(int argc, char** argv, char* env_path) {
    for (int i = 0; i < argc; i++) {
        // 输入输出重定向
        if (strcmp(argv[i], ">") == 0) {
            int fd = open(argv[i+1], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
            dup2(fd, 1);
            close(fd);
            argv[i] = NULL;
            break;
        } else if (strcmp(argv[i], "<") == 0) {
            int fd = open(argv[i+1], O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
            dup2(fd, 0);
            close(fd);
            argv[i] = NULL;
            break;
        } else if (strcmp(argv[i], "|") == 0) {
            // pipe
            // 应该需要fork新的进程
            int fds[2];
            pipe(fds);
            
            int pid = fork();
            if (pid < 0) {
                fprintf(stderr, "fork child fail");
            } else if (pid == 0) {
                // 子进程的输入从管道中获取
                dup2(fds[0], 0);
                // close(fds[1]);
                argv += (i+1);
                return run(argc-(i+1), argv, env_path);
            } else if (pid > 0) {
                // 父进程的输出重定向到管道中
                dup2(fds[1], 1);
                close(fds[1]);
                argv[i] = NULL; 
            }
            break;
        }
    }

    return _run(argc, argv, env_path);
}

/* Intialization procedures for this shell */
void init_shell() {
    /* Our shell is connected to standard input. */
    shell_terminal = STDIN_FILENO;

    /* Check if we are running interactively */
    shell_is_interactive = isatty(shell_terminal);

    if (shell_is_interactive) {
        /* If the shell is not currently in the foreground, we must pause the shell until it becomes a
         * foreground process. We use SIGTTIN to pause the shell. When the shell gets moved to the
         * foreground, we'll receive a SIGCONT. */
        while (tcgetpgrp(shell_terminal) != (shell_pgid = getpgrp()))
            kill(-shell_pgid, SIGTTIN);

        /* Saves the shell's process id */
        shell_pgid = getpid();

        /* Take control of the terminal */
        tcsetpgrp(shell_terminal, shell_pgid);

        /* Save the current termios to a variable, so it can be restored later. */
        tcgetattr(shell_terminal, &shell_tmodes);
    }
}

int main(unused int argc, unused char* argv[]) {
    init_shell();

    static char line[4096];
    int line_num = 0;

    /* Please only print shell prompts when standard input is not a tty */
    if (shell_is_interactive)
        fprintf(stdout, "%d: ", line_num);

    while (fgets(line, 4096, stdin)) {
        /* Split our line into words. */
        struct tokens* tokens = tokenize(line);

        /* Find which built-in function to run. */
        int fundex = lookup(tokens_get_token(tokens, 0));

        if (fundex >= 0) {
            cmd_table[fundex].fun(tokens);
        } else {
            int status;
            char* env_path = getenv("PATH");

            int argc = tokens_get_length(tokens);

            /* char* argv[argc]时(argv放在栈上时)
             * 程序无法正确执行是因为什么? */
            char **argv = (char**)(malloc(sizeof(char*) * argc));
            for (int i = 0; i < argc; i++) {
                argv[i] = tokens_get_token(tokens, i);
            }

            // fprintf(stdout, "This shell doesn't know how to run programs.\n");
            int pid = fork();
            if (pid < 0) {
                fprintf(stderr, "fork child fail");
            } else if (pid == 0) {
                int res = run(argc, argv, env_path);
                tokens_destroy(tokens);
                free(argv);
                exit(res);
            } else {
                wait(&status);
                if (status != 0 && status != 11) {
                    fprintf(stderr, "%d\n", status);
                }
            }

            free(argv);
        }

        if (shell_is_interactive)
            /* Please only print shell prompts when standard input is not a tty */
            fprintf(stdout, "%d: ", ++line_num);

        /* Clean up memory */
        tokens_destroy(tokens);
    }

    return 0;
}
