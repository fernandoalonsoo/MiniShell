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

static void     print_prompt();
static void     read_command(char** buffer, tline** line);
static char*    read_line(FILE* archivo);
static void     execute_command(tline* line);

int main(){
    tline*  line;
    char*   buffer;

    while (1)
    {
        print_prompt();
        read_command(&buffer, &line);
        execute_command(line);
    }    
}

static void print_prompt() {
    char*   pwd;

    // fputs(PROMPT, stdout);

    pwd = getenv("PWD");
    printf("%s %s", pwd, PROMPT);   

}

static void read_command(char** buffer, tline** line) {

    *buffer = read_line(stdin);
    *line = tokenize(*buffer);
}

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

static void execute_command(tline* line){
    
    if (!(line -> ncommands > 0))
        return;

    // Creamos un proceso hijo
    pid_t pid = fork();

    // Tanto el proceso padre como el hijo empiezan a ejecutar los comandos

    // El pid == 0 avisa al proceso hijo que el es el hijo
    if (pid == 0) {
        // Esto se ejecuta en el proceso hijo
        if (execvp(line->commands[0].argv[0], line->commands[0].argv) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }
    } 
    // pid = (proceso hijo) fork a almacenado el proceso hijo en pid para indicar que este es el proceso padre
    else if (pid > 0) {
        // Esto se ejecuta en el proceso padre
        if (!line->background) {
            int status;
            waitpid(pid, &status, 0);
        }
    }
    // Nos han devuelto -1 ha habido error en el fork
    else {
        // Error al crear el proceso hijo
        perror("fork");
        exit(EXIT_FAILURE);
    }
}