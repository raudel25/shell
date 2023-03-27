//
// Created by raudel on 3/23/2023.
//
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "decod.h"
#include "builtin.h"
#include "execute.h"
#include "list.h"

#define MY_SH_TOK_DELIM " \t\r\n\a"

List *pid_history = NULL;

int redirect_instr(char *args) {
    if (strcmp(args, "<") == 0)
        return 0;

    if (strcmp(args, ">") == 0)
        return 1;

    if (strcmp(args, ">>") == 0)
        return 2;

    if (strcmp(args, "|") == 0)
        return 3;

    return -1;
}

int redirect_out(char *fileName) {
    char *end_ptr = 0;

    int fd = (int) strtol(fileName, &end_ptr, 10);

    if (*(end_ptr + 1) != '\0') {
        fd = open(fileName, O_WRONLY | O_TRUNC | O_CREAT, 0600);
    }

    return fd;
}

int redirect_out_append(char *fileName) {
    char *end_ptr = 0;

    int fd = (int) strtol(fileName, &end_ptr, 10);

    if (*(end_ptr + 1) != '\0') {
        fd = open(fileName, O_WRONLY | O_APPEND | O_CREAT, 0600);
    }

    return fd;
}

int redirect_in(char *fileName) {
    char *end_ptr = 0;

    int fd = (int) strtol(fileName, &end_ptr, 10);

    if (*(end_ptr + 1) != '\0') {
        fd = open(fileName, O_RDONLY);
    }

    return fd;
}

int get_execute(char *variable) {
    if (variable == NULL) {
        for (int i = 0; i < 26; i++) {
            if (variables[i] != NULL) {
                printf("%c = ", (char) (i + 'a'));
                printf("%s\n", variables[i]);
            }
        }

        return 1;
    }

    if (check_variable(variable)) {
        for (int i = 0; i < 26; i++) {
            if (i != variable[0] - 'a')
                continue;

            if (variables[i] != NULL) {
                printf("%c = ", (char) (i + 'a'));
                printf("%s\n", variables[i]);

                return 1;
            }
        }
    }

    return 0;
}

int my_sh_launch(char **args, int fd_in, int fd_out, int fd_next) {
    pid_t pid;
    int status;

    pid = fork();
    if (pid == 0) {
        if (fd_in != -1) {
            dup2(fd_in, STDIN_FILENO);
            close(fd_in);
        }

        if (fd_out != -1) {
            dup2(fd_out, STDOUT_FILENO);
            close(fd_out);
        }

        if (strcmp(args[0], "get") == 0) {
            exit(get_execute(args[1]));
        }

        if (execvp(args[0], args) == -1) {
            perror("my_shell");
        }
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("my_shell");
    } else {
        append(pid_history, pid);

        if (fd_in != -1){
            close(fd_in);
        }
        if (fd_out != -1){
            close(fd_out);
        }

        if (fd_next == -1) {
            for (int i = 0; i < pid_history->len; ++i) {
                do {
                    int c_pid = pid_history->array[i];
                    waitpid(c_pid, &status, WUNTRACED);
                } while (!WIFEXITED(status) && !WIFSIGNALED(status));
            }
        }
    }

    return 1;
}

void my_sh_new_args(int init, char **args, int fd_in, int fd[3], int aux[2]) {
    fd[0] = fd_in != -1 ? fd_in : -1;
    fd[1] = -1;
    fd[2] = -1;

    int k = init;
    int instr = -1;

    while (args[k] != NULL) {
        instr = redirect_instr(args[k]);
        if (instr != -1)
            break;

        k++;
    }

    aux[0] = k;
    aux[1] = k + 1;

    if (instr == 0) {
        fd[0] = redirect_in(args[k + 1]);

        if (args[k + 2] != NULL) {
            instr = redirect_instr(args[k + 2]);
            if (instr == 1 || instr == 2) k += 2;
            if (instr == 3) k++;
        }

        aux[1] = k + 2;
    }
    if (instr == 1) {
        fd[1] = redirect_out(args[k + 1]);
        aux[1] = k + 2;
    }
    if (instr == 2) {
        fd[1] = redirect_out_append(args[k + 1]);
        aux[1] = k + 2;
    }
    if (instr == 3) {
        int fd1[2];
        pipe(fd1);

        fd[1] = fd1[1];
        fd[2] = fd1[0];
    }

}

int my_sh_command_again(char **args) {
    int status = 1;

    int q = 0;
    if (args[1] != NULL) {
        if (strlen(args[1]) == 1 && args[1][0] - '0' > 0 && args[1][0] - '0' < 10) q = args[1][0] - '0';
        if (strcmp(args[1], "10") == 0) q = 10;
    }

    char *c_again = get_again(q);

    if (q != 0 && c_again != NULL) {
        status = my_sh_execute(c_again, 1);
    } else
        printf("my_sh: incorrect command again\n");

    free(c_again);

    return status;
}

int my_sh_command_set(char **args) {
    int status = 1;

    if (args[1] != NULL && args[2] != NULL) {
        if (check_variable(args[1])) {
            if (variables[args[1][0] - 'a'] != NULL) {
                free(variables[args[1][0] - 'a']);
            }
            if (args[2][0] != '`') {
                variables[args[1][0] - 'a'] = (char *) malloc(strlen(args[2]));
                strcpy(variables[args[1][0] - 'a'], args[2]);
            } else {

            }
        }
    } else
        printf("my_sh: incorrect command set\n");

    return status;
}

int my_sh_execute(char *new_line, int save) {
    int status;
    char *aux = (char *) malloc(strlen(new_line));
    strcpy(aux, new_line);

    char **args = my_sh_split_line(new_line, MY_SH_TOK_DELIM);

    if (args[0] == NULL)
        return 1;

    if (strcmp(args[0], "again") == 0) {
        status = my_sh_command_again(args);
    } else if (strcmp(args[0], "set") == 0) {
        status = my_sh_command_set(args);
    } else {
        if (save) save_history(aux);

        status = my_sh_execute_args(args);
    }

    free(aux);
    free(args);

    return status;
}

int my_sh_execute_args(char **args) {
    for (int i = 0; i < my_sh_num_builtins(); i++) {
        if (strcmp(args[0], builtin_str[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }

    int fd[3];
    fd[2] = -1;
    int aux[2];
    int init = 0;
    int result;

    while (1) {
        my_sh_new_args(init, args, fd[2], fd, aux);
        int end = aux[0];

        char **new_args = (char **) malloc((end - init) * sizeof(char *));

        for (int i = init; i < end; i++) {
            new_args[i - init] = (char *) malloc(strlen(args[i]));
            strcpy(new_args[i-init],args[i]);
        }
        new_args[end] = NULL;

        result = my_sh_launch(new_args, fd[0], fd[1], fd[2]);
        free(new_args);

        init = aux[1];
        if (args[end] == NULL || args[init] == NULL)
            break;
    }

    return result;
}
