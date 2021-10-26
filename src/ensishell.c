/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "variante.h"
#include "readcmd.h"

#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>

/*#pragma clang diagnostic push*/
#ifndef VARIANTE
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

struct List_Tache {
	pid_t  pid;
    char * cmd;
    //char bg;
	//int wait; //0 si il n'est pas en wait 
	//int numero;
    struct List_Tache *next;
    struct timeval debut;
};

struct List_Tache *list_current = NULL ;
struct List_Tache *list_root =NULL;

/*
void add_list(struct List_Tache *l, pid_t pid, char* cmd, unsigned char bg, int wait){
    l->next = malloc(sizeof (struct List_Tache));
    l->next->next=NULL;
    l->next-> pid = pid;
    l->next->bg = bg;
    l->next->cmd = cmd;
	l->next->numero = l->numero +1;
	l->next->wait=wait;
    gettimeofday(&(l->next->debut), NULL);

    l= l->next;
}*/



struct List_Tache* new_job(char * cmd, pid_t pid, struct List_Tache * head){
	struct List_Tache* new= malloc(sizeof(struct List_Tache));
	new->pid=pid;
	new->cmd= malloc(sizeof(char)*strlen(cmd +1));
	strcpy(new->cmd, cmd);
	new->next = head;
	struct timeval debut;
	gettimeofday(&debut, NULL);
	new->debut=debut;

	return new;
}


void display_list(void){
	int statut;
	struct List_Tache *job = list_root;
	while (job->next!=NULL){
		statut=waitpid(job->pid, NULL, WNOHANG);
		if (!statut){
			printf("Processus %i is processing : commande %s", job->pid, job->cmd);
		}
	}
}


void read_and_execute(struct cmdline *cmd){

    if (cmd->err != NULL){
        printf("error: %s \n", cmd->err);
    }

	else if(cmd->seq[0]!=NULL){

		if(!strcmp(cmd->seq[0][0], "jobs")){
			display_list();
		}
		else{
			int pipes[2];
			pid_t pid[2];
			if (cmd->seq[1]!=NULL){// on a un pipe |
				pipe(pipes);
			}
			int n=1;
			if (cmd->seq[1]!=NULL){n=2;}
			int in;
			int out;
			int error;

			for (int i=0; i<n; i++){
				pid[i]=fork();
				if(pid[i]<0){printf("error fork");break;}
				else if (pid[i]==0){
					if(cmd->seq[1]!=NULL){ //on a bien un |
						dup2(pipes[abs(i-1)], abs(i-1));
						close(pipes[i]);
						close(pipes[abs(i-1)]);

						
						if (i==0 && cmd->in){
							in=open(cmd->in, O_RDONLY);
							dup2(in, 0);
							close(in);

						}
						if(i==1 && cmd->out){
							out = open(cmd->out, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
							dup2(out, 1);
							close(out);
						}
						if(i==1 && cmd->err){
							error= open(cmd->err, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
							dup2(error, 2);
							close(error);
						}
					}
					else{
						if (cmd->in){
							in=open(cmd->in, O_RDONLY);
							dup2(in, 0);
							close(in);

						}
						if(cmd->out){
							out = open(cmd->out, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
							dup2(out, 1);
							close(out);
						}
						if(cmd->err){
							error= open(cmd->err, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP);
							dup2(error, 2);
							close(error);
						}
					}
					execvp(cmd->seq[i][0], cmd->seq[i]);
				}
				
			}
		
			for(int i=0; i<n; i++){
				if (!cmd->bg){ //si pas en background les suivants atendent
					waitpid(pid[i], NULL, 0);
				}
				else{ //processus en background
					list_current = new_job( cmd->seq[i][0], pid[i], list_current);
				}
				if (i==0 && cmd->seq[1] !=NULL){
					close(pipes[1]);
					close(pipes[0]);
				}
			}
		}
	}

}




#if USE_GUILE == 1
#include <libguile.h>

int question6_executer(char *line)
{
	/* Question 6: Insert your code to execute the command line
	 * identically to the standard execution scheme:
	 * parsecmd, then fork+execvp, for a single command.
	 * pipe and i/o redirection are not required.
	 */
	read_and_execute(parsecmd(&line));

	/* Remove this line when using parsecmd as it will free it */
	free(line);
	
	return 0;
}

SCM executer_wrapper(SCM x)
{
        return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif


void terminate(char *line) {
#if USE_GNU_READLINE == 1
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
	  free(line);
	printf("exit\n");
	exit(0);
}


int main() {
        printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#if USE_GUILE == 1
        scm_init_guile();
        /* register "executer" function in scheme */
        scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif


	while (1) {
		struct cmdline *l;
		char *line=0;
		int i, j;
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		   can not be cleaned at the end of the program. Thus
		   one memory leak per command seems unavoidable yet */
		line = readline(prompt);
		if (line == 0 || ! strncmp(line,"exit", 4)) {
			terminate(line);
		}

#if USE_GNU_READLINE == 1
		add_history(line);
#endif


#if USE_GUILE == 1
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
                        continue;
                }
#endif

		/* parsecmd free line and set it up to 0 */
		l = parsecmd( & line);

		/* If input stream closed, normal termination */
		if (!l) {
		  
			terminate(0);
		}
		

		
		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		if (l->in) printf("in: %s\n", l->in);
		if (l->out) printf("out: %s\n", l->out);
		if (l->bg) printf("background (&)\n");

		/* Display each command of the pipe */
		for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];
			printf("seq[%d]: ", i);
                        for (j=0; cmd[j]!=0; j++) {
                                printf("'%s' ", cmd[j]);
                        }
			printf("\n");
		}
		read_and_execute(l);
	}

}
