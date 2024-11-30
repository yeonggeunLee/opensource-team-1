#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>

#define MAX_ARGS 50

void handle_signal(int signo);
int getargs(char *cmd, char **argv);
void handle_redirection_and_pipes(char **argv, int narg);
void function_echo(char **argv);
void function_ls(char **argv);
void function_pwd(char **argv);
void function_mkdir(char **argv);
void function_rmdir(char **argv);
void function_ln(char **argv);
void function_cp(char **argv);
void function_rm(char **argv);
void function_mv(char **argv);
void function_cat(char **argv);

int main(void) {
    char buf[256];
    char *argv[MAX_ARGS + 1];
    int narg;
    pid_t pid;
    int is_background;

    signal(SIGINT, handle_signal);
    signal(SIGQUIT, handle_signal);

    while (1) {
        // 쉘 프롬프트 출력
        printf("쉘> ");
        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            perror("fgets failed");
            continue;
        }

        // 개행 문자 제거
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
            len--;
        }

        // 빈 명령어 처리
        if (len == 0) {
            continue;
        }

        // exit 명령어 처리
        if (strcmp(buf, "exit") == 0) {
            break;
        }

        // 백그라운드 실행 여부 확인
        is_background = 0;
        if (len > 0 && buf[len - 1] == '&') {
            is_background = 1;
            buf[len - 1] = '\0';
            len--;
        }

        narg = getargs(buf, argv);

        if (strcmp(argv[0], "cd") == 0) {
            if (narg == 1) {
                printf("cd: missing argument\n");
            } else {
                if (chdir(argv[1]) == -1) {
                    perror("cd failed");
                }
            }
            continue;
        }
        
        // 자식 프로세스 생성
        pid = fork();
        if (pid == 0) {
            handle_redirection_and_pipes(argv, narg);

            if (strcmp(argv[0], "ls") == 0) {
                function_ls(argv);                            // ls 명령어 
            } else if (strcmp(argv[0], "pwd") == 0) {
                function_pwd(argv);                           // pwd 명령어 
            } else if (strcmp(argv[0], "echo") == 0) {
                function_echo(argv);                          // echo 명령어 
            } else if (strcmp(argv[0], "mkdir") == 0) {
                function_mkdir(argv);                         // mkdir 명령어 
            } else if (strcmp(argv[0], "rmdir") == 0) {
                function_rmdir(argv);                         // rmdir 명령어 
            } else if (strcmp(argv[0], "ln") == 0) {
                function_ln(argv);                            // ln 명령어 
            } else if (strcmp(argv[0], "cp") == 0) {
                function_cp(argv);                            // cp 명령어 
            } else if (strcmp(argv[0], "rm") == 0) {
                function_rm(argv);                            // rm 명령어 
            } else if (strcmp(argv[0], "mv") == 0) {
                function_mv(argv);                            // mv 명령어 
            } else if (strcmp(argv[0], "cat") == 0) {
                function_cat(argv);                           // cat 명령어 
            } else {
                printf("Unknown command: %s\n", argv[0]);     // 만들지 않은 명령어
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS);
        } else if (pid > 0) { 
            if (!is_background) {
                waitpid(pid, NULL, 0);
            } else {
                printf("Process running in background with PID: %d\n", pid);
            }
        } else {
            perror("fork failed");
        }
    }

    printf("쉘 종료.\n");
    return 0;
}

// SIGINT 및 SIGQUIT 시그널 처리
void handle_signal(int signo) {
    if (signo == SIGINT) {
        printf("\nCaught Ctrl-C (SIGINT) signal. Press Enter to continue.\n");
    } else if (signo == SIGQUIT) {
        printf("\nCaught Ctrl-Z (SIGQUIT) signal. Press Enter to continue.\n");
    }
    fflush(stdin);
}

// 명령어 인자 파싱
int getargs(char *cmd, char **argv) {
    int narg = 0;
    while (*cmd) {
        if (*cmd == ' ' || *cmd == '\t' || *cmd == '\n') *cmd++ = '\0';
        else {
            argv[narg++] = cmd++;
            while (*cmd != '\0' && *cmd != ' ' && *cmd != '\t' && *cmd != '\n')
                cmd++;
        }
    }
    argv[narg] = NULL;
    return narg;
}

