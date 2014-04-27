// UCLA CS 111 Lab 1 command execution

#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#include <error.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h> // waitpid()
#include <stdio.h>
#include <stdlib.h> // exit()
#include <fcntl.h> // open()
#include <string.h>

/* FIXME: You may need to add #include directives, macro definitions,
 static function definitions, etc.  */

#define DEBUG 0

int sequenceCommandsDependent(command_t cmd_1, command_t cmd_2);

/* DATA STUCTURES */

// Command queue for BFS
typedef struct command_queue_item{
    command_t command;
    struct command_queue_item * next;
} command_queue_item_t;

typedef struct command_queue{
    command_queue_item_t *head;
    command_queue_item_t *tail;
} command_queue_t;

// Add to command queue
void
addToCommandQueue(command_queue_t * c_queue, command_t command) {
    if (c_queue->head == NULL) {
        command_queue_item_t * new_item = (command_queue_item_t *)checked_malloc(sizeof(struct command_queue_item));
        new_item->command = command;
        new_item->next = NULL;
        c_queue->head = new_item;
        c_queue->tail = new_item;
    }
    else {
        command_queue_item_t * new_c_queue = (command_queue_item_t *)checked_malloc(sizeof(command_queue_item_t));
        new_c_queue->command = command;
        new_c_queue->next = NULL;
        c_queue->tail->next = new_c_queue;
        c_queue->tail = new_c_queue;
    }
}

// Get from command queue
command_t getFromCommandQueue(command_queue_t * c_queue) {
    command_t next_command = c_queue->head->command;
    command_queue_item_t * prev_head = c_queue->head;
    c_queue->head = c_queue->head->next;
    free(prev_head);
    return next_command;
}

int commandQueueIsEmpty(command_queue_t * c_queue) {
    return (c_queue->head == NULL) ? 1 : 0;
}

/* END of DATA STRUCTURES */

int
command_status (command_t c)
{
    return c->status;
}

