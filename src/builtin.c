//
// Created by raudel on 3/25/2023.
//

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <pwd.h>
#include <readline/history.h>

#include "builtin.h"
#include "decode.h"
#include "help.c"
#include "execute.h"

#define BOLD_BLUE "\033[1;34m"
#define RESET "\033[0m"
#define ERROR "\033[1;31mmy_sh\033[0m"

#define HISTORY_FILE ".my_sh_history"
#define MY_SH_TOK_BUF_SIZE 1024
#define MY_SH_MAX_HISTORY 100
#define MY_SH_TOK_DELIM " \t\r\n\a"

int current_pid;

int last_pid;

List *background_pid = NULL;

GList *variables_key;

GList *variables_value;

struct passwd *pw = NULL;

int num_commands() {
    return sizeof(commands) / sizeof(char *);
}

char *builtin_str[7] = {
        "cd",
        "unset",
        "fg",
        "exit",
        "true",
        "false",
        "set"
};

char *builtin_str_out[4] = {
        "help",
        "history",
        "jobs",
        "get"
};

int (*builtin_func[7])(char **) = {
        &my_sh_cd,
        &my_sh_unset,
        &my_sh_foreground,
        &my_sh_exit,
        &my_sh_true,
        &my_sh_false,
        &my_sh_set
};

int (*builtin_func_out[4])(char **) = {
        &my_sh_help,
        &my_sh_history,
        &my_sh_jobs,
        &my_sh_get
};


char *my_sh_path_history() {
    char *path = (char *) malloc(sizeof(char) * (strlen(pw->pw_dir) + strlen(HISTORY_FILE) + 1));

    strcpy(path, pw->pw_dir);
    strcat(path, "/");
    strcat(path, HISTORY_FILE);

    return path;
}

void my_sh_load_history(){
    char *path = my_sh_path_history();
    read_history(path);
    stifle_history(MY_SH_MAX_HISTORY);
    free(path);
}

int my_sh_num_builtins() {
    return sizeof(builtin_str) / sizeof(char *);
}

int my_sh_num_builtins_out() {
    return sizeof(builtin_str) / sizeof(char *);
}


int my_sh_cd(char **args) {
    if (args[1] == NULL) {
        if (chdir(pw->pw_dir) != 0) {
            perror(ERROR);

            return 1;
        }
    } else {
        if (chdir(args[1]) != 0) {
            perror(ERROR);

            return 1;
        }
    }
    return 0;
}

int my_sh_help(char **args) {
    if (args[1] == NULL) {
        printf("\n");

        for (int i = 0; i < num_commands(); i++) {
            printf("%s%s%s: %s\n", BOLD_BLUE, commands[i], RESET, commands_help[i]);
        }

        return 0;
    }

    for (int i = 0; i < num_commands(); i++) {
        if (strcmp(commands[i], args[1]) == 0) {
            printf("%s", commands_help[i]);
            return 0;
        }
    }

    fprintf(stderr, "%s: command not found\n", ERROR);

    return 1;
}

void my_sh_save_history(char *line){
    add_history(line);
}

int my_sh_exit() {
    char *path = my_sh_path_history();
    write_history(path);
    free(path);

    exit(EXIT_SUCCESS);
}

int my_sh_history() {
    HIST_ENTRY **list = history_list();
    for (int j = 0; j < history_length; j++) {
        printf("%d: %s\n", j + 1, list[j]->line);
    }

    return 0;
}

void my_sh_init_variables() {
    variables_key = createListG();
    variables_value = createListG();
}

int contains_key(char *key) {
    for (int i = 0; i < variables_key->len; i++) {
        if (strcmp(key, variables_key->array[i]) == 0) return i;
    }

    return -1;
}

int my_sh_unset(char **args) {
    if (args[1] != NULL) {
        int index = contains_key(args[1]);

        if (index != -1) {
            removeAtIndexG(variables_key, index);
            removeAtIndexG(variables_value, index);
        } else {
            fprintf(stderr, "%s: variable not found\n", ERROR);

            return 1;
        }
    } else {
        fprintf(stderr, "%s: incorrect command unset\n", ERROR);

        return 1;
    }

    return 0;
}

int my_sh_foreground(char **args) {
    if (background_pid->len == 0) {
        fprintf(stderr, "%s: the process does not exist in the background\n", ERROR);
        return 1;
    }

    int c_pid;
    int status = 1;

    int index = args[1] == NULL ? background_pid->len - 1 : (int) strtol(args[1], 0, 10) - 1;
    if (index >= background_pid->len) {
        fprintf(stderr, "%s: the process does not exist in the background\n", ERROR);
        return 1;
    }

    c_pid = background_pid->array[index];
    current_pid = c_pid;
    do {
        waitpid(c_pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));

    removeAtIndex(background_pid, index);

    return status;
}

int my_sh_jobs() {
    for (int i = 0; i < background_pid->len; i++) {
        printf("[%d]\t%d\n", i + 1, background_pid->array[i]);
    }

    return 0;
}

