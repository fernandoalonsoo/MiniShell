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
#define JOB_RUNNING 0
#define JOB_STOPPED 1

typedef struct job{
    pid_t pid;
    char *command;
    int status;
    int job_id;
    int is_bg;
    struct job *next;
} job_t;

static job_t    *jobs_list = NULL;
static int      next_job_id = 1;

// Signal handlers
static void sigint_handler(int sign);
static void sigtstp_handler(int sign);
static void check_background_jobs();

// Mandatos internos
static int      check_internal_commands(tline* line);
static int      execute_exit();
static int      execute_cd(tline* line);
static int      execute_jobs();
static int      execute_umask(tline* line);
static int      execute_bg(tline* line);

// Manejo de Jobs
static void     update_job_status(pid_t pid, int status);
static void     remove_job(pid_t pid);
static void     cleanup_jobs(void);

// Funciones auxiliares
static void     print_prompt();
static void     read_command(char** buffer, tline** line);
static char*    read_line(FILE* archivo);
static void     manage_fd(tline* line, int n);
static void     execute_commands(tline* line);

int main(){
    tline*              line;
    char*               buffer;
    struct sigaction    sa;

    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error al configurar SIGINT");
        exit(1);
    }

    sa.sa_handler = sigtstp_handler;
    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("Error al configurar SIGTSTP");
        exit(1);
    }

    

    while (1)
    {
        check_background_jobs();
        print_prompt();
        read_command(&buffer, &line);
        execute_commands(line);
    }    
}

static void sigint_handler(int sign) {
    (void)sign;
    printf("\n");
    fflush(stdout);
}
static void sigtstp_handler(int sign) {
    (void)sign;
    if (jobs_list != NULL) {
        job_t *current = jobs_list;
        if (!current->is_bg) {
            kill(current->pid, SIGTSTP);
            update_job_status(current->pid, JOB_STOPPED);
            printf("\nProceso %d detenido.\n", current->pid);
        }
    }
    printf("\n" PROMPT);
    fflush(stdout);
}
static void check_background_jobs() {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        job_t *job = jobs_list;
        while (job != NULL) {
            if (job->pid == pid) {
                if (WIFSTOPPED(status)){
                    job->status = JOB_STOPPED;
                    printf("\n[%d]+ Stopped\t%s\n", job->job_id, job->command);
                } else if (WIFEXITED(status) || WIFSIGNALED(status)){
                    printf("\n[%d]+ Done\t%s\n", job->job_id, job->command);
                    remove_job(pid);
                }
                break;
            }
            job = job->next;
        }
    }
}

static int execute_umask(tline* line) {
    mode_t mask;

    // Pipes?
    if (line->ncommands > 1) {
        fprintf(stderr, "umask no se puede ejecutar con pipes.\n");
        return 1;
    }

    // +1 argumento?
    if (line->commands[0].argc > 2) {
        fprintf(stderr, "demasiados argumentos.\n");
        return 1;
    }

    // =1 argumento?
    if (line->commands[0].argc == 2) {
        char*endptr;
        // convertir a octal
        mask = (mode_t)strtol(line->commands[0].argv[1], &endptr, 8);
        // octal valido?
        if (*endptr != '\0') {
            fprintf(stderr, "numero octal no valido.\n");
            return 1;
        }
        umask(mask);
        return 1;
    }
        
    // sin argumentos, mostrar máscara actual
    else {
        mask = umask(0);  // máscara actual
        umask(mask);      // restaurar
        printf("%04o\n", mask);
    }
    
    return 1;
}

static int     check_internal_commands(tline* line){

    if (strcmp(line->commands[0].argv[0], "exit") == 0)
        return execute_exit(line);
    if (strcmp(line->commands[0].argv[0], "cd") == 0)
        return execute_cd(line);
    if (strcmp(line->commands[0].argv[0], "jobs") == 0)
        return execute_jobs();
    if (strcmp(line->commands[0].argv[0], "bg") == 0)
    return execute_bg(line);
    if (strcmp(line->commands[0].argv[0], "umask") == 0)
        return execute_umask(line);
    return 0;
}

static int     execute_exit(){ // Como parametro los procesos en segundo plano
    // Matar procesos restantes

    job_t *current = jobs_list;
    if (current != NULL) {
        printf("Se están ejecutando procesos en segundo plano. \n");
        while (current != NULL) {
            printf("[%d] %d %s\n",
                current->job_id,
                current->pid,
                current->command);
            current = current->next;
        }
        return 1;
    }

    cleanup_jobs();
    
    exit(0);
    return 1; 
}

static int     execute_cd(tline* line){

        if (line -> commands[0].argc > 1)
        {
            if (chdir(line -> commands[0].argv[1]) != 0)
                perror("cd");
        }
        else
        {
            char *home = getenv("HOME");
            if (!home)
                perror("Cd: variable home no encontrada");
            else
            {
                if (chdir(home) != 0)
                    perror("cd");
            }
        }
        return 1; // Salir de la función exitosamente ejecutando cd
}