void
execute_command (command_t c, int time_travel)
{
    if (DEBUG) printf("In execute_command...\n");
    
    pid_t child;
    int status;     // Execution result of child process
    int fd[2];      // File descriptors for pipe
    
    // TODO: when to use 0/1 vs STDIN_FILENO/STDOUT_FILENO
    
    // Create a framework to handle different command types
    switch (c->type) {
            
            /* Use kernel to execute command and get results
             * Fork new child process */
        case SIMPLE_COMMAND:
            child = fork();
            // Child branch
            if (child == 0) {
                // Handle redirects
                int fd_in;
                int fd_out;
                
                if (c->input) {
                    if ((fd_in = open(c->input, O_RDONLY, 0666)) == 01)
                        error (1, 0, "Cannot open input file.");
                    if (dup2(fd_in, 0) == -1)
                        error(1, 0, "Cannot execute input redirect.");
                }
                if (c->output) {
                    if ((fd_out = open(c->output, O_WRONLY|O_CREAT|O_TRUNC, 0666)) == 01)
                        error (1, 0, "Cannot open output file.");
                    if (dup2(fd_out, 1) == -1)
                        error(1, 0, "Cannot execute output redirect.");
                }
                
                // Handle execution
                execvp(c->u.word[0], c->u.word);
                error(1, 0, "Cannot execute command.");
            }
            // Parent branch
            else if (child > 0) {
                // Wait for child to finish
                waitpid(child, &status, 0);
                // Reap execution status
                c->status = status;
            }
            // Fork failed
            else {
                error(1, 0, "Cannot create child process.");
            }
            break;
            
            /* Recursively call execute_command
             * cmd1 && cmd2
             * run left child command first and on success, run right child command */
        case AND_COMMAND:
            
            execute_command(c->u.command[0], time_travel);
            if (c->u.command[0]->status == 0) {
                execute_command(c->u.command[1], time_travel);
                c->status = c->u.command[1]->status;
            }
            else {
                // Set status of AND command
                c->status = c->u.command[0]->status;
            }
            
            break;
            
            /* Recursively call execute_command
             * cmd1 || cmd2
             * run left child command first and on failure, run right child command */
        case OR_COMMAND:
            execute_command(c->u.command[0], time_travel);
            if (c->u.command[0]->status == 0) {
                // Set status of || command
                c->status = c->u.command[0]->status;
            }
            else {
                execute_command(c->u.command[1], time_travel);
                c->status = c->u.command[1]->status;
            }
            break;
            
            /* Redirect output of cmd1 to input of cmd2
             * cmd1 | cmd2
             * fd[0] for read and fd[1] for write */
        case PIPE_COMMAND:
            // Create pipe
            if (pipe(fd) == -1)
                error(1, 0, "Cannot create pipe.");
            
            child = fork();
            
            // Child branch
            // Child (cmd1) writes to pipe
            if (child == 0) {
                close(fd[0]);   // Close the read-end of pipe, otherwise, it will continue to occupy fd
                if (dup2(fd[1], 1) == -1)
                    error(1, 0, "Cannot redirect output.");
                execute_command(c->u.command[0], time_travel);
                close(fd[1]);   // Close write-end of pipe
                exit(c->u.command[0]->status);
            }
            // Parent branch
            // Parent (cmd2) reads from pipe
            else if (child > 0) {
                waitpid(child, &status, 0);
                c->u.command[0]->status = status;
                close(fd[1]);   // Close the write-end of pipe
                if (dup2(fd[0], 0) == -1)
                    error(1, 0, "Cannot redirect input.");
                execute_command(c->u.command[1], time_travel);
                close(fd[0]);   // Close the read-end of pipe
                c->status = c->u.command[1]->status;
            }
            else {
                error(1, 0, "Cannot create child process.");
            }
            break;
            
            /* Recursively call execute_command
             * (cmd1)
             * run subshell_command */
        case SUBSHELL_COMMAND:
            // Set time travel is 0 when encountering subshell command
            execute_command(c->u.subshell_command, 0);
            c->status = c->u.subshell_command->status;
            break;
            
            // TODO: implement sequence command
            /* Recursively call execute_command
             * cmd1 ; cmd2 or cmd1 ;
             */
        case SEQUENCE_COMMAND:
            /* Parallelize sequence command when possible
             * Check for depedency between cmd1 and cmd2
             * cmd2 has no input/output that is output of cmd1
             * &&
             * cmd1 has no input/output that is output of cmd2
             */
            
            // If there are 2 commands and no dependency, then run in parallel
            if (time_travel && c->u.command[1] && ((sequenceCommandsDependent(c->u.command[0], c->u.command[1])) == 0)) {
                if (DEBUG) printf("Running sequence commands in parallel..\n");
                
                // TODO: pass status back to parent process
                pid_t sequence_child = fork();
                if (sequence_child == 0) {
                    execute_command(c->u.command[0], time_travel);
                    c->status = c->u.command[0]->status;
                }
                else if (sequence_child > 0) {
                    execute_command(c->u.command[1], time_travel);
                    waitpid(sequence_child, &status, 0);
                }
                else {
                    error(1, 0, "Cannot create child process.");
                }
                c->status = c->u.command[1]->status;
            }
            // Else, run in serial (as implemented in 1b)
            else {
                if (DEBUG) printf("Running sequence commands in serial...\n");
                execute_command(c->u.command[0], time_travel);
                if (c->u.command[1]) {
                    execute_command(c->u.command[1], time_travel);
                    c->status = c->u.command[1]->status;
                }
                else {
                    c->status = c->u.command[0]->status;
                }
            }
            break;
    }
}

