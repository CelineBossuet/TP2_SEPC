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
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>


#include <libguile.h>

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

struct rlimit lim;
int set_limite =0;




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

struct List_Tache * remove_job(struct List_Tache * head){
    struct List_Tache *new=head->next;
    free(head->cmd);
    free(head);
    return new;
}

void display_list(void){
	int statut;
	struct List_Tache *job = list_current;
	if (job==NULL || waitpid(job->pid, NULL, WNOHANG)){
		printf("pas de processus en cours \n");
	}
	else{
		while (job!=NULL){
			statut=waitpid(job->pid, NULL, WNOHANG);
			
			if (!statut){
				printf("Processus %i is processing : commande %s \n", job->pid, job->cmd);
			}
			job=job->next;
		}
	}
	

}

void process_time_calcul (int sig, siginfo_t * siginfo, void * ctx){
	struct List_Tache * job=list_current ;
	while ( job!=NULL){
		if (job->pid == siginfo->si_pid){
			struct timeval temps;
			gettimeofday(&temps, NULL);
			float delta = (temps.tv_sec - job->debut.tv_sec + (temps.tv_usec- job->debut.tv_usec)*1E-6);
			printf("\n Processus %i fini en %f secondes \n", job->pid, delta);
			readline("ensishell>");
			
		}
		job=job->next;
	}
	
}


void read_and_execute(struct cmdline *cmd){
	
    if (cmd->err != NULL){
        printf("error: %s \n", cmd->err);
    }
	else if(cmd->seq[0]!=NULL){

		if(strcmp(cmd->seq[0][0], "jobs")==0){
			display_list();
		}
		else if(strcmp(cmd->seq[0][0], "ulimit")==0){
			int limite;
			if (cmd->seq[0][1]!=NULL){
				limite=atoi(cmd->seq[0][1]);
				set_limite=1; //nous avons une limite
				lim.rlim_cur=limite;
				lim.rlim_max=limite+5;
				printf("nouvelle limite de temps de %d secondes\n", limite);
			}
			else{
				printf("pas de limite donnée, rééssayez\n");
			}

		}
		else{
			int pipes[2];
			pid_t pid[2];
			if (cmd->seq[1]!=NULL){// on a un pipe |
				pipe(pipes);
				if (pipe(pipes)==-1){
					perror("pipe error");
					exit(EXIT_FAILURE);
				}
			}
			int n=1;
			if (cmd->seq[1]!=NULL){n=2;}
			int in;
			int out;
			int error;

			for (int i=0; i<n; i++){
				pid[i]=fork();
				if(pid[i]<0){perror("error fork"); exit(EXIT_FAILURE);}

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
							ftruncate(out, 0);
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
							ftruncate(out,0);
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
				if (i==0 && cmd->seq[1] !=NULL){
					close(pipes[1]);
					close(pipes[0]);
				}
				if (set_limite>0){
						printf("ahhh \n");
						setrlimit(RLIMIT_CPU, &lim);
					}
				if (!cmd->bg){ //si pas en background les suivants attendent
					waitpid(pid[i], NULL, 0);
				}
				else{ //processus en background

					list_current = new_job( cmd->seq[i][0], pid[i], list_current);
				}
			}
		}
	}

}




#if USE_GUILE == 1


int question6_executer(char *line)
{
	/* Question 6: Insert your code to execute the command line
	 * identically to the standard execution scheme:
	 * parsecmd, then fork+execvp, for a single command.
	 * pipe and i/o redirection are not required.
	 */
	read_and_execute(parsecmd(&line));

	
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
	
	struct sigaction signal;
	signal.sa_sigaction = process_time_calcul;
	signal.sa_flags = SA_SIGINFO | SA_NOCLDWAIT;
    sigset_t *mask = malloc(sizeof(sigset_t));
    sigemptyset(mask);
    signal.sa_mask = *mask;
    sigaction(SIGCHLD, &signal, NULL);
	free(mask);


	lim.rlim_cur=0;


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
	for (struct List_Tache * job=list_current; job!=NULL; ){
		job=remove_job(job);
	}

}
