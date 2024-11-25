#include "parser.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#define PROMPT "\033[1;36m" "msh> " "\033[0m"
#define SIZE 1024 // Tamaño buffer de stdin

static void execute_command(tline* line);

static char* read_line(FILE* archivo) {
    char* linea;

    linea = malloc(SIZE);
    if (linea == NULL) {
        return NULL;
    }
    if (fgets(linea, SIZE, archivo) == NULL) {
        free(linea);
        return NULL;
    }
    return linea;
}

int main(){
    tline*    line;
    char*    buffer;

    fputs(PROMPT, stdout);
    while (buffer = read_line(stdin))
    {
        line = tokenize(buffer);
        if (line -> ncommands > 0)
            execute_command(line);
        fputs(PROMPT, stdout);
    }
    
}

static void execute_command(tline* line){
    
    pid_t pid = fork();

    if (pid == 0) {
        // Esto se ejecuta en el proceso hijo
        if (execvp(line->commands[0].argv[0], line->commands[0].argv) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
    }

    } else if (pid > 0) {
        // Esto se ejecuta en el proceso padre
        if (!line->background) {
            int status;
            waitpid(pid, &status, 0);
    }

    } else {
        // Error al crear el proceso hijo
        perror("fork");
    }
}