int sequenceCommandsDependent(command_t cmd_1, command_t cmd_2) {
    if (DEBUG) printf("In sequenceCommandsDependent...");
    int dependent = 0;
    
    // Generate inputs/outputs list for both commands
    char ** cmd_1_inputs = (char **)checked_malloc(sizeof(char *)*100);
    char ** cmd_1_outputs = (char **)checked_malloc(sizeof(char *)*100);
    
    // Initialize queue
    command_queue_t * c_queue_1 = (command_queue_t *)checked_malloc(sizeof(struct command_queue));
    c_queue_1->head = NULL;
    c_queue_1->tail = NULL;
    
    int input_pos_1 = 0;
    int output_pos_1 = 0;
    command_t command;
    
    // Add initial command to queue
    if (DEBUG) printf("*** Add initial command to queue\n");
    addToCommandQueue(c_queue_1, cmd_1);
    
    while (!commandQueueIsEmpty(c_queue_1)) {
        if (DEBUG) printf("Looking through c_queue...\n");
        command = getFromCommandQueue(c_queue_1);
        switch (command->type) {
            case AND_COMMAND:
                // Add commands to queue
                if (DEBUG) printf("AND case\n");
                addToCommandQueue(c_queue_1, command->u.command[0]);
                addToCommandQueue(c_queue_1, command->u.command[1]);
                break;
            case SEQUENCE_COMMAND:
                // Add commands to queue
                if (DEBUG) printf("SEQUENCE case\n");
                addToCommandQueue(c_queue_1, command->u.command[0]);
                if (command->u.command[1])
                    addToCommandQueue(c_queue_1, command->u.command[1]);
                break;
            case OR_COMMAND:
                // Add commands to queue
                addToCommandQueue(c_queue_1, command->u.command[0]);
                addToCommandQueue(c_queue_1, command->u.command[1]);
                break;
            case PIPE_COMMAND:
                // Add commands to queue
                addToCommandQueue(c_queue_1, command->u.command[0]);
                addToCommandQueue(c_queue_1, command->u.command[1]);
                break;
            case SIMPLE_COMMAND:
                if (DEBUG) printf("SIMPLE case\n");
                // Check for I/O redirection
                if (command->input) {
                    cmd_1_inputs[input_pos_1] = command->input;
                    // Increment input_pos
                    input_pos_1++;
                }
                if (command->output) {
                    cmd_1_outputs[output_pos_1] = command->output;
                    // Increment output_pos
                    output_pos_1++;
                }
                
                int word_count = 1;
                // Check for words
                while (command->u.word[word_count]) {
                    // If first char isn't a '-'
                    if (command->u.word[word_count][0] != '-') {
                        cmd_1_inputs[input_pos_1] = command->u.word[word_count];
                        input_pos_1++;
                    }
                    word_count++;
                }
                break;
            case SUBSHELL_COMMAND:
                // Add command to queue
                addToCommandQueue(c_queue_1, command->u.subshell_command);
                break;
            default:
                // TODO: could combine AND, OR, PIPE code
                break;
        }
    }
    cmd_1_inputs[input_pos_1] = NULL;
    cmd_1_outputs[output_pos_1] = NULL;
    
    if (DEBUG) printf("Finished chechking command 1. Checking command 2...");
    
    char ** cmd_2_inputs = (char **)checked_malloc(sizeof(char *)*100);
    char ** cmd_2_outputs = (char **)checked_malloc(sizeof(char *)*100);
    
    // Initialize queue
    command_queue_t * c_queue_2 = (command_queue_t *)checked_malloc(sizeof(struct command_queue));
    c_queue_2->head = NULL;
    c_queue_2->tail = NULL;
    
    int input_pos_2 = 0;
    int output_pos_2 = 0;
    
    // Add initial command to queue
    if (DEBUG) printf("Add initial command to queue\n");
    addToCommandQueue(c_queue_2, cmd_2);
    
    while (!commandQueueIsEmpty(c_queue_2)) {
        if (DEBUG) printf("Looking through c_queue...\n");
        command = getFromCommandQueue(c_queue_2);
        switch (command->type) {
            case AND_COMMAND:
                // Add commands to queue
                if (DEBUG) printf("AND case\n");
                addToCommandQueue(c_queue_2, command->u.command[0]);
                addToCommandQueue(c_queue_2, command->u.command[1]);
                break;
            case SEQUENCE_COMMAND:
                // Add commands to queue
                if (DEBUG) printf("SEQUENCE case\n");
                addToCommandQueue(c_queue_2, command->u.command[0]);
                if (command->u.command[1])
                    addToCommandQueue(c_queue_2, command->u.command[1]);
                break;
            case OR_COMMAND:
                // Add commands to queue
                addToCommandQueue(c_queue_2, command->u.command[0]);
                addToCommandQueue(c_queue_2, command->u.command[1]);
                break;
            case PIPE_COMMAND:
                // Add commands to queue
                addToCommandQueue(c_queue_2, command->u.command[0]);
                addToCommandQueue(c_queue_2, command->u.command[1]);
                break;
            case SIMPLE_COMMAND:
                if (DEBUG) printf("SIMPLE case\n");
                // Check for I/O redirection
                if (command->input) {
                    cmd_2_inputs[input_pos_2] = command->input;
                    // Increment input_pos
                    input_pos_2++;
                }
                if (command->output) {
                    cmd_2_outputs[output_pos_2] = command->output;
                    // Increment output_pos
                    output_pos_2++;
                }
                
                int word_count = 1;
                // Check for words
                while (command->u.word[word_count]) {
                    // If first char isn't a '-'
                    if (command->u.word[word_count][0] != '-') {
                        cmd_2_inputs[input_pos_2] = command->u.word[word_count];
                        input_pos_2++;
                    }
                    word_count++;
                }
                break;
            case SUBSHELL_COMMAND:
                // Add command to queue
                addToCommandQueue(c_queue_2, command->u.subshell_command);
                break;
            default:
                // TODO: could combine AND, OR, PIPE code
                break;
        }
    }
    cmd_2_inputs[input_pos_2] = NULL;
    cmd_2_outputs[output_pos_2] = NULL;
    
    if (DEBUG) {
        int i;
        printf("input_pos_1:%d\n", input_pos_1);
        printf("output_pos_1:%d\n", output_pos_1);
        printf("input_pos_2:%d\n", input_pos_2);
        printf("output_pos_2:%d\n", output_pos_2);
        
        printf("- Command 1 inputs -\n");
        for (i = 0; i < input_pos_1; i++) {
            printf("%d:%s\n", i, cmd_1_inputs[i]);
        }
        printf("- Command 1 outputs -\n");
        for (i = 0; i < output_pos_1; i++) {
            printf("%d:%s\n", i, cmd_1_outputs[i]);
        }
        printf("- Command 2 inputs -\n");
        for (i = 0; i < input_pos_2; i++) {
            printf("%d:%s\n", i, cmd_2_inputs[i]);
        }
        printf("- Command 2 outputs -\n");
        for (i = 0; i < output_pos_2; i++) {
            printf("%d:%s\n", i, cmd_2_outputs[i]);
        }
    }
    
    int x = 0;
    int y = 0;
    // Check outputs of cmd1 against inputs and outputs of cmd2
    for (x = 0; x < output_pos_1; x++) {
        for (y = 0; y < output_pos_2; y++) {
            if ((strcmp(cmd_1_outputs[x], cmd_2_outputs[y])) == 0) {
                dependent = 1;
                break;
            }
        }
        for (y = 0; y < input_pos_2; y++) {
            if ((strcmp(cmd_1_outputs[x], cmd_2_inputs[y])) == 0) {
                dependent = 1;
                break;
            }
        }
    }
    // Check inputs of cmd1 against outputs of cmd2
    for (x = 0; x < input_pos_1; x++) {
        for (y = 0; y < output_pos_2; y++) {
            if ((strcmp(cmd_1_inputs[x], cmd_2_outputs[y])) == 0) {
                dependent = 1;
                break;
            }
        }
    }
    
    free(cmd_1_inputs);
    free(cmd_1_outputs);
    free(cmd_2_inputs);
    free(cmd_2_outputs);
    
    if (DEBUG) printf("RETURNING!!!\n");
    
    return dependent;
}

