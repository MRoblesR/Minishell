#include "parser.h" //todo hay que cambiar esto
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>


tline *line; //todo podriamos poner esto como no global
/*----------------------------------Estructuras de datos------------------------*/
struct nodo {
    struct nodo *sig;
    tline *contenido;
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
    newNodo->pid = pid;
    return newNodo;
}

void MostrarLinea(Nodo *pNodo) {
    //todo corregir el formato
    int i;
    printf(">>[%i]    ", pNodo->pid);
    if (pNodo->contenido->redirect_input != NULL) {
        printf("%s > ", pNodo->contenido->redirect_input);
    }
    for (i = 0; i < pNodo->contenido->ncommands; i++) {
        printf("%s ", pNodo->contenido->commands[i].filename);
        printf("%p ", pNodo->contenido->commands[i].argv);
    }
    if (pNodo->contenido->redirect_output != NULL) {
        printf("1>%s  ", pNodo->contenido->redirect_output);
    }
    if (pNodo->contenido->redirect_error != NULL) {
        printf("2> %s ", pNodo->contenido->redirect_error);
    }
    printf("\n");
}

void destruirNodo(Nodo *pNodo) {
    if (pNodo == NULL) {
        free(pNodo);
    } else {
        kill(SIGKILL, pNodo->pid);
        free(pNodo->contenido->commands);//Se elimina el array
        free(pNodo->contenido);//Se elimina el struct line
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

    printf("  PID     MANDATO");
    while (cursor != NULL) {
        MostrarLinea(cursor);
        cursor = getSigNodo(cursor);
    }
}

void EliminarCursor(Nodo *cursor, Nodo *ant) {
    if (cursor == jobs->head) {

    } else {
        ant->sig = cursor->sig;
        destruirNodo(cursor);
    }
}

void EliminarProcesoCabeza() {
    Nodo * cursor=jobs->head;
    jobs->head = jobs->head->sig;
    destruirNodo(cursor);
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
        fprintf(stderr, "error al abrir el fichero %s\n", line->redirect_input);
        exit(3);
    }
}

void RevisarErrorCreacionFicheroStdout(int fileDescriptor) {
    if (fileDescriptor < 0) {
        fprintf(stderr, "error al crear el fichero %s\n", line->redirect_output);
        exit(4);
    }
}

void RevisarErrorCreacionFicheroStderr(int fileDescriptor) {
    if (fileDescriptor < 0) {
        fprintf(stderr, "error al crear el fichero%s\n", line->redirect_error);
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

    if (line->ncommands == 1) {//todo revisar esto
        return NULL;//Si solo hay un comando no hace falta crear pipes
    } else {
        arrayPipes = (int **) malloc(sizeof(int *) * line->ncommands - 1);
        for (contador = 0; contador < line->ncommands - 1; contador++) {
            arrayPipes[contador] = (int *) malloc(sizeof(int) * 2);//Se crean dos int que actuarán como pipe
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
            fileDescriptor = open(line->redirect_input, O_RDONLY);// todo lo mismo que creat
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
            fileDescriptor = creat(line->redirect_output, 0644);//todo replantear lo del creat
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
void EsperarHijos(int *arrayPIDs) {
    int contador;
    for (contador = 0; contador < line->ncommands; contador++) {
        waitpid(arrayPIDs[contador], NULL, 0);
    }
}

int HijoHaTerminado(int status) {
    if (WIFEXITED(status)) {
        return 1;
    }
    if (WIFSIGNALED(status)) {
        return 1;
    }
    return 0;

}

void CambiarSenalesForeground() {
    signal(SIGINT,SIG_DFL);
    signal(SIGQUIT,SIG_DFL);
}

void CambiarSenalesBackground() {
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
}

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
    int *status;

    while (cursor != NULL) {
        status = NULL;
        waitpid(cursor->pid, status, WNOHANG); //todo no sé si esto funciona así
        if (HijoHaTerminado(*status)) {
            EliminarCursor(cursor, ant);
            cursor = ant->sig;
        }
    }
}

/*---------------------Gestión de comandos----------------*/
void ExecuteCD(void) {
    char *dir;
    int errorInt;

    if (line->commands[0].argv[1] == NULL) {
        dir = getenv("HOME");
    } else {
        dir = line->commands[0].argv[1];
    }

    errorInt = chdir(dir);
    RevisarErrorCd(dir, errorInt);
}

void ExecuteJOBS() {
    /*Jobs como comando solo muestra los comandos en bg actualmente*/
    MostrarPila();
}


void ExecuteFG() {
    pid_t pid;
    signal(SIGINT, EliminarProcesoCabeza);
    signal(SIGQUIT, EliminarProcesoCabeza);
    if (line->commands[0].argv[1] == NULL) {
        pid = jobs->head->pid; //Si no se introduce el pid deseado se pasa el ultimo añadido
    } else {
        pid = atoi(line->commands[0].argv[1]);
    }
    waitpid(pid, NULL, 0);
    EliminarProcesoCabeza();
}

void Execute() {
    int **arrayPipes;
    int *arrayPIDs;
    int contador;
    pid_t pid;

    arrayPIDs = malloc(sizeof(pid_t) * line->ncommands);

    arrayPipes = CrearArrayPipes();

    for (contador = 0; contador < line->ncommands; contador++) {
        pid = fork();
        RevisarErrorFork(pid);
        arrayPIDs[contador] = pid;

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
    DestruirArrayPipes(arrayPipes);
    EsperarHijos(arrayPIDs);
    free(arrayPIDs);
}


/*---------------------Rutina principal----------------*/
int main(void) {
    char buffer[1024];
    pid_t pid;
    jobs = InicializarPila();

    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);//Se ignoran las señales de teclado de terminación
    signal(SIGCHLD, LimpiarJobs);//Cuando un hijo manda una señal de que ha terminado se ejecuta el
    printf("msh> ");
    while (fgets(buffer, 1024, stdin)) {
        line = tokenize(buffer);
        if (strcmp(line->commands[0].argv[0], "cd") == 0) {
            ExecuteCD();
        } else if (strcmp(line->commands[0].argv[0], "jobs") == 0) {
            ExecuteJOBS();
        } else if (strcmp(line->commands[0].argv[0], "exit") == 0) {
            break;
        } else if (strcmp(line->commands[0].argv[0], "fg") == 0) {
            ExecuteFG();
        } else {
            if (line->background == 0) {
                Execute();
            } else {
                pid = fork();
                if (pid == 0) {
                    Execute();
                } else {
                    AnnadirNodoALaPila(buffer, pid);// La espera se produce cuando un hijo manda la señal SIGCHLD
                }
            }
        }
        printf("msh> ");
    }
    destruirPila();
    exit(0);
}