int my_sh_get(char **args) {
    if (args[1] == NULL) {
        for (int i = 0; i < variables_key->len; i++) {
            printf("%s%s%s = %s\n", BOLD_BLUE, variables_key->array[i], RESET, variables_value->array[i]);
        }

        return 0;
    }

    int index = contains_key(args[1]);
    if (index != -1) {
        printf("%s\n", variables_value->array[index]);

        return 0;
    } else {
        fprintf(stderr, "%s: variable not found\n", ERROR);
    }

    return 1;
}

int my_sh_true() {
    return 0;
}

int my_sh_false() {
    return 1;
}

int my_sh_set(char **args) {
    int status = 0;

    if (args[1] != NULL && args[2] != NULL) {
            if (args[2][0] != '`') {
                int index = contains_key(args[1]);
                if (index != -1) {
                    removeAtIndexG(variables_key, index);
                    removeAtIndexG(variables_value, index);
                }
                char *aux_value = (char *) malloc(sizeof(char) * strlen(args[2]));
                char *aux_key = (char *) malloc(sizeof(char) * strlen(args[1]));
                strcpy(aux_value, args[2]);
                strcpy(aux_key, args[1]);
                appendG(variables_key, aux_key);
                appendG(variables_value, aux_value);
            } else {
                char *new_command = NULL;

                if (args[2][strlen(args[2]) - 1] == '`') {
                    new_command = sub_str(args[2], 1, (int) strlen(args[2]) - 2);
                    my_sh_decode_set(new_command);
                }

                if (new_command != NULL) {
                    char *new_command_format = my_sh_decode_line(new_command);

                    char *buffer = (char *) malloc(MY_SH_TOK_BUF_SIZE);
                    char c = 1;
                    int i = 0;
                    fflush(stdout);

                    int temp_stdout;
                    temp_stdout = dup(fileno(stdout));

                    int fd[2];
                    pipe(fd);
                    dup2(fd[1], fileno(stdout));

                    my_sh_execute(new_command_format, 0);

                    write(fd[1], "", 1);
                    close(fd[1]);

                    fflush(stdout);
                    dup2(temp_stdout, fileno(stdout));
                    close(temp_stdout);

                    while (1) {
                        read(fd[0], &c, 1);
                        buffer[i] = c;
                        if (c == 0)
                            break;
                        i++;
                        if (i % MY_SH_TOK_BUF_SIZE == 0) {
                            buffer = (char *) realloc(buffer, i * 2);
                        }
                    }

                    close(fd[0]);

                    if (i != 0) {
                        buffer[i] = 0;
                        if (buffer[i - 1] == '\n')
                            buffer[i - 1] = 0;
                        int index = contains_key(args[1]);
                        if (index != -1) {
                            removeAtIndexG(variables_key, index);
                            removeAtIndexG(variables_value, index);
                        }
                        char *aux_key = (char *) malloc(sizeof(char) * strlen(buffer));
                        strcpy(aux_key, args[1]);
                        appendG(variables_key, aux_key);
                        appendG(variables_value, buffer);
                    } else {
                        fprintf(stderr, "%s: the output of the command is null\n", ERROR);
                        status = 1;
                    }
                    free(new_command);
                    free(new_command_format);
                } else {
                    fprintf(stderr, "%s: incorrect command set\n", ERROR);
                    status = 1;
                }
            }
    } else {
        status = 1;
    }

    return status;
}

int my_sh_background(char **args, int init, int end) {
    int pid;

    pid = fork();
    if (pid == 0) {
        setpgid(0, 0);

        exit(my_sh_parser(args, init, end, -1, -1));
    } else if (pid > 0) {
        setpgid(pid, pid);
        append(background_pid, pid);
        printf("[%d]\t%d\n", background_pid->len, pid);
    }

    return 0;
}

char *my_sh_again(char *line) {
    char **args = my_sh_split_line(line, MY_SH_TOK_DELIM);
    char *aux = (char *) malloc(MY_SH_TOK_BUF_SIZE);
    strcpy(aux, "");

    int q;
    int last;
    int error = 0;

    int len = array_size(args);
    for (int i = 0; i < len; i++) {
        if (strcmp(args[i], "again") == 0) {
            last = 0;

            if (args[i + 1] != NULL) {
                char *p;
                q = (int) strtol(args[i + 1], &p, 10);

                if (q == 0) last = 1;
                else i++;
            } else last = 1;

            if (last) q = history_length;

            if (q <= history_length) {
                HIST_ENTRY **list = history_list();
                strcat(aux, list[q - 1]->line);
            } else {
                error = 1;
                strcat(aux, "false");
            }
        } else {
            strcat(aux, args[i]);
        }

        if (i != len - 1) {
            strcat(aux, " ");
        }
    }

    if (error) {
        fprintf(stderr, "%s: incorrect command again\n", ERROR);
    }

    free(args);
    return aux;
}

void my_sh_update_background() {
    int status;

    if (background_pid->len > 0) {
        for (int i = 0; i < background_pid->len; ++i) {
            waitpid(background_pid->array[i], &status, WNOHANG);
            if (WIFEXITED(status)) {
                printf("[%d]\tDone\t%d\n", i + 1, background_pid->array[i]);
                removeAtIndex(background_pid, i);

                i = -1;
            }
        }
    }
}

