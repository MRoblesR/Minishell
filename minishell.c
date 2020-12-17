#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include "parser.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>


tline *line;

void
xcd(void){
	char *home;
	int e;
	
	if(line->commands[0].argv[1]==NULL){
		home = getenv("HOME");
		e=chdir(home);
		if (e<0){
			fprintf(stderr,"error al cambiar de directorio\n");
	}}else{
		e=chdir(line->commands[0].argv[1]);
		if (e<0){
			fprintf(stderr, "error %s\n",line->commands[0].argv[1]);
		}
	
	}
}

void
closePipes(int **p){
	int i;
	for(i=0;i<line->ncommands-1;i++){
		close(p[i][0]);
		close(p[i][1]);
	}
	free(p);
}

void
x(void){
	int i;
	pid_t pid;
	int **p;
	if (line->ncommands>1){
		p=malloc(sizeof(int*)*line->ncommands-1);
		for(i=0;i<line->ncommands-1;i++){
			p[i]=malloc(sizeof(int)*2);
			pipe(p[i]);
		}
	}

	for(i=0;i<line->ncommands;i++){
		pid=fork();
		if (pid<0){
			fprintf(stderr, "error al crear el hijo\n");
			exit(1);	
		}
		if (pid==0){
			if (line->commands[i].filename==NULL){
				fprintf(stderr,"%s:No se encuentra el mandato\n", line->commands[i].argv[0]);
				exit(1);
			}
			if (line->ncommands>1){
				
				if(i==0){
					dup2(p[0][1],1);
				}else if(i==(line->ncommands-1)){
					dup2(p[i-1][0],0);
				}else{
					dup2(p[i][1],1);
					dup2(p[i-1][0],0);
				}
				closePipes(p);
			}
			execvp(line->commands[i].filename,line->commands[i].argv);
			exit(1);
		}
	}
	if(line->ncommands>1){
		closePipes(p);
	}
	for(i=0;i<line->ncommands;i++){
		wait(NULL);
	}
}

int
main(void){
	char buf[1024];

	printf("msh> ");	
	while (fgets(buf, 1024, stdin)) {
		
		line = tokenize(buf);
		if (line==NULL) {
			continue;
		}
		if(strcmp(line->commands[0].argv[0],"cd")==0){
			xcd();
		}else{
			x();
		}
		printf("==> ");
	}
	return 0;
}
