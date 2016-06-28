/* main.c - The foo shell

  Copyright (c) 2015, Giovanni Robira, giovanni.robira@usp.br> 
		      Julia Minetto, julia.macias@usp.br>	
		      Luiz Augusto, luiz.manoel@usp.br> 	
		      Gabriel Luiz F. Souto, gabriel.souto@usp.br>

   This file is part of Shellpson.

   Shellzao is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/


#include <stdlib.h>
#include <stdio.h>
#include <foosh.h>
#include <debug.h>
#include <unistd.h>
#include <signal.h> /* Para usar os sinais*/
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h> /* open() */
#include <fcntl.h> /* open() */
#include <termios.h>
#include <signal.h>


/* Variaveis globais */
int go_on = 1;      /* This variable controls the main loop. */
pid_t shell_pgid;
struct termios shell_tmodes;
int shell_terminal;
int shell_is_interactive;
list_t* jobs_list;
list_t* process_list;
pipeline_t* current_job;

/*----------------COMANDOS BUILT IN-----------------------*/
/* Command cd */
int cd (char *path)
{
    char cwd[300];
    if(path == NULL) /* If there is no argument the directory is changed to home */
    {
        if(!chdir("/home"))
            return 1;
        else
            return 0;
    }
    else
    {
        getcwd(cwd, sizeof(cwd));
        strcat(cwd, "/");
        strcat(cwd, path);

        if(!chdir(cwd))
            return 0; /* Success! The working directory is changed */
        else
        {
            printf("Directory not found!\n");
            return 1; /* Fail! The working directory was not changed */
        }
            
    }
}

/* Função que verifica se é comando interno */
int exec_builtin(char** command)
{
    if(!strcmp(command[0], "cd"))
    {
        cd(command[1]);
        /* printf("\nRodou\n"); */
        return 0;
    }
    else if(!strcmp(command[0], "exit"))
    {
        if(DEBUG) printf ("\nExit shell\n");
        go_on = 0;
        return 0;
    }
    else 
    {
        if(DEBUG) printf ("\nNao eh builtin\n");
        return 1;
    }
}

/* TODO: Essa função precisa ser implementada, por enquanto não faz nada */
void delete_node(void* n)
{
    printf("Freeing node\n");
}

/* Essa função coloca todos os processos em uma lista */

void create_process_list(pipeline_t* p)
{
    process_t* j; /* job */
    /* iterators */
    int i; 
    int k;
    list_node_t* node;

    j = (process_t*) malloc (sizeof(process_t));
    
    
    for (i = 0; i < p->ncommands; i++)
    {
        if(DEBUG) printf("Number of arguments %d\n", p->narguments[i]);

        /* Alocando espaço para o comando e seus argumentos */
        j->argv = (char**) malloc (p->narguments[i]*sizeof(char*));

        for (k = 0; k < p->narguments[i]; k++)
        {
            j->argv[i] = (char*) malloc (ARG_MAX_SIZE*sizeof(char));
        }

        j->argv = p->command[i];
        if(DEBUG) printf("Command %s\n", j->argv[i]);

        j->pid = -1;
        if(DEBUG) printf("PID: %d\n", j->pid);

        j->narguments = p->narguments[i];
        if(DEBUG) printf("N Arguments: %d\n", p->narguments[i]);

        j->status = 0;
        if(DEBUG) printf("Status: %d\n\n", j->status);

        node = append_node(process_list);
        if(DEBUG) printf("No recebido\n");
        node->value = (process_t*) j;
        if(DEBUG) printf("Processo colocado na lista\n");
        if(p->first_process == NULL)
            p->first_process = j;
    }
}

/* Adiciona um job `a lista */

void add_job(pipeline_t* p)
{
    list_node_t* node;

    node = append_node(jobs_list);
    node->value = (pipeline_t*) p;
}

/* Handlers dos sinais */
/* TODO: Fazer com que os sinais sejam reconhecidos de verdade */
/* TODO: Implementar os handlers, por enquanto nao fazem nada */

void handleCtrlC(int first)
{
    printf("SIGINT! PID: %d\n", getpid());  
}

void handleCtrlZ(int first)
{
    printf("SIGTSTP!\n");
}

void handleSigChld(int first)
{
    printf("Filho %d terminou.\n\n", getpid());
}

