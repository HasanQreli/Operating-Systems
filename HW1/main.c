#define _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <sys/wait.h>
#include <unistd.h>
#include "parser.h" 

void execute_pipeline(parsed_input parsedInput);
void execute_pipeline_subpipe(single_input input);

void execute_pipeline_subpipe_subshell(single_input input, int pipe_fds_ss[][2], int input_no, int num_pipes_ss){
    int num_pipes = input.data.pline.num_commands - 1;
    int pipe_fds[num_pipes][2];
    pid_t pid;

    for(int i=0; i<num_pipes; i++){
        if(pipe(pipe_fds[i]) < 0){
            perror("pipe error");
            return;
        }
    }

    for(int i=0; i<input.data.pline.num_commands; i++){
        pid = fork();
        if(pid < 0){
            perror("fork");
            return;
        } 
        else if(pid == 0){
            if(i==0){
                //printf("after call pipefds: %d\n", pipe_fds_ss[input_no][0]);
                dup2(pipe_fds_ss[input_no][0], STDIN_FILENO);
            }
            // Close all pipes from subshell
            for(int j=0; j<num_pipes_ss; j++){
                close(pipe_fds_ss[j][0]);
                close(pipe_fds_ss[j][1]);
            }
            if(i>0){
                dup2(pipe_fds[i-1][0], STDIN_FILENO);
            }
            if(i < input.data.pline.num_commands-1){
                dup2(pipe_fds[i][1], STDOUT_FILENO);
            }
            for(int j=0; j<num_pipes; j++){
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            
            // Execute the command
            execvp(input.data.pline.commands[i].args[0], input.data.pline.commands[i].args);
            // If execvp returns, there was an error
            perror("execvp");
            return;
        }
    }

    // Close all pipes in parent process
    for(int i=0; i<num_pipes; i++){
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }

    // dont wait here so that we can use it both in seq and parallel
}

void single_command(parsed_input parsedInput, int i){
    pid_t pid = fork();
    
    if(pid == 0){ // Child process
        execvp(parsedInput.inputs[i].data.cmd.args[0], parsedInput.inputs[i].data.cmd.args);
        perror("execvp");
        exit(42);
    }
}

void execute_subshell(char subshell[INPUT_BUFFER_SIZE]){
    pid_t pid = fork();
    if(pid<0){
        perror("fork");
        return;
    }
    else if(pid == 0){ // Child process for Subshell
        parsed_input parsedInput;
        parse_line(subshell, &parsedInput);

        if(parsedInput.num_inputs == 1 && parsedInput.separator == SEPARATOR_NONE && parsedInput.inputs[0].type == INPUT_TYPE_COMMAND){
            single_command(parsedInput, 0);
            wait(NULL);
        } 
        else if(parsedInput.separator == SEPARATOR_PIPE){ // If it's a pipeline
            //printf("Pipeline\n");
            execute_pipeline(parsedInput);
        }
        else if(parsedInput.separator == SEPARATOR_SEQ){
            //printf("Sequential with %d inputs\n", parsedInput.num_inputs);
            for(int i=0; i<parsedInput.num_inputs; i++){
                if(parsedInput.inputs[i].type == INPUT_TYPE_COMMAND){
                    single_command(parsedInput, i);
                    wait(NULL);
                }
                else if(parsedInput.inputs[i].type == INPUT_TYPE_PIPELINE){ 
                    execute_pipeline_subpipe(parsedInput.inputs[i]);
                    // Wait for all child processes to finish
                    for(int j = 0; j < parsedInput.inputs[i].data.pline.num_commands; j++){
                        wait(NULL);
                    }
                }
            }
        }
        else if(parsedInput.separator == SEPARATOR_PARA){
            //printf("Parallel with %d inputs\n", parsedInput.num_inputs);
            int num_processes = 0;
            int pipe_fds[parsedInput.num_inputs][2];
            for(int i=0; i<parsedInput.num_inputs; i++){
                if(pipe(pipe_fds[i]) < 0){
                    perror("pipe error");
                    return;
                }
            }
            for(int i=0; i<parsedInput.num_inputs; i++){
                if(parsedInput.inputs[i].type == INPUT_TYPE_COMMAND){
                    num_processes++;
                    pid_t pid = fork();
                    
                    if(pid == 0){ // Child process
                        dup2(pipe_fds[i][0], STDIN_FILENO);
                        for(int j = 0; j < parsedInput.num_inputs; j++){
                            close(pipe_fds[j][0]);
                            close(pipe_fds[j][1]);
                        }
                        execvp(parsedInput.inputs[i].data.cmd.args[0], parsedInput.inputs[i].data.cmd.args);
                        perror("execvp");
                        exit(42);
                    }
                }
                else if(parsedInput.inputs[i].type == INPUT_TYPE_PIPELINE){
                    execute_pipeline_subpipe_subshell(parsedInput.inputs[i], pipe_fds, i, parsedInput.num_inputs);
                    num_processes += parsedInput.inputs[i].data.pline.num_commands;
                }
            }
            
            // repeater code
            char buffer[1];

            // Read from stdin until EOF
            while(read(STDIN_FILENO, buffer, 1)){
                // Write the read data to every pipe
                for(int i = 0; i < parsedInput.num_inputs; i++){
                    write(pipe_fds[i][1], buffer, 1);
                }
            }

            // Close all pipes
            for(int i = 0; i < parsedInput.num_inputs; i++){
                close(pipe_fds[i][1]);
                close(pipe_fds[i][0]);
            }

            // wait for processes to finish
            
            for(int i=0; i<num_processes; i++){
                wait(NULL);
            }
        }
        else{
            printf("Invalid input\n");
        }
        free_parsed_input(&parsedInput);
        exit(0);
    }
    else{ // Parent process
        int status;
        waitpid(pid, &status, 0);
    }
}

void execute_subshell_forked(char subshell[INPUT_BUFFER_SIZE]){
    parsed_input parsedInput;
        parse_line(subshell, &parsedInput);

        if(parsedInput.num_inputs == 1 && parsedInput.separator == SEPARATOR_NONE && parsedInput.inputs[0].type == INPUT_TYPE_COMMAND){
            single_command(parsedInput, 0);
            wait(NULL);
        } 
        else if(parsedInput.separator == SEPARATOR_PIPE){ // If it's a pipeline
            //printf("Pipeline\n");
            execute_pipeline(parsedInput);
        }
        else if(parsedInput.separator == SEPARATOR_SEQ){
            //printf("Sequential with %d inputs\n", parsedInput.num_inputs);
            for(int i=0; i<parsedInput.num_inputs; i++){
                if(parsedInput.inputs[i].type == INPUT_TYPE_COMMAND){
                    single_command(parsedInput, i);
                    wait(NULL);
                }
                else if(parsedInput.inputs[i].type == INPUT_TYPE_PIPELINE){ 
                    execute_pipeline_subpipe(parsedInput.inputs[i]);
                    // Wait for all child processes to finish
                    for(int j = 0; j < parsedInput.inputs[i].data.pline.num_commands; j++){
                        wait(NULL);
                    }
                }
            }
        }
        else if(parsedInput.separator == SEPARATOR_PARA){
            //printf("Parallel with %d inputs\n", parsedInput.num_inputs);
            int num_processes = 0;
            int pipe_fds[parsedInput.num_inputs][2];
            for(int i=0; i<parsedInput.num_inputs; i++){
                if(pipe(pipe_fds[i]) < 0){
                    perror("pipe error");
                    return;
                }
            }
            for(int i=0; i<parsedInput.num_inputs; i++){
                if(parsedInput.inputs[i].type == INPUT_TYPE_COMMAND){
                    num_processes++;
                    pid_t pid = fork();
                    
                    if(pid == 0){ // Child process
                        dup2(pipe_fds[i][0], STDIN_FILENO);
                        for(int j = 0; j < parsedInput.num_inputs; j++){
                            close(pipe_fds[j][0]);
                            close(pipe_fds[j][1]);
                        }
                        execvp(parsedInput.inputs[i].data.cmd.args[0], parsedInput.inputs[i].data.cmd.args);
                        perror("execvp");
                        exit(42);
                    }
                }
                else if(parsedInput.inputs[i].type == INPUT_TYPE_PIPELINE){
                    //printf("Before call pipefds: %d\n", pipe_fds[i][0]);
                    execute_pipeline_subpipe_subshell(parsedInput.inputs[i], pipe_fds, i, parsedInput.num_inputs);
                    num_processes += parsedInput.inputs[i].data.pline.num_commands;
                }
            }
            
            // repeater code
            char buffer[1];

            // Read from stdin until EOF
            while(read(STDIN_FILENO, buffer, 1)){
                // Write the read data to every pipe
                for(int i = 0; i < parsedInput.num_inputs; i++){
                    write(pipe_fds[i][1], buffer, 1);
                }
            }


            // Close all pipes
            for(int i = 0; i < parsedInput.num_inputs; i++){
                close(pipe_fds[i][1]);
                close(pipe_fds[i][0]);
            }

            // wait for processes to finish
            
            for(int i=0; i<num_processes; i++){
                wait(NULL);
            }
        }
    free_parsed_input(&parsedInput);
    exit(0);
}

// !!!pipe icin exit kontrol etmiyorum!!!
void execute_pipeline(parsed_input parsedInput){
    int num_pipes = parsedInput.num_inputs - 1;
    int pipe_fds[num_pipes][2];
    pid_t pid;

    for(int i=0; i<num_pipes; i++){
        if(pipe(pipe_fds[i]) < 0){
            perror("pipe error");
            return;
        }
    }

    for(int i=0; i<parsedInput.num_inputs; i++){
        pid = fork();
        if(pid < 0){
            perror("fork");
            return;
        } 
        else if(pid == 0){
            if(i > 0){
                dup2(pipe_fds[i-1][0], STDIN_FILENO);
            }
            if(i < parsedInput.num_inputs-1){
                dup2(pipe_fds[i][1], STDOUT_FILENO);
            }
            for(int j = 0; j < num_pipes; j++){
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            if(parsedInput.inputs[i].type == INPUT_TYPE_COMMAND){
                execvp(parsedInput.inputs[i].data.cmd.args[0], parsedInput.inputs[i].data.cmd.args);
            }
            else if(parsedInput.inputs[i].type == INPUT_TYPE_SUBSHELL){
                execute_subshell_forked(parsedInput.inputs[i].data.subshell);
            }
            perror("execvp");
            return;
        }
    }

    for(int i=0; i<num_pipes; i++){
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }

    // Wait for all child processes to finish
    // didnt use waitpid since there are n children and we wait n times
    for(int i=0; i<parsedInput.num_inputs; i++){
        wait(NULL);
    }
}

void execute_pipeline_subpipe(single_input input){
    int num_pipes = input.data.pline.num_commands - 1;
    int pipe_fds[num_pipes][2];
    pid_t pid;

    for(int i=0; i<num_pipes; i++){
        if(pipe(pipe_fds[i]) < 0){
            perror("pipe error");
            return;
        }
    }

    for(int i=0; i<input.data.pline.num_commands; i++){
        pid = fork();
        if(pid < 0){
            perror("fork");
            return;
        } 
        else if(pid == 0){ 
            if(i > 0){
                dup2(pipe_fds[i-1][0], STDIN_FILENO);
            }
            if(i < input.data.pline.num_commands-1){
                dup2(pipe_fds[i][1], STDOUT_FILENO);
            }
            for(int j = 0; j < num_pipes; j++){
                close(pipe_fds[j][0]);
                close(pipe_fds[j][1]);
            }
            execvp(input.data.pline.commands[i].args[0], input.data.pline.commands[i].args);
            perror("execvp");
            return;
        }
    }

    for(int i=0; i<num_pipes; i++){
        close(pipe_fds[i][0]);
        close(pipe_fds[i][1]);
    }

    // dont wait here so that we can use it both in seq and parallel
}

int main(){
    while(true){
        printf("/> ");
        char *input = NULL;
        size_t len = 0;
        parsed_input parsedInput;

        // Read input from stdin
        if(getline(&input, &len, stdin) == -1){
            printf("Error reading input\n");
            free(input);
            return 1;
        }
        
        
        // Parse the input line
        if(!parse_line(input, &parsedInput)){
            printf("Error parsing input\n");
            free(input);
            return 1;
        }

        free(input);
        
        // If it's a single command, execute it
        if(parsedInput.num_inputs == 1 && parsedInput.separator == SEPARATOR_NONE && parsedInput.inputs[0].type == INPUT_TYPE_COMMAND){
            //printf("Single command , %s\n", parsedInput.inputs[0].data.cmd.args[0]);
            if(strcmp(parsedInput.inputs[0].data.cmd.args[0], "quit") == 0){
                exit(0);
            }
            single_command(parsedInput, 0);
            wait(NULL);
        } 
        else if(parsedInput.num_inputs == 1 && parsedInput.separator == SEPARATOR_NONE &&  parsedInput.inputs[0].type == INPUT_TYPE_SUBSHELL){
            //printf("Subshell\n");
            execute_subshell(parsedInput.inputs[0].data.subshell);
        }
        else if(parsedInput.separator == SEPARATOR_PIPE){ // If it's a pipeline
            //printf("Pipeline\n");
            execute_pipeline(parsedInput);
        }
        else if(parsedInput.separator == SEPARATOR_SEQ){
            //printf("Sequential with %d inputs\n", parsedInput.num_inputs);
            for(int i=0; i<parsedInput.num_inputs; i++){
                if(parsedInput.inputs[i].type == INPUT_TYPE_COMMAND){
                    single_command(parsedInput, i);
                    wait(NULL);
                }
                else if(parsedInput.inputs[i].type == INPUT_TYPE_PIPELINE){ 
                    execute_pipeline_subpipe(parsedInput.inputs[i]);
                    // Wait for all child processes to finish
                    for(int j = 0; j < parsedInput.inputs[i].data.pline.num_commands; j++){
                        wait(NULL);
                    }
                }
            }
        }
        else if(parsedInput.separator == SEPARATOR_PARA){
            //printf("Parallel with %d inputs\n", parsedInput.num_inputs);
            int num_processes = 0;
            for(int i=0; i<parsedInput.num_inputs; i++){
                if(parsedInput.inputs[i].type == INPUT_TYPE_COMMAND){
                    num_processes++;
                    single_command(parsedInput, i);
                }
                else if(parsedInput.inputs[i].type == INPUT_TYPE_PIPELINE){
                    execute_pipeline_subpipe(parsedInput.inputs[i]);
                    num_processes += parsedInput.inputs[i].data.pline.num_commands;
                }
            }
            for(int i=0; i<num_processes; i++){
                wait(NULL);
            }
        }
        
        else{
            printf("Invalid input\n");
        }


        free_parsed_input(&parsedInput);
    }
    
    return 0;
}