// Used to build dependency graph
typedef struct command_dependency {
    command_t command;
    char ** inputs;
    char ** outputs;
    struct command_dependency * childs;
} command_dependency_t;

typedef struct command_dependency_list {
    command_dependency_t *commands[100];
} command_dependency_list_t;

void addToDependencyList(command_dependency_list_t * d_list, int pos, command_dependency_t * c_dependency) {
    d_list->commands[pos] = c_dependency;
}


void
execute_command_stream (command_stream_t c_stream, int time_travel)
{
    if (DEBUG) printf("In execute_command_stream (time_travel:%d)..\n", time_travel);
    
    // For each command in command_stream, gather list of inputs and list of outputs
    command_t command;
    
    command_dependency_list_t * c_d_list = (command_dependency_list_t *)checked_malloc(sizeof(struct command_dependency_list));
    int c_d_list_pos = 0;
    
    while ((command = read_command_stream(c_stream)))
    {
        // Traverse single command, BFS
        // Input is command argument without '-' or '--' or input redirection
        // Output only occurs in output redirecion
        command_t c_head = command;
        
        // Initialize queue
        command_queue_t * c_queue = (command_queue_t *)checked_malloc(sizeof(struct command_queue));
        c_queue->head = NULL;
        c_queue->tail = NULL;
        
        // Initialize input and output list
        char **inputs = (char **)checked_malloc(sizeof(char *)*100);
        char **outputs = (char **)checked_malloc(sizeof(char *)*100);
        
        int input_pos = 0;
        int output_pos = 0;
        
        // Add initial command to queue
        if (DEBUG) printf("Add initial command to queue\n");
        addToCommandQueue(c_queue, command);
        
        while (!commandQueueIsEmpty(c_queue)) {
            if (DEBUG) printf("Looking through c_queue...\n");
            command = getFromCommandQueue(c_queue);
            switch (command->type) {
                case AND_COMMAND:
                    // Add commands to queue
                    if (DEBUG) printf("AND case\n");
                    addToCommandQueue(c_queue, command->u.command[0]);
                    addToCommandQueue(c_queue, command->u.command[1]);
                    break;
                case SEQUENCE_COMMAND:
                    // Add commands to queue
                    if (DEBUG) printf("SEQUENCE case\n");
                    addToCommandQueue(c_queue, command->u.command[0]);
                    if (command->u.command[1])
                        addToCommandQueue(c_queue, command->u.command[1]);
                    break;
                case OR_COMMAND:
                    // Add commands to queue
                    addToCommandQueue(c_queue, command->u.command[0]);
                    addToCommandQueue(c_queue, command->u.command[1]);
                    break;
                case PIPE_COMMAND:
                    // Add commands to queue
                    addToCommandQueue(c_queue, command->u.command[0]);
                    addToCommandQueue(c_queue, command->u.command[1]);
                    break;
                case SIMPLE_COMMAND:
                    if (DEBUG) printf("SIMPLE case\n");
                    // Check for I/O redirection
                    if (command->input) {
                        inputs[input_pos] = command->input;
                        // Increment input_pos
                        input_pos++;
                    }
                    if (command->output) {
                        outputs[output_pos] = command->output;
                        // Increment output_pos
                        output_pos++;
                    }
                    
                    int word_count = 1;
                    // Check for words
                    while (command->u.word[word_count]) {
                        // If first char isn't a '-'
                        if (command->u.word[word_count][0] != '-') {
                            inputs[input_pos] = command->u.word[word_count];
                            input_pos++;
                        }
                        word_count++;
                    }
                    break;
                case SUBSHELL_COMMAND:
                    // Add command to queue
                    addToCommandQueue(c_queue, command->u.subshell_command);
                    break;
                default:
                    // TODO: could combine AND, OR, PIPE code
                    break;
            }
        }
        inputs[input_pos] = NULL;
        outputs[output_pos] = NULL;
        
        if (DEBUG) printf("Num of inputs:%d  Num of outputs:%d\n", input_pos, output_pos);
        
        // Partially fill out dependency struct for command, inputs, outputs
        command_dependency_t * c_dependency = (command_dependency_t *)checked_malloc(sizeof(struct command_dependency));
        c_dependency->command = c_head;
        c_dependency->inputs = inputs;
        c_dependency->outputs = outputs;
        
        // Add dependency struct to list (in order to keep track of it since they are not linked yet)
        addToDependencyList(c_d_list, c_d_list_pos, c_dependency);
        c_d_list_pos++;
        
        free(c_queue);
    }
    c_d_list->commands[c_d_list_pos] = NULL;
    
    if (DEBUG) {
        int i = 0; int j = 0;
        printf("Print out of c_d_list:\n");
        for (i; c_d_list->commands[i] != NULL ; i++) {
            printf(" i:%d\n" ,i);
            print_command(c_d_list->commands[i]->command);
            
            int j;
            printf("  -INPUTS-\n");
            for (j = 0; c_d_list->commands[i]->inputs[j] != NULL; j++) {
                printf("  %d %s\n", j, c_d_list->commands[i]->inputs[j]);
            }
            printf("  -OUTPUTS-\n");
            for (j = 0; c_d_list->commands[i]->inputs[j] != NULL; j++) {
                printf("  %d %s\n", j, c_d_list->commands[i]->outputs[j]);
            }
        }
    }
    
    // Compare inputs and ouputs and build dependency
    if (DEBUG) printf("Comparing inputs and outputs (brute force..-_-)...\n");
    
    // Use array of [c_d_list_pos]x[c_d_list_pos]
    int d_matrix[c_d_list_pos][c_d_list_pos];
    int x; int y; int z;
    for (x = 0; x < c_d_list_pos; x++) {
        for (y = 0; y < c_d_list_pos; y++) {
            d_matrix[x][y] = 0;
        }
    }
    
    for (x = 0; x < c_d_list_pos; x++) {
        for (y = x + 1; y < c_d_list_pos; y++) {
            // If input or output of c_d_list[y] is an output of c_d_list[x]
            // then y is dependent on x
            
            int output_word_pos;
            int input_word_pos;
            int output_word_pos_2;
            
            for (output_word_pos = 0; c_d_list->commands[x]->outputs[output_word_pos] != NULL; output_word_pos++) {
                int dependent = 0;
                for (input_word_pos = 0; c_d_list->commands[y]->inputs[input_word_pos] != NULL; input_word_pos++) {
                    if ((strcmp(c_d_list->commands[x]->outputs[output_word_pos], c_d_list->commands[y]->inputs[input_word_pos])) == 0) {
                        if (DEBUG) printf("Input of Y matches output of X");
                        d_matrix[y][x] = 1;
                        dependent = 1;
                        break;
                    }
                }
                if (dependent) break;
                for (output_word_pos_2 = 0; c_d_list->commands[y]->outputs[output_word_pos_2] != NULL; output_word_pos_2++) {
                    if ((strcmp(c_d_list->commands[x]->outputs[output_word_pos], c_d_list->commands[y]->outputs[output_word_pos_2])) == 0) {
                        if (DEBUG) printf("Output of Y matches output of X");
                        d_matrix[y][x] = 1;
                        break;
                    }
                }
            }
            for (input_word_pos = 0; c_d_list->commands[x]->inputs[input_word_pos] != NULL; input_word_pos++) {
                int dependent = 0;
                for (output_word_pos_2 = 0; c_d_list->commands[y]->outputs[output_word_pos_2] != NULL; output_word_pos_2++) {
                    if ((strcmp(c_d_list->commands[x]->inputs[input_word_pos], c_d_list->commands[y]->outputs[output_word_pos_2])) == 0) {
                        if (DEBUG) printf("Output of Y matches input of X");
                        d_matrix[y][x] = 1;
                        break;
                    }
                }
            }
        }
    }
    
    if (DEBUG) {
        printf("Printing out dependency matrix...\n");
        int i; int j;
        for (i = 0; i < c_d_list_pos; i++) {
            for (j = 0; j < c_d_list_pos; j++) {
                printf("%d ", d_matrix[i][j]);
            }
            printf("\n");
        }
    }
    
    // Generate W vector (sum dependencies for each command)
    int w_vector[c_d_list_pos];
    for (x = 0; x < c_d_list_pos; x++) {
        int sum = 0;
        for (y = 0; y < c_d_list_pos; y++) {
            sum += d_matrix[x][y];
        }
        w_vector[x] = sum;
    }
    
    if (DEBUG) {
        printf("Printing out W vector...\n");
        int i;
        for (i = 0; i < c_d_list_pos; i++)
            printf("%d ", w_vector[i]);
        printf("\n");
    }
    
    // Fork and execute each command that has W = 0
    pid_t child;
    pid_t child_pids[c_d_list_pos];
    int last_forked[c_d_list_pos];
    for (x = 0; x < c_d_list_pos; x++) {
        child_pids[x] = -2;
        last_forked[x] = 0;
    }
    int commandsLeft = 1;
    
    // While there are still commands left to be executed
    while (commandsLeft) {
        // Fork and execute commands with W = 0
        for (x = 0; x < c_d_list_pos; x++) {
            if (w_vector[x] == 0) {
                child = child_pids[x] = fork();
                if (child == 0) {
                    if (DEBUG) printf ("In child process..\n");
                    execute_command(c_d_list->commands[x]->command, time_travel);
                    commandsLeft = 0;
                    break;
                }
                else if (child > 0) {
                    // If parent waits, then cannot be spawn more than 1 child??
                    // Parent keeps track of executing/done tasks
                    last_forked[x] = 1;
                }
                else {
                    error(1, 0, "Cannot create child process.");
                }
            }
        }
        // Parent process
        if (child != 0 && child != -1) {
            if (DEBUG) printf("PARENT: my PID is %d\n", getpid());
            for (x = 0; x < c_d_list_pos; x++) {
                if (last_forked[x] == 1) {
                    int status;
                    if (DEBUG) printf("Waiting for child PID %d\n", child_pids[x]);
                    waitpid(child_pids[x], &status, 0);
                    if (DEBUG) printf("Exit status %d = %d\n", x, WEXITSTATUS(status));
                    
                    // Check dependency and decrement W vector
                    for (y = 0; y < c_d_list_pos; y++) {
                        if (d_matrix[y][x] == 1) {
                            w_vector[y] = w_vector[y] - 1;
                        }
                    }
                    w_vector[x] = -1;
                    last_forked[x] = -1;
                }
            }
            if (DEBUG) {
                printf("Printing out W vector...\n");
                int i;
                for (i = 0; i < c_d_list_pos; i++)
                    printf("%d ", w_vector[i]);
                printf("\n");
            }
            
            commandsLeft = 0;
            for (x = 0; x < c_d_list_pos; x++) {
                if (w_vector[x] >= 0) {
                    if (DEBUG) printf(" Still commands left to be executed\n");
                    commandsLeft = 1;
                    break;
                }
            }
        }
    }
    
    for (x = 0; x < c_d_list_pos; x++) {
        command_dependency_t * current = c_d_list->commands[x];
        free(current);
    }
    free(c_d_list);
}