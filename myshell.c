#include "parser.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>

#define PROMPT "\033[1;36m" "msh> " "\033[0m"
#define SIZE 1024 // Tamaño buffer de stdin

static void     print_prompt();
static void     read_command(char** buffer, tline** line);
static char*    read_line(FILE* archivo);
static void     manage_fd(tline* line, int n);
/* static void     manage_pipes(tline* line, int n); */
static void     execute_commands(tline* line);


int main(){
    tline*  line;
    char*   buffer;

    while (1)
    {
        print_prompt();
        read_command(&buffer, &line);
        execute_commands(line);
    }    
}

static void print_prompt() {
    // char*   pwd;

    fputs(PROMPT, stdout);

/*     pwd = getenv("PWD");
    printf("%s %s", pwd, PROMPT);    */

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

static void     manage_fd(tline* line, int n){
    int fd;

    fd = -1;
    // Comprobamos entrada estándar
    if (n == 0 && line-> redirect_input)
        fd = open(line-> redirect_input, O_RDONLY);

    else // Comprobamos salida y error estándar
    {
        if (line-> redirect_output)
            fd = open(line-> redirect_output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        else if (line-> redirect_error)
            fd = open(line-> redirect_error, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    }

    if (fd < 0)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }
    else
    {
        if (n == 0)
            dup2(fd, STDIN_FILENO);
        else if (n == 1)
            dup2(fd, STDOUT_FILENO);
        else 
            dup2(fd, STDERR_FILENO);

        close(fd);
    }
}

/* static void     manage_pipes(tline* line, int n){


} */
    
static void     execute_commands(tline* line){
    int i;
    int pipefd[2];
    int last_pipe;

    if (!(line -> ncommands > 0))
        return;

    last_pipe = -1;
    for (i = 0; i < line->ncommands; i++) 
    {
        // Si no estamos en el último comando creamos un pipe
        if (i < line -> ncommands - 1)
        {
            if (pipe(pipefd) == -1) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }
        }

        // Creamos un proceso hijo
        pid_t pid = fork();

        // Tanto el proceso padre como el hijo empiezan a ejecutar los comandos

        // El pid == 0 avisa al proceso hijo que el es el hijo
        if (pid == 0) {

            // Si no es el primer comando, redirigir la entrada estándar al pipe anterior
            if (last_pipe != -1) {
                if (dup2(last_pipe, STDIN_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(last_pipe);  // Cerramos después de redirigir
            }

            // Si no es el último comando, redirigir la salida estándar al pipe actual
            if (i < line->ncommands - 1) {
                close(pipefd[0]);  // Cerrar el extremo de lectura del pipe, no lo necesitamos aquí
                if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                close(pipefd[1]);  // Cerramos después de redirigir
            }

            // Comprobamos entrada estándar
            if (i == 0 && line-> redirect_input)
                manage_fd(line, 0);

            // Comprobamos salida y error estándar
            if (i == line-> ncommands - 1 && (line-> redirect_output || line -> redirect_error))
            {
                if (line -> redirect_output)
                    manage_fd(line, 1);
                else
                    manage_fd(line, 2);
            }

            if (execvp(line->commands[i].argv[0], line->commands[i].argv) == -1) {
                perror("execvp");
                exit(EXIT_FAILURE);
            }
        }   
        // pid = (proceso hijo) fork a almacenado el proceso hijo en pid para indicar que este es el proceso padre
        else if (pid > 0) {

            if (!line->background) {
                int status;
                waitpid(pid, &status, 0);
            }

            // El proceso padre cierra los extremos del pipe que no necesita
            if (last_pipe != -1) {
                close(last_pipe);  // Cerramos el extremo de lectura del pipe anterior
            }
            if (i < line->ncommands - 1) {
                close(pipefd[1]);  // Cerramos el extremo de escritura del pipe en el padre
                last_pipe = pipefd[0];  // Guardamos el extremo de lectura para el próximo comando
            }
        }
        // Nos han devuelto -1 ha habido error en el fork
        else {
            // Error al crear el proceso hijo
            perror("fork");
            exit(EXIT_FAILURE);
        }
    } 
    // Cerramos el último pipe
    if (last_pipe != -1) {
        close(last_pipe);
    }
}