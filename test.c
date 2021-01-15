#include "parser.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

tline *line; 
/*----------------------------------Estructuras de datos------------------------*/

/*---------------------Nodo--------------------*/
struct nodo {
    struct nodo *sig;
    tline *contenido;
    char *linea;
    pid_t pid;
};

typedef struct nodo Nodo;

/*Metodos nodo*/
/*Getter nodo*/
Nodo *getSigNodo(Nodo *pNodo) {
    return pNodo->sig;
}

Nodo *CrearNodo(char *valor, pid_t pid) {
    Nodo *newNodo = (Nodo *) malloc(sizeof(Nodo));
    newNodo->sig = NULL;
    newNodo->contenido = tokenize(valor);
    /*Se crea el token para que cuando se introduzca un nuevo comando
     * este no sea sustituido en todos los nodos*/
    newNodo->linea = strdup(valor);
    newNodo->pid = pid;
    return newNodo;
}

void MostrarLinea(Nodo *pNodo) {
    printf(">>[%i]    ", pNodo->pid);
    printf("%s", pNodo->linea);
    printf("\n");
}

void destruirNodo(Nodo *pNodo) {
    if (pNodo == NULL) {
        free(pNodo);
    } else {
        kill(SIGKILL, pNodo->pid);
        free(pNodo->linea);
        free(pNodo);
    }
}

/*---------------------Pila--------------------*/
struct pila {
    struct nodo *head;
};

typedef struct pila Pila;
/*Variable global jobs*/
Pila *jobs;

/*Metodos Pila*/

Pila *InicializarPila() {
    Pila *newList = (Pila *) malloc(sizeof(Pila));
    newList->head = NULL;
    return newList;
}

Pila *AnnadirNodoALaPila(char *valor, pid_t pid) {
    Nodo *newNodo = CrearNodo(valor, pid);
    newNodo->sig = jobs->head;
    jobs->head = newNodo;
    return jobs;
}


void MostrarPila() {
    Nodo *cursor;
    cursor = jobs->head;

    printf("  PID     MANDATO\n");
    while (cursor != NULL) {
        MostrarLinea(cursor);
        cursor = getSigNodo(cursor);
    }
}

void EliminarProcesoCabeza() {
    Nodo *cursor = jobs->head;
    jobs->head = jobs->head->sig;
    destruirNodo(cursor);
}

void EliminarCursor(Nodo *cursor, Nodo *ant) {
    if (cursor == jobs->head) {
        EliminarProcesoCabeza();
    } else {
        ant->sig = cursor->sig;
        destruirNodo(cursor);
    }
}

void EliminarPID(pid_t pid) {
    Nodo *cursor = jobs->head;
    Nodo *ant = NULL;
    while (cursor->pid != pid) {
        ant = cursor;
        cursor = cursor->sig;
    }
    EliminarCursor(cursor, ant);
}


void destruirPila() {
    Nodo *nodoBorrar;
    Nodo *nodo;

    if (jobs == NULL) {
        free(jobs);
    } else {
        nodo = jobs->head;
        while (nodo != NULL) {
            nodoBorrar = nodo;
            nodo = getSigNodo(nodo);
            destruirNodo(nodoBorrar);
        }
        free(jobs);
    }
}

/*---------------------------Errores------------------------------------------*/

void RevisarErrorFork(int pid) {
    if (pid < 0) {
        fprintf(stderr, "Error, fallo al crear el hijo\n");
        exit(1);
    }
}

void RevisarErrorMandato(int contador) {
    if (line->commands[contador].filename == NULL) {
        fprintf(stderr, "%s: No se encuentra el mandato\n", line->commands[contador].argv[0]);
        exit(2);
    }
}

void RevisarErrorAperturaFichero(int fileDescriptor) {
    if (fileDescriptor < 0) {
        fprintf(stderr, "%s: Error. No se puede abrir el fichero\n", line->redirect_input);
        exit(3);
    }
}

void RevisarErrorCreacionFicheroStdout(int fileDescriptor) {
    if (fileDescriptor < 0) {
        fprintf(stderr, "%s: Error. No se puede crear el fichero\n", line->redirect_output);
        exit(4);
    }
}

void RevisarErrorCreacionFicheroStderr(int fileDescriptor) {
    if (fileDescriptor < 0) {
        fprintf(stderr, "%s: Error. No se puede crear el fichero\n", line->redirect_error);
        exit(5);
    }
}

void RevisarErrorDup2(int errorCode) {
    if (errorCode < 0) {
        fprintf(stderr, "Se produjo un error en las redirecciones(dup2)");
        exit(6);
    }
}

void RevisarErrorCd(char *dir, int errorInt) {
    if (errorInt < 0) {
        fprintf(stderr, "Error, no se pudo cambiar al directorio: %s\n", dir);
    }
}