void init_shell(sigset_t* chldMask)
{
    /* Inicializadno variaveis */
    current_job = NULL;
    jobs_list = NULL;
    process_list = NULL;

    /* Vendo se estamos rodando interativamente  */
    shell_terminal = STDIN_FILENO;
    shell_is_interactive = isatty (shell_terminal);

    /* Cria as listas de jobs e de processos */
    process_list = new_list(delete_node);
    jobs_list = new_list(delete_node);

    /* Ignore interactive and job-control signals.  */
      signal (SIGINT, handleCtrlC);
      signal (SIGQUIT, SIG_IGN);
      signal (SIGTSTP, handleCtrlZ);
      signal (SIGTTIN, SIG_IGN);
      signal (SIGTTOU, SIG_IGN);
      signal (SIGCHLD, handleSigChld);
}

int main (int argc, char **argv)
{
    /* Declarações de variáveis */
    buffer_t *command_line;
    int i, aux;
    /*char cwd[300];*/
    char* username;
    int fIn, fOut; /* Guardarão os fds de entrada e saída padrão */
    int pid;
    pipeline_t *pipeline; /* Estrutura que armazenara um job */
    sigset_t chldMask;

    /* Inicializando o shell */
    init_shell(&chldMask);

    command_line = new_command_line ();

    pipeline = new_pipeline ();

    /* See if we are running interactively.  */
    shell_terminal = STDIN_FILENO;
    shell_is_interactive = isatty (shell_terminal);

    if (shell_is_interactive)
    {
        /* Loop until we are in the foreground.  */
        while (tcgetpgrp (shell_terminal) != (shell_pgid = getpgrp ()))
        kill (- shell_pgid, SIGTTIN);


        /* Put ourselves in our own process group.  */
        shell_pgid = getpid ();
        if (setpgid (shell_pgid, shell_pgid) < 0)
        {
            perror ("Couldn't put the shell in its own process group\n");
            exit (1);
        }

        /* Grab control of the terminal.  */
        tcsetpgrp (shell_terminal, shell_pgid);

        /* Save default terminal attributes for shell.  */
        tcgetattr (shell_terminal, &shell_tmodes);
    }
    
    username = getlogin(); /* Pega o nome de usuario */
    
    /* Loop principal */
    while (go_on)
    {
        /* Pega o diretorio do usuario */
        /* getcwd(cwd, sizeof(cwd)); */
        /* Imprime o prompt */
        /*printf ("%s %s ", cwd, PROMPT);*/
        printf("%s $ ", username);
        fflush (stdout);

        /* Lê a linha de comando. Arruma o negocio zuado dos sinais */
        do
            aux = read_command_line (command_line);
        while ((aux < 0) && (errno == EINTR));

        fatal (aux < 0, NULL);

        parse_command_line (command_line, pipeline);
        
        create_process_list(pipeline);
        add_job(pipeline);

        /* Guarda a entrada e saída padrão */
        /* Por enquanto a saida e entrada do job e a padrao */
        fIn = dup(1); /* Entrada padrão */
        fOut = dup(0); /* Saída padrão */
        pipeline->fIn = fIn;
        pipeline->fOut = fOut;

        /* Verifica se precisa redirecionar a ENTRADA */
        if(pipeline->file_in[0] != '\0')
        {
            /* Fecha a entrada padrão */
            close(0);
            /* Abre um novo file descriptor, ele pega o primeiro espaço vazio, 
             * que no caso será o espaço da ENTRADA padrão */
            open(pipeline->file_in, O_RDONLY, S_IRUSR | S_IWUSR);
        }

        /* Verifica se precisa redirecionar a SAIDA */
        if(pipeline->file_out[0] != '\0')
        {
            /* Fecha a saída padrão */
            close(1);
            /* Abre um novo file descriptor, ele pega o primeiro espaço vazio, 
             * que no caso será o espaço da SAÍDA padrão */
            open(pipeline->file_out, O_CREAT| O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR);
        }

        
        /* Tentará executar todos os comandos */
        for (i = 0; pipeline->command[i][0]; i++)
        {
            /* Tenta executar um comando built-in */
            if(exec_builtin(pipeline->command[i]))
            {
                int status;
                pid = fork();
                /* Processo filho */
                if(pid == 0)
                {
                    if(DEBUG) printf("Child process\n");

                    /* Executa o comando */
                    aux = execvp(pipeline->command[i][0], pipeline->command[i]);
                    fatal(aux < 0, "Could not find program\n");
                }
                /* Processo pai */
                else if (pid > 0)
                {
                    wait (&status);
                }
            }
        }

        /* Restaura a entrada e saida padrão */
        close(1); /* Fecha a saida */
        dup(fOut); /* Cria outro fd com o STDIN */
        close(0); /* Fecha a entrada */
        dup(fIn); /* Cria outro fd com o STDOUT */
        
    }

    release_command_line (command_line);
    release_pipeline (pipeline);

    return EXIT_SUCCESS;
}