// 파일 재지향 및 파이프 처리
void handle_redirection_and_pipes(char **argv, int narg) {
    int i, fd;
    int pipe_fds[2];
    int in_fd = STDIN_FILENO;
    int out_fd = STDOUT_FILENO;
    int pipe_flag = 0;
    
    for (i = 0; i < narg; i++) {
        if (strcmp(argv[i], "|") == 0) {
            argv[i] = NULL;
            pipe_flag = 1;
            break;
        }
    }

    if (pipe_flag) {
        pipe(pipe_fds);
        if (fork() == 0) {
            dup2(pipe_fds[1], STDOUT_FILENO);
            close(pipe_fds[0]);
            execvp(argv[0], argv);
            perror("exec failed");
            exit(EXIT_FAILURE);
        } else {
            dup2(pipe_fds[0], STDIN_FILENO);
            close(pipe_fds[1]);
            execvp(argv[i + 1], &argv[i + 1]);
            perror("exec failed");
            exit(EXIT_FAILURE);
        }
    }

    for (i = 0; i < narg; i++) {
        if (strcmp(argv[i], ">") == 0) {
            argv[i] = NULL;
            fd = open(argv[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("open failed");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        } else if (strcmp(argv[i], "<") == 0) {
            argv[i] = NULL;
            fd = open(argv[i + 1], O_RDONLY);
            if (fd == -1) {
                perror("open failed");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }
    }
}

// echo 명령어 처리
void function_echo(char **argv) {
    for (int i = 1; argv[i] != NULL; i++) {
        printf("%s ", argv[i]);
    }
    printf("\n");
}

// ls 명령어 처리
void function_ls(char **argv) {
    execvp("/bin/ls", argv);
    perror("ls failed");
    exit(EXIT_FAILURE);
}

// pwd 명령어 처리
void function_pwd(char **argv) {
    execvp("/bin/pwd", argv);
    perror("pwd failed");
    exit(EXIT_FAILURE);
}

// mkdir 명령어 처리
void function_mkdir(char **argv) {
    if (argv[1] == NULL) {
        printf("Usage: mkdir <directory>\n");
        return;
    }

    if (execvp("mkdir", argv) == -1) {
        perror("mkdir failed");
        exit(EXIT_FAILURE);
    }
}

// rmdir 명령어 처리
void function_rmdir(char **argv) {
    if (argv[1] == NULL) {
        printf("Usage: rmdir <directory>\n");
        return;
    }

    if (execvp("rmdir", argv) == -1) {
        perror("rmdir failed");
        exit(EXIT_FAILURE);
    }
}

// ln 명령어 처리
void function_ln(char **argv) {
    if (argv[1] == NULL || argv[2] == NULL) {
        printf("Usage: ln <source> <destination>\n");
        return;
    }

    if (execvp("ln", argv) == -1) {
        perror("ln failed");
        exit(EXIT_FAILURE);
    }
}

// cp 명령어 처리
void function_cp(char **argv) {
    if (argv[1] == NULL || argv[2] == NULL) {
        printf("Usage: cp <source> <destination>\n");
        return;
    }

    if (execvp("cp", argv) == -1) {
        perror("cp failed");
        exit(EXIT_FAILURE);
    }
}

void function_rm(char **argv) {
    if (argv[1] == NULL) {
        printf("Usage: rm <file>\n");
        return;
    }

    if (execvp("rm", argv) == -1) {
        perror("rm failed");
        exit(EXIT_FAILURE);
    }
}

// mv 명령어 처리
void function_mv(char **argv) {
    if (argv[1] == NULL || argv[2] == NULL) {
        printf("Usage: mv <source> <destination>\n");
        return;
    }

    if (execvp("mv", argv) == -1) {
        perror("mv failed");
        exit(EXIT_FAILURE);
    }
}

// cat 명령어 처리
void function_cat(char **argv) {
    execvp("/bin/cat", argv);
    perror("cat failed");
    exit(EXIT_FAILURE);
}
