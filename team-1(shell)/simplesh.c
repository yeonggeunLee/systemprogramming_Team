#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_ARGS 50

void handle_signal(int signo);

int getargs(char *cmd, char **argv);

int main(void)
{
    char buf[256];
    char *argv[MAX_ARGS + 1];
    int narg;
    pid_t pid;
    int is_background;

    signal(SIGINT, handle_signal);
    signal(SIGQUIT, handle_signal);

    while (1) {
    	// 쉘처럼 동작하게 하기 위한 내용
        printf("shell> ");
        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            perror("fgets failed");
            exit(EXIT_FAILURE);
        }

        // 개행 문자 제거
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') {
            buf[len - 1] = '\0';
        }

        // exit 확인
        if (strcmp(buf, "exit") == 0) {
            break;
        }

        // 백그라운드 실행 여부 확인
        is_background = 0;
        if (len > 0 && buf[len - 1] == '&') {
            is_background = 1;
            buf[len - 1] = '\0'; // Remove '&'
        }

        narg = getargs(buf, argv);

        if (narg > 0) {
            if (strcmp(argv[narg - 1], "&") == 0) {
                is_background = 1;
                argv[narg - 1] = NULL; // Remove '&'
            }
            if (strcmp(argv[0], "ls") == 0){            
                pid = fork();
                if (pid == 0) {
                    execvp("/bin/ls", argv);
                    perror("ls failed");
                    exit(EXIT_FAILURE);
                } else if (pid > 0) {
                    if (!is_background) {
                        waitpid(pid, NULL, 0);
                    }
                } else {
                      perror("fork failed");
                }
            } else if (strcmp(argv[0], "pwd") == 0) {
                pid = fork();
                if (pid == 0) {
                    execvp("/bin/pwd", argv);
                    perror("pwd failed");
                    exit(EXIT_FAILURE);
                } else if (pid > 0) {
                    if (!is_background) {
                        waitpid(pid, NULL, 0);
                    }
                } else {
                      perror("fork failed");
                }

            } else if (strcmp(argv[0], "cd") == 0) {
                if (narg != 2) {
                    fprintf(stderr, "Usage: cd directory\n");
                } else {
                      if (chdir(argv[1]) == -1) {
                          perror("cd failed");
                      }
                }
            } else if (strcmp(argv[0], "mkdir") == 0) {
                  if (narg != 2) {
                      fprintf(stderr, "Usage: mkdir directory\n");
                  } else {
                      if (mkdir(argv[1], 0777) == -1) {
                          perror("mkdir failed");
                      }
                  }
            } else if (strcmp(argv[0], "rmdir") == 0) {
                if (narg != 2) {
                  fprintf(stderr, "Usage: rmdir directory\n");
                } else {
                    if (rmdir(argv[1]) == -1) {
                      perror("rmdir failed");
                    }
                }
            } else if (strcmp(argv[0], "ln") == 0) {
                if (narg != 3) {
                    fprintf(stderr, "Usage: ln source_file target_link\n");
                } else {
                    if (link(argv[1], argv[2]) == -1) {
                        perror("ln failed");
                    }
                }
            } else if (strcmp(argv[0], "cp") == 0) {
                if (narg != 3) {
                    fprintf(stderr, "Usage: cp source_file destination\n");
                } else {
                    // 원본 파일 열기
                    int src_fd = open(argv[1], O_RDONLY);
                    if (src_fd == -1) {
                        perror("cp failed");
                        continue;
                    }

                    // 대상 파일 열기
                    int dest_fd = open(argv[2], O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    if (dest_fd == -1) {
                        perror("cp failed");
                        close(src_fd);
                        continue;
                    }

                    // 내용 복사
                    char buffer[4096];
                    ssize_t bytesRead;
                    while ((bytesRead = read(src_fd, buffer, sizeof(buffer))) > 0) {
                        if (write(dest_fd, buffer, bytesRead) == -1) {
                            perror("cp failed");
                            break;
                        }
                    }

                    close(src_fd);
                    close(dest_fd);
                }
            } else if (strcmp(argv[0], "rm") == 0) {
                if (narg != 2) {
                    fprintf(stderr, "Usage: rm file\n");
                } else {
                    if (unlink(argv[1]) == -1) {
                        perror("rm failed");
                    }
                }
            } else if (strcmp(argv[0], "mv") == 0) {
                if (narg != 3) {
                    fprintf(stderr, "Usage: mv source_file destination\n");
                } else {
                    if (rename(argv[1], argv[2]) == -1) {
                        perror("mv failed");
                    }
                }
            } else if (strcmp(argv[0], "cat") == 0) {
                if (narg != 2) {
                    fprintf(stderr, "Usage: cat file\n");
                } else {
                    // 파일 열기
                    int fd = open(argv[1], O_RDONLY);
                    if (fd == -1) {
                        perror("cat failed");
                        continue;
                    }

                    // 내용 출력
                    char buffer[4096];
                    ssize_t bytesRead;
                    while ((bytesRead = read(fd, buffer, sizeof(buffer))) > 0) {
                        if (write(STDOUT_FILENO, buffer, bytesRead) == -1) {
                            perror("cat failed");
                            break;
                        }
                    }

                    close(fd);
                }
            } else {
                int pipe_index = -1;
                for (int i = 0; i < narg; ++i) {
                    if (strcmp(argv[i], "|") == 0) {
                        pipe_index = i;
                        break;
                    }
                }

                if (pipe_index != -1) {
                    argv[pipe_index] = NULL;

                    int pipefd[2];
                    if (pipe(pipefd) == -1) {
                        perror("pipe failed");
                        exit(EXIT_FAILURE);
                    }

                    pid = fork();

                    if (pid == 0) {
                        close(pipefd[0]);
                        dup2(pipefd[1], STDOUT_FILENO);
                        close(pipefd[1]);

                        execvp(argv[0], argv);

                        perror("execvp failed");
                        exit(EXIT_FAILURE);
                    } else if (pid > 0) {
                        waitpid(pid, NULL, 0);

                        pid_t pid2 = fork();

                        if (pid2 == 0) {
                            close(pipefd[1]);
                            dup2(pipefd[0], STDIN_FILENO);
                            close(pipefd[0]);

                            execvp(argv[pipe_index + 1], &argv[pipe_index + 1]);

                            perror("execvp failed");
                            exit(EXIT_FAILURE);
                        } else if (pid2 > 0) {
                            close(pipefd[0]);
                            close(pipefd[1]);

                            if (!is_background) {
                                waitpid(pid2, NULL, 0);
                            }
                        } else {
                            perror("fork failed");
                        }
                    } else {
                        perror("fork failed");
                    }
                } else {
                    pid = fork();

                    if (pid == 0) {
                        if (is_background) {
                            setsid();
                        }

                        execvp(argv[0], argv);

                        perror("execvp failed");
                        exit(EXIT_FAILURE);
                    } else if (pid > 0) {
                        if (!is_background) {
                            waitpid(pid, NULL, 0);
                        }
                    } else {
                        perror("fork failed");
                    }
                }
            }
        }
    }

    return 0;
}

void handle_signal(int signo)
{
    if (signo == SIGINT) {
        printf("\nCaught Ctrl-C (SIGINT) signal. Press Enter to continue.\n");
    } else if (signo == SIGQUIT) {
        printf("\nCaught Ctrl-Z (SIGQUIT) signal. Press Enter to continue.\n");
    }

    fflush(stdin);
}

int getargs(char *cmd, char **argv)
{
    int narg = 0;
    while (*cmd) {
        if (*cmd == ' ' || *cmd == '\t' || *cmd == '\n')
            *cmd++ = '\0';
        else {
            argv[narg++] = cmd++;
            while (*cmd != '\0' && *cmd != ' ' && *cmd != '\t' && *cmd != '\n')
                cmd++;
        }
    }
    argv[narg] = NULL;

    return narg;
}