static char *create_command_string(tline *line) {
    char buffer[4096] = "";
    int offset = 0;
    
    for (int i = 0; i < line->ncommands; i++) {
        for (int j = 0; j < line->commands[i].argc; j++) {
            offset += snprintf(buffer + offset, sizeof(buffer) - offset,
                             "%s ", line->commands[i].argv[j]);
        }
        if (i < line->ncommands - 1) {
            offset += snprintf(buffer + offset, sizeof(buffer) - offset, "| ");
        }
    }
    if (line->background) {
        strcat(buffer, "&");
    }
    return strdup(buffer);
}

static void add_job(pid_t pid, tline *line) {
    job_t *new_job = malloc(sizeof(job_t));
    if (!new_job) {
        perror("malloc");
        return;
    }
    
    new_job->pid = pid;
    new_job->command = create_command_string(line);
    new_job->status = JOB_RUNNING;
    new_job->job_id = next_job_id++;
    new_job->is_bg = line->background;
    new_job->next = jobs_list;
    jobs_list = new_job;
}

static void remove_job(pid_t pid) {
    job_t *current = jobs_list;
    job_t *prev = NULL;
    
    while (current != NULL) {
        if (current->pid == pid) {
            if (prev == NULL) {
                jobs_list = current->next;
            } else {
                prev->next = current->next;
            }
            free(current->command);
            free(current);
            return;
        }
        prev = current;
        current = current->next;
    }
}

static int execute_bg(tline *line) {
    job_t *job = NULL;
    int job_id;

    if (line->commands[0].argc == 1) {
        job_t *current = jobs_list;
        while (current != NULL) {
            if (current->status == JOB_STOPPED) {
                job = current;
            }
            current = current->next;
        }
    } else if (line->commands[0].argc == 2) {
        job_id = atoi(line->commands[0].argv[1]);
        job_t *current = jobs_list;
        while (current != NULL) {
            if (current->job_id == job_id && current->status == JOB_STOPPED) {
                job = current;
                break;
            }
            current = current->next;
        }
    } else {
        fprintf(stderr, "bg: uso incorrecto\n");
        return -1;
    }

    if (job == NULL) {
        fprintf(stderr, "bg: no hay trabajos detenidos\n");
        return -1;
    }

    job->status = JOB_RUNNING;
    kill(job->pid, SIGCONT);
    printf("[%d]+ %s &\n", job->job_id, job->command);

    return 0;
}

static void cleanup_jobs(void) {
    while (jobs_list != NULL) {
        job_t *temp = jobs_list;
        jobs_list = jobs_list->next;
        free(temp->command);
        free(temp);
    }
}

static int execute_jobs(void) {
    job_t *current = jobs_list;
    job_t *mostRecent = jobs_list;
    job_t *last = NULL;
    char status_char;
    
    if (mostRecent != NULL && mostRecent->next != NULL) {
        last = mostRecent->next;
    }

    while (current != NULL) {
        if (current == mostRecent) {
            status_char = '+';  // El más reciente
        } else if (current == last) {
            status_char = '-';  // El penúltimo
        } else {
            status_char = ' ';  // El resto
        }
        
        printf("[%d]%c %s %s\n",
            current->job_id,
            status_char,
            (current->status == JOB_RUNNING) ? "Running" : "Stopped",
            current->command);
        
        current = current->next;
    }
    return 1;
}

static void update_job_status(pid_t pid, int status) {
    job_t *job = jobs_list;
    while (job != NULL) {
        if (job->pid == pid) {
            job->status = status;
            return;
        }
        job = job->next;
    }
}

static void print_prompt() {
    // char*   pwd;

     fputs(PROMPT, stdout);

/*     pwd = getenv("PWD");
    printf("%s %s", pwd, PROMPT);   */ 

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
    int     fd;
    char*   name;

    fd = -1;
    name = NULL;
    // Comprobamos entrada estándar
    if (n == 0 && line-> redirect_input){
        fd = open(line-> redirect_input, O_RDONLY);
        name = line->redirect_input;
    }
    else // Comprobamos salida y error estándar
    {
        if (line-> redirect_output)
        {
            fd = open(line-> redirect_output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            name = line->redirect_output;
        }
        else if (line-> redirect_error){
            fd = open(line-> redirect_error, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            name = line->redirect_error;
        }
    }

    if (fd < 0)
    {
        fprintf(stderr, "%s: No such file or directory\n", name);
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
    
static void     execute_commands(tline* line){
    int i;
    int pipefd[2];
    int last_pipe;

    if (!(line -> ncommands > 0))
        return;

    i = 0;
    if (check_internal_commands(line) == 1) {
        // Si es el único comando, no hace falta continuar
        if (line->ncommands == 1) {
            return;
        }
        i++;  // Sino incrementamos en 1
    }

    last_pipe = -1;
    for (; i < line->ncommands; i++) 
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
                fprintf(stderr, "%s: Command not found\n", line->commands[i].argv[0]);
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

            if (line->background && i == line->ncommands - 1) {
                add_job(pid, line);
                printf("[%d] %d\n", next_job_id - 1, pid);
            } else if (!line->background) {
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
    // Cerramos el último pipe
    if (last_pipe != -1) 
        close(last_pipe);
}