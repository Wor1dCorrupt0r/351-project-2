//Jasper Hsu
//CPSC 351 
//project 2: unixshell.C 
//Professor McCarthy
//3/28/2021
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

//this #define is for the file descriptor(fd), its a pipe with read and write end 
#define READ_END 0 
#define WRITE_END 1

int find_pipe_rhs(char** cargs); // command line arguments(cargs) for finding the pipe
char** tokparse(char* input, char* cargs[]){
    const char d[2] = {' ', '\t'};  // delimiters d, for strtok()
    char* token;
    int num_args = 0;
    token = strtok(input, d);
    cargs[0] = token;

    char** REDIR = malloc(2 * sizeof(char*)); //REDIR for I/O redirection
    for(int a = 0; a < 2; ++a)
        REDIR[a] = malloc(BUFSIZ * sizeof(char));
        REDIR[0] = ""; 
        REDIR[1] = ""; 

    while(token != NULL){ // while token doesn't equal to null, adds token to cargs
        token = strtok(NULL, d); //breaks null strings into tokens with delimeters
        if(token == NULL) break; //if token is null, break/terminate this loop and go to the next one
        if(!strncmp(token, ">", 1)){
            token = strtok(NULL, d);
            REDIR[0] = "o"; 
            REDIR[1] = token;
            return REDIR; 
        } 
        else if(!strncmp(token, "<", 1)){
            token = strtok(NULL, d);
            REDIR[0] = "i";
            REDIR[1] = token;
            return REDIR;
        }
        else if(!strncmp(token, "|", 1)){ // found pipe
            REDIR[0] = "p";        
        }
        cargs[++num_args] = token;
    }
    return REDIR; 
}

int main(int argc, const char* argv[]){
    char input [BUFSIZ]; // input for buffer size 
    char MRU   [BUFSIZ]; // MRU cache for buffer size 
    int pipefd[2]; // pipe file descriptor

    //memory set for cleaning buffers
    memset(input, 0, BUFSIZ * sizeof(char)); 
    memset(MRU,   0, BUFSIZ * sizeof(char));

    while(1){    //while should run
        printf("osh> ");  
        fflush(stdout);   // flushes the output buffer of stdout
        fgets(input, BUFSIZ, stdin); // reads the strings/characters from stdin and stores into input
        input[strlen(input) - 1] = '\0'; // wipe out newline at end of string
        // printf("input was: \n'%s'\n", input);

        if(strncmp(input, "exit", 4) == 0) // only compare first 4 letters
            return 0;
        if(strncmp(input, "!!", 2)) // check for history (!!) command 
            strcpy(MRU, input);    

        bool wait_daemon = true;
        char* wait_offset = strstr(input, "&");
        if(wait_offset != NULL){*wait_offset = ' '; wait_daemon=false;}

        pid_t pid = fork();  
        if(pid < 0){   //  if pid is less than 0, then fork child process fails
            fprintf(stderr, "fork failed...\n");
            return -1; // exit 
        }
        if(pid != 0){  // parent process
            if(wait_daemon){
                wait(NULL);
                wait(NULL);
            }
        }
        else{    // child process
            //printf("child process start...\n");
            char* cargs[BUFSIZ];    
            memset(cargs, 0, BUFSIZ * sizeof(char));
            int history = 0;
             if(!strncmp(input, "!!", 2)) 
               history = 1; //use"!!"for reading from MRU cache
             char** redirect = tokparse( (history ? MRU : input), cargs);
             if(history && MRU[0] == '\0'){ //if history and MRU are 0, then no command enters
                printf("No recently used command.\n");
                exit(0);
            } 
            // redirecting i/o to file descriptor
            if(!strncmp(redirect[0], "o", 1)){ // redir
                printf("output saved to ./%s\n", redirect[1]);
                int fd = open(redirect[1], O_TRUNC | O_CREAT | O_RDWR);
                dup2(fd, STDOUT_FILENO); // copy or duplicates stdout to file descriptor 
            }
            else if(!strncmp(redirect[0], "i", 1)){ // input redir
                printf("reading from file: ./%s\n", redirect[1]);
                int fd = open(redirect[1], O_RDONLY); // reading process for file descriptor
                memset(input, 0, BUFSIZ * sizeof(char));
                read(fd, input,  BUFSIZ * sizeof(char));
                memset(cargs, 0, BUFSIZ * sizeof(char));
                input[strlen(input) - 1]  = '\0';
                tokparse(input, cargs);
            }
            else if(!strncmp(redirect[0], "p", 1)){// found pipe
                pid_t pidc; 
                int pipe_rhs_offset = find_pipe_rhs(cargs);
                cargs[pipe_rhs_offset] = "\0";
                int e = pipe(pipefd);
                if(e < 0){
                    fprintf(stderr, "pipe failed...\n");
                    return 1;
                }
                // set buffers for both lhs and rhs to 0 
                char* lhs[BUFSIZ], *rhs[BUFSIZ];
                memset(lhs, 0, BUFSIZ*sizeof(char));
                memset(rhs, 0, BUFSIZ*sizeof(char));

                for(int x = 0; x < pipe_rhs_offset; ++x){
                    lhs[x] = cargs[x];
                }
                for(int j = 0; j < BUFSIZ; ++j){
                    int i = j + pipe_rhs_offset + 1;
                    if(cargs[i] == 0) break;
                    rhs[j] = cargs[i];
                }
                
                pidc = fork(); //fork a child procss
                if(pidc < 0){
                    fprintf(stderr, "fork failed...\n");
                    return 1;
                }
                if(pidc != 0){   // parent process 
                    dup2(pipefd[WRITE_END], STDOUT_FILENO);// copy or duplicates stdout to WRITE_END of file descriptor
                    close(pipefd[WRITE_END]); // close WRITE_END of pipe
                    execvp(lhs[0], lhs); //execute command in parent
                    exit(0); 
                }
                else{   // child process
                    dup2(pipefd[READ_END], STDIN_FILENO);// copy or duplicate READ_END of pipe to STDIN_FILENO
                    close(pipefd[READ_END]);// close READ_END of pipe
                    execvp(rhs[0], rhs);// execute command in child
                    exit(0); 
                }
                wait(NULL);
            }
            execvp(cargs[0], cargs);
            exit(0);  
        }
    }
    return 0;
}
// Locating the pipe char and then return its index 
int find_pipe_rhs(char** cargs){
    int index = 0;
    while(cargs[index] != '\0'){
        if(!strncmp(cargs[index], "|", 1)){// found pipe
            return index;// new cargs starting at offset
        }
        ++index; //index increments 
    }
    return -1;
}
