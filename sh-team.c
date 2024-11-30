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

    signal(SIGINT, handle_signal); // SIGINT (Ctrl-C) 처리
    signal(SIGQUIT, handle_signal); // SIGQUIT (Ctrl-Z) 처리

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
            buf[len - 1] = '\0'; // Remove '&'
            len--;
        }

        narg = getargs(buf, argv);

        // cd 명령어 처리
        if (strcmp(argv[0], "cd") == 0) {
            if (narg == 1) {
                printf("cd: missing argument\n");
            } else {
                if (chdir(argv[1]) == -1) {
                    perror("cd failed");
                }
            }
            continue; // cd 명령어는 자식 프로세스에서 실행하지 않음
        }

        // 명령어 처리 (리디렉션 및 파이프 처리 포함)
        pid = fork();
        if (pid == 0) { // 자식 프로세스: 명령어 실행
            // 파일 재지향 처리 및 파이프 처리
            handle_redirection_and_pipes(argv, narg);

            // function_* 함수들로 명령어 처리
            if (strcmp(argv[0], "ls") == 0) {
                function_ls(argv);
            } else if (strcmp(argv[0], "pwd") == 0) {
                function_pwd(argv);
            } else if (strcmp(argv[0], "echo") == 0) {
                function_echo(argv);
            } else if (strcmp(argv[0], "mkdir") == 0) {
                function_mkdir(argv);
            } else if (strcmp(argv[0], "rmdir") == 0) {
                function_rmdir(argv);
            } else if (strcmp(argv[0], "ln") == 0) {
                function_ln(argv);
            } else if (strcmp(argv[0], "cp") == 0) {
                function_cp(argv);
            } else if (strcmp(argv[0], "rm") == 0) {
                function_rm(argv);
            } else if (strcmp(argv[0], "mv") == 0) {
                function_mv(argv);
            } else if (strcmp(argv[0], "cat") == 0) {
                function_cat(argv);
            } else {
                printf("Unknown command: %s\n", argv[0]);
                exit(EXIT_FAILURE);
            }
            exit(EXIT_SUCCESS); // 명령어 실행 후 종료
        } else if (pid > 0) { // 부모 프로세스
            if (!is_background) { // 백그라운드가 아니면 자식 프로세스 종료를 기다림
                waitpid(pid, NULL, 0);
            } else { // 백그라운드 명령어를 실행하고 바로 다음 프롬프트로 돌아감
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

    // 파이프 처리
    int pipe_flag = 0;
    for (i = 0; i < narg; i++) {
        if (strcmp(argv[i], "|") == 0) {
            argv[i] = NULL;  // 파이프 위치에서 명령어 분리
            pipe_flag = 1;   // 파이프가 있음을 표시
            break;
        }
    }

    if (pipe_flag) {
        pipe(pipe_fds);
        if (fork() == 0) {
            dup2(pipe_fds[1], STDOUT_FILENO); // 출력 파이프 연결
            close(pipe_fds[0]);
            execvp(argv[0], argv); // 첫 번째 명령어 실행
            perror("exec failed");
            exit(EXIT_FAILURE);
        } else {
            dup2(pipe_fds[0], STDIN_FILENO); // 입력 파이프 연결
            close(pipe_fds[1]);
            execvp(argv[i + 1], &argv[i + 1]); // 두 번째 명령어 실행
            perror("exec failed");
            exit(EXIT_FAILURE);
        }
    }
    // 기존의 리디렉션 처리
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

// 'echo' 명령어 처리
void function_echo(char **argv) {
    for (int i = 1; argv[i] != NULL; i++) {
        printf("%s ", argv[i]);
    }
    printf("\n");
}

// 'ls' 명령어 처리
void function_ls(char **argv) {
    execvp("/bin/ls", argv);
    perror("ls failed");
    exit(EXIT_FAILURE);
}

// 'pwd' 명령어 처리
void function_pwd(char **argv) {
    execvp("/bin/pwd", argv);
    perror("pwd failed");
    exit(EXIT_FAILURE);
}

// 'mkdir' 명령어 처리
void function_mkdir(char **argv) {
    if (mkdir(argv[1], 0755) == -1) {
        perror("mkdir failed");
    }
}

// 'rmdir' 명령어 처리
void function_rmdir(char **argv) {
    if (rmdir(argv[1]) == -1) {
        perror("rmdir failed");
    }
}

// 'ln' 명령어 처리
void function_ln(char **argv) {
    if (link(argv[1], argv[2]) == -1) {
        perror("ln failed");
    }
}

// 'cp' 명령어 처리
void function_cp(char **argv) {
    if (argv[1] == NULL || argv[2] == NULL) {
        printf("Usage: cp <source> <destination>\n");
        return;
    }

    int src_fd, dest_fd;
    char buffer[1024];
    ssize_t bytes_read, bytes_written;

    // 소스 파일 열기
    src_fd = open(argv[1], O_RDONLY);
    if (src_fd == -1) {
        perror("Failed to open source file");
        return;
    }

    // 대상 파일 열기 (쓰기 모드)
    dest_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dest_fd == -1) {
        perror("Failed to open destination file");
        close(src_fd);
        return;
    }

    // 파일 복사
    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        bytes_written = write(dest_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            perror("Error writing to destination file");
            close(src_fd);
            close(dest_fd);
            return;
        }
    }

    if (bytes_read == -1) {
        perror("Error reading source file");
    }

    // 파일 닫기
    close(src_fd);
    close(dest_fd);

    printf("파일 복사 성공\n");
}

// 'rm' 명령어 처리
void function_rm(char **argv) {
    if (remove(argv[1]) == -1) {
        perror("rm failed");
    }
}

// 'mv' 명령어 처리
void function_mv(char **argv) {
    struct stat statbuf;
    if (stat(argv[2], &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
        // 두 번째 인자가 디렉터리인 경우, 파일을 해당 디렉터리로 이동
        char new_path[1024];
        snprintf(new_path, sizeof(new_path), "%s/%s", argv[2], argv[1]);
        if (rename(argv[1], new_path) == -1) {
            perror("mv failed");
        }
    } else {
        // 두 번째 인자가 디렉터리가 아니면 기존 방식으로 이동
        if (rename(argv[1], argv[2]) == -1) {
            perror("mv failed");
        }
    }
}

// 'cat' 명령어 처리
void function_cat(char **argv) {
    execvp("/bin/cat", argv);
    perror("cat failed");
    exit(EXIT_FAILURE);
}