/*---------------------Gestión de pipes----------------*/
int **CrearArrayPipes() {
    int **arrayPipes;
    int contador;
    //Si solo hay un comando no hace falta crear pipes
    if (line->ncommands == 1) {
        return NULL;
    } else {
        arrayPipes = (int **) malloc(sizeof(int *) * line->ncommands - 1);
        for (contador = 0; contador < line->ncommands - 1; contador++) {
            //Se crean dos int que actuarán como pipe
            arrayPipes[contador] = (int *) malloc(sizeof(int) * 2);
            pipe(arrayPipes[contador]);
        }
        return arrayPipes;
    }
}

void CerrarPipesExcepto(int **arrayPipes, int excepcion) {
    int contador;

    for (contador = 0; contador < line->ncommands; contador++) {
        if (contador == excepcion) {
            continue;
        } else {
            if (contador == 0) {
                close(arrayPipes[0][1]);
            } else if (contador == (line->ncommands - 1)) {
                close(arrayPipes[contador - 1][0]);
            } else {
                close(arrayPipes[contador - 1][0]);
                close(arrayPipes[contador][1]);
            }
        }
    }
}

void GestionarPipesIO(int **arrayPipes, int contador) {
    int errorCode;

    if (line->ncommands > 1) {
        if (contador == 0) {
            errorCode = dup2(arrayPipes[0][1], 1);
        } else if (contador == (line->ncommands - 1)) {
            errorCode = dup2(arrayPipes[contador - 1][0], 0);
        } else {
            errorCode = dup2(arrayPipes[contador][1], 1);
            errorCode += dup2(arrayPipes[contador - 1][0], 0);
        }
        RevisarErrorDup2(errorCode);
        CerrarPipesExcepto(arrayPipes, contador);
    }
}

void DestruirArrayPipes(int **arrayPipes) {
    int contador;
    if (line->ncommands > 1) {/*En el caso de que solo haya un argumento el array de pipes será NULL*/
        for (contador = 0; contador < line->ncommands - 1; contador++) {
            close(arrayPipes[contador][0]);
            close(arrayPipes[contador][1]);
            free(arrayPipes[contador]);
        }
    }
    free(arrayPipes);
}

/*---------------------Gestión de redirecciones----------------*/
void GestionarRedireccionesEntradaFichero(int contador) {
    int errorCode;
    int fileDescriptor;

    if (contador == 0) {
        if (line->redirect_input != NULL) {
            fileDescriptor = open(line->redirect_input, O_RDONLY);
            RevisarErrorAperturaFichero(fileDescriptor);

            errorCode = dup2(fileDescriptor, 0);
            RevisarErrorDup2(errorCode);
            close(fileDescriptor);
        }
    }
}

void GestionarRedireccionesSalidaFichero(int contador) {
    int errorCode;
    int fileDescriptor;

    if (contador == line->ncommands - 1) {
        if (line->redirect_output != NULL) {
            fileDescriptor = creat(line->redirect_output, 0644);
            RevisarErrorCreacionFicheroStdout(fileDescriptor);

            errorCode = dup2(fileDescriptor, 1);
            RevisarErrorDup2(errorCode);
            close(fileDescriptor);
        }
    }
}

void GestionarRedireccionesErrorFichero(int contador) {
    int errorCode;
    int fileDescriptor;

    if (contador == line->ncommands - 1) {
        if (line->redirect_error != NULL) {
            fileDescriptor = creat(line->redirect_error, 0644);
            RevisarErrorCreacionFicheroStderr(fileDescriptor);

            errorCode = dup2(fileDescriptor, 2);
            RevisarErrorDup2(errorCode);
            close(fileDescriptor);
        }
    }
}

/*---------------------Gestión de hijos----------------*/
//se esperan a los hijos creados
void EsperarHijos(int *arrayPIDs) {
    int contador;
    for (contador = 0; contador < line->ncommands; contador++) {
        waitpid(arrayPIDs[contador], NULL, 0);
    }
}
//comprueba como ha acabado el hijo
int HijoHaTerminado(int status) {
    if (WIFEXITED(status)) {
        return 1;
    }
    if (WIFSIGNALED(status)) {
        return 1;
    }
    return 0;

}
//se estableceran las señales del fg
void CambiarSenalesForeground() {
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
}
//se estableceran las señales del bg
void CambiarSenalesBackground() {
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
}
//se comprueba que señal se debe cambiar
void AjustarSenalesProcesoHijo() {
    if (line->background == 0) {
        CambiarSenalesBackground();
    } else if (line->background == 1) {
        CambiarSenalesForeground();
    }
}

