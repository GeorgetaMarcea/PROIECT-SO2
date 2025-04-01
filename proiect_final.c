#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <fcntl.h>

#define MAX 4096
#define CMD_L 100
#define PIPE_L 2
#define ARGC 10

// getuid -> get user identity
// char *getcwd(char buf[.size], size_t size) -> get current working directory

void userdir(){
    uid_t uid = getuid();
    char director_curent[MAX];
    struct passwd *pw = getpwuid(uid);
    if(pw == NULL){
        perror("Eroare la determinarea utilizatorului");
        exit(1);
    }

    if (getcwd(director_curent, sizeof(director_curent)) == NULL) {
        perror("Eroare la determinarea directorului curent");
        exit(2);
    }

    printf("MY BASH -> %s:%s>> ", pw->pw_name, director_curent);
}

void citire_linie(char *linie){
    userdir();
    fgets(linie, MAX, stdin);
    linie[strcspn(linie, "\n")] = '\0';  //eliminam newline-ul de la sf
}

void comanda_ls(){
    DIR *dir;
    struct stat st;
    struct dirent *intrare;

    if((dir = opendir(".")) == NULL){
        perror("Eroare la deschiderea directorului curent");
        exit(2);
    }
    
    while((intrare = readdir(dir)) != NULL){
        if((strcmp(intrare->d_name, ".") == 0) || (strcmp(intrare->d_name, "..") == 0))
            continue; //ignoram directorul curent si parintele

        if(lstat(intrare->d_name, &st) == -1){
            perror("Eroare la lstat");
            exit(3);
        }

        if(S_ISDIR(st.st_mode)){
            printf("%s\t", intrare->d_name);
        }

        if(S_ISREG(st.st_mode)){
            printf("%s\t", intrare->d_name);
        }
    // target-ul unei leg sim -> calea reala a fisierului sau directorului la care face referire
        if(S_ISLNK(st.st_mode)){
            char target[MAX];
            if(readlink(intrare->d_name, target, sizeof(target)) == -1){
                perror("Eroare la target");
                exit(4);
            }

            printf("%s -> %s\t", intrare->d_name, target);

        }
    }
    closedir(dir);
    printf("\n");
}

// chdir() changes the current working directory of the calling process to the directory specified in path.
void comanda_cd(char *cale_director_nou){
    if((chdir(cale_director_nou)) == -1){
        perror("Eroare la schimbarea directorului");
        exit(5);
    }
}

void comanda_cat(char *fisier){
    int fd = open(fisier, O_RDONLY);
    if(fd == -1){
        perror("Eroare la deschiderea fisierului pentru cat");
        exit(6);
    }

    char x;
    while((read(fd, &x, 1)) > 0){
        printf("%c", x);
    }

    close(fd);
    printf("\n");
}

void executa_comanda(char *comanda){
    // spargem comanda in argumente
    // ex: dc avem ls -l -> argumente = {"ls", "-l"}
    // gcc -Wall -o p ex.c
    //printf("Șirul comanda înainte de strtok: '%s'\n", comanda);
    char *argumente[CMD_L];
    char copie_comanda[CMD_L];
    strncpy(copie_comanda, comanda, CMD_L);
    int i = 0;
    char *token = strtok(copie_comanda, " "); // argumente[0]=gcc

    while(token != NULL){
        argumente[i++] = token;
        token = strtok(NULL," ");
    }

    argumente[i] = NULL;

    /* v - execv(), execvp(), execvpe()
       The char *const argv[] argument is an array of pointers to null-terminated strings that represent the argument
       list available to the new program.  The first argument, by convention, should point to the filename associated
       with the file being executed.  The array of pointers must be terminated by a null pointer.*/

    // daca folosim execv trebuie sa specificam calea absoluta/relativa a executabilului
    // ex: daca avem ls -> trebuia /bin/ls
    // asa ca folosim execvp -> aceasta cauta executabilele in variabila PATH si nu mai trebuie sa specificam calea

    // for (int j = 0; argumente[j] != NULL; j++) {
    //     printf("argumente[%d]: %s\n", j, argumente[j]);
    // }

    //if(strcmp(argumente[0], "gcc") == 0){
        if((execvp(argumente[0], argumente)) == -1){
            perror("Eroare la executarea comenzii");
            exit(8);
        }
    //}
}


void comanda_pipe(char *comanda){
    //aflam cele doua comenzi 
    char *comenzi[PIPE_L];
    comenzi[0] = strtok(comanda, "|");
    comenzi[1] = strtok(NULL, "|");

    if(comenzi[1] == NULL){
        perror("Comamda respectiva nu contine pipe");
        exit(9);
    }

    int pfd[2]; // 0 -> citim, 1 -> scriem

    if(pipe(pfd) < 0){
        perror("Eroare la crearea pipe-ului");
        exit(10);
    }

    pid_t pid = fork();
    if(pid < 0){
        perror("Eroare la fork");
        exit(11);
    }

    if(pid == 0){
        //proces copil
        //acesta va executa comanda nr 1
        close(pfd[0]); //inchidem capatul de citire (copilul nu citeste, el scrie)
        //realizeaza duplicarea descriptorului oldfd și returneaza noul descriptor
        dup2(pfd[1], STDOUT_FILENO); //toate iesirile trimise care stdout sunt scrise in pipe
        close(pfd[1]);
        executa_comanda(comenzi[0]);
    }else{
        //proces parinte
        //acesta executa comanda nr 2
        close(pfd[1]); //inchidem capatul de scriere (parintele citeste)
        dup2(pfd[0], STDIN_FILENO); //toate citirile de la stdin vor fi redirectionate catre capatul de citire
        close(pfd[0]);
        waitpid(pid, NULL, 0); //asteptam ca procesul copil sa termine
        executa_comanda(comenzi[1]);
    }
}

void executa_binar(char *comanda) {
    pid_t pid = fork();
    if (pid < 0){
        perror("Eroare la fork");
        exit(12);
    }

    if (pid == 0){
        // proces copil
        executa_comanda(comanda);
    }else{
        // proces parinte
        waitpid(pid, NULL, 0);
    }
}

int main() {
    char comanda[MAX];
    char copie_comanda[MAX];

    while(1) {
        citire_linie(comanda);
        strncpy(copie_comanda, comanda, MAX);

        if(strchr(copie_comanda, '|') != NULL) {
            comanda_pipe(copie_comanda);
        }else{
            char *cuvant = strtok(copie_comanda, " ");

            if((strcmp(cuvant, "exit")) == 0) {
                break;
            }

            if((strcmp(cuvant, "ls")) == 0) {
                comanda_ls();

            }else if((strcmp(cuvant, "cd")) == 0) {
                char *dir_nou = strtok(NULL, " ");
                comanda_cd(dir_nou);

            }else if((strcmp(cuvant, "cat")) == 0) {
                char *fisier = strtok(NULL, " ");
                comanda_cat(fisier);

            }

            executa_binar(comanda);
        }
    }
    return 0;
}