void LimpiarJobs() {
    Nodo *cursor = jobs->head;
    Nodo *ant = NULL;
    int status;
    //si la pila no sta vacia se comprobará si el hijo ha acabado o no
    while (cursor != NULL) {
        waitpid(cursor->pid, &status, WNOHANG);
        if (HijoHaTerminado(status)) {
            //si ha acabado lo elimina de la pila
            EliminarCursor(cursor, ant);
            if (ant != NULL) {
                cursor = ant->sig;
            }else{
                cursor=jobs->head;
            }
        }
    }
}

/*---------------------Gestión de comandos----------------*/
void ExecuteCD(void) {
    char *dir;
    int errorInt;
    //si no se introduce un directorio nos llevará a $HOME
    if (line->commands[0].argv[1] == NULL) {
        dir = getenv("HOME");
    } else {
        //si se introduce se establece como variable
        dir = line->commands[0].argv[1];
    }
    //comprobará si da error y sino se ejecutará
    errorInt = chdir(dir);
    RevisarErrorCd(dir, errorInt);
}

void ExecuteJOBS() {
    /*Jobs como comando solo muestra los comandos en bg actualmente*/
    MostrarPila();
}

void ExecuteFG() {
    pid_t pid;
    CambiarSenalesForeground();
    //si la pila esta vacia es que no hay comandos ejecutandose en bg
    if (jobs->head == NULL) {
        printf("No hay comandos en bg\n");
    } else {
        //Si no se introduce el pid deseado se pasa el ultimo añadido
        if (line->commands[0].argv[1] == NULL) {
            pid = jobs->head->pid; 
        } else {
            //Si no se toma de pantalla y se convierte a int
            pid = atoi(line->commands[0].argv[1]);
        }
        //se esperará al hijo y luego se eliminara su pid de la pila
        waitpid(pid, NULL, 0);
        EliminarPID(pid);
    }
}

void Execute() {
    int **arrayPipes;
    int *arrayPIDs;
    int contador;
    pid_t pid;
    //se creará un array de pids y de pipes
    arrayPIDs = malloc(sizeof(pid_t) * line->ncommands);

    arrayPipes = CrearArrayPipes();
    //se ejecutará tantas veces como argumentos hayan sido introducidos
    for (contador = 0; contador < line->ncommands; contador++) {
        //se creará un hijo por cada comando, se verá si hay error y se almacenará su pid
        pid = fork();
        RevisarErrorFork(pid);
        arrayPIDs[contador] = pid;
        //se comprobara que su pid es correcto, se gestionarán las redirecciones y los pipes y al final se ejecutará el comando con sus argumentos
        if (pid == 0) {
            RevisarErrorMandato(contador);
            AjustarSenalesProcesoHijo();
            GestionarRedireccionesEntradaFichero(contador);
            GestionarRedireccionesSalidaFichero(contador);
            GestionarRedireccionesErrorFichero(contador);
            GestionarPipesIO(arrayPipes, contador);
            execvp(line->commands[contador].filename, line->commands[contador].argv);
            exit(0);
        }
    }
    // se esperará a que los hijos acaben y luego se eliminaran los arrays de pipes y pids creados al principio
    EsperarHijos(arrayPIDs);
    DestruirArrayPipes(arrayPipes);
    free(arrayPIDs);
}

/*---------------------Rutina principal----------------*/
int main(void) {
    char buffer[1024];
    pid_t pid;
    jobs = InicializarPila();
    //Se ignoran las señales de teclado de terminación
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    //Cuando un hijo manda una señal de que ha terminado se ejecuta
    signal(SIGCHLD, LimpiarJobs);
    printf("msh1> ");
    //se lee de pantalla la línea introducida y luego se convierte en tokens
    while (fgets(buffer, 1024, stdin)) {
        line = tokenize(buffer);
        //dependiendo de cual sea el primer argumento se ejecutaran diferentes funciones
        if (strcmp(line->commands[0].argv[0], "cd") == 0) {
            ExecuteCD();
        } else if (strcmp(line->commands[0].argv[0], "jobs") == 0) {
            ExecuteJOBS();
        } else if (strcmp(line->commands[0].argv[0], "exit") == 0) {
            break;
        } else if (strcmp(line->commands[0].argv[0], "fg") == 0) {
            ExecuteFG();
        } else {
            // si no es ninguno de los comandos anteriores se comprobara si esta en bg o en fg
            if (line->background == 0) {
                Execute();
            } else {
                pid = fork();
                if (pid == 0) {
                    Execute();
                    exit(0);
                } else {
                    //la espera se produce cuando un hijo manda la señal SIGCHLD
                    AnnadirNodoALaPila(buffer, pid);
                }
            }
        }
        printf("msh> ");
    }
    destruirPila();
    exit(0);
}
