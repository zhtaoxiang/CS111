// UCLA CS 111 Lab 1 command reading
// Stanley Ku 403576238
// Valerie Runge 004355520

#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#include <error.h>
#include <stdlib.h>
#include <stdio.h> // printf
#include <ctype.h> // isalnum
#include <string.h> // strchr, strncpy
#include <stdbool.h> // bool

#define DEBUG 0
#define bufSize 10000

/* FIXME: Define the type 'struct command_stream' here.  This should
 complete the incomplete type declaration in command.h.  */

struct command_stream_list {
    command_t command;
    struct command_stream_list *next;
};

struct command_stream
{
    struct command_stream_list *start;
	struct command_stream_list *end;
};

// Function that adds return from makeTree to command_stream_t
void addCommandToCommandStream(struct command_stream *cStream, command_t c)
{
    struct command_stream_list *newList = (struct command_stream_list *)checked_malloc(sizeof(struct command_stream_list));
    newList->command = c;
    
    // If list is empty
    if (cStream->start == NULL) {
		cStream->start=newList;
        cStream->end=newList;
		newList->next=0;
    }
    else {
        //if the list is not empty
		cStream->end->next=newList;
		cStream->end=newList;
		newList->next=0;
    }
}

// Function that removes item from command_stream_t
struct command * removeCommandFromCommandStream(struct command_stream *cStream)
{
	if(cStream->start) {
		struct command *command = cStream->start->command;
		struct command_stream_list *curHead = cStream->start;
        
		cStream->start=cStream->start->next;
        
		free(curHead);
		return command;
	}
	return 0;
}

// Token structure. Used to convert raw line text into high level tokens
struct token
{
	char *word;
	struct token *next;
};

struct token_list
{
	struct token *f_token;
	struct token *l_token;
};

void addTokenToTokenList(struct token_list *tl, struct token *t) {
    if (tl->f_token != NULL) {
        tl->l_token->next = t;
        tl->l_token = t;
    } else {
        tl->f_token = t;
        tl->l_token = t;
    }
}

// Node structure. Used as a intermediate between tokens and actual tree command structure
enum nodeType {
    LEFT_PAREN = 5,
    RIGHT_PAREN = 5,
    PIPE = 4,
    AND = 3,
    OR = 3,
    SEMI = 2,
    SIMPLE = 1,
};

struct node {
    enum nodeType type;
    bool andType;
    bool leftType;
    command_t simpleCommand;
    struct node *next;
};

struct node_list {
    struct node *f_node;
    struct node *l_node;
};

void addNodeToNodeList(struct node_list *nl, struct node *n) {
    if (nl->f_node != NULL) {
        nl->l_node->next = n;
        nl->l_node = n;
    } else {
        nl->f_node = n;
        nl->l_node = n;
    }
};

// Stack structure. Used for organizing nodes into priority tree
struct stack {
    struct command *command;
    struct stack *next;
};

// Pop function
struct command * popStack(struct stack **top)
{
    if (*top == NULL) {
        if (DEBUG) printf(" popStack returning NULL\n");
        return NULL;
    }
    struct command *command = (*top)->command;
    struct stack *temp = (*top);
    *top = (*top)->next;
    free(temp);
    return command;
    
}

// Push function
void pushStack(struct stack **top, struct command *value)
{
    if (DEBUG) printf(" Stack top value:%p\n", top);
    if (DEBUG) printf(" Pushing command_type:%d\n", value->type);
    
    struct stack *newStack = (struct stack *)checked_malloc(sizeof(struct stack));
    newStack->command = value;
    newStack->next = *top;
    *top = newStack;
}

bool stackIsEmpty(struct stack *top)
{
    return top == NULL;
}

/*                   *
 * Support functions *
 *                   */
// Returns a new line from stream
char * getLineFromScript(int (*get_next_byte) (void *),
                         void *get_next_byte_argument)
{
    size_t lineSize = 100;
    char *line = (char *)checked_malloc(sizeof(char *) * lineSize);
    
    int c = (*get_next_byte)(get_next_byte_argument);
    if (c < 0 || c == EOF) {
        return NULL;
    }
    
    size_t linePos = 0;
    while (1) {
        if (DEBUG) printf("Reading char:%c\n", (char)c);
        if (linePos == lineSize) {
            lineSize *= 2;
            line = (char *)checked_grow_alloc((void *)line, &lineSize);
        }
        if (c < 0 || ((char) c) == '\n') {
            line[linePos] = '\0';
            return line;
        }
        line[linePos] = ((char)c);
        linePos++;
        c = (*get_next_byte)(get_next_byte_argument);
    }
}

bool isSpecialToken(char c)
{
    return strchr(";|&()", c);
}

// Returns substring from startPos to stopPos
char * getSubstring(char *line, int startPos, int stopPos)
{
    size_t size = stopPos - startPos + 1;
    char *substring = (char *) checked_malloc(size + 1);
    strncpy(substring, line + startPos, size);
    substring[size] = '\0';
    return substring;
}

// Outputs error message with corresponding line number
void errorMsg(int lineNum)
{
    error(1, 0, "Syntax Error on line %d", lineNum);
}

// Checks if char is a valid character: alphanumeric and !%+,-./:@^_
bool isValidWordChar(char c)
{
    return ( isalnum(c) || strchr("!%+,-./:@^_<>", c));
}

bool isTokenBlank(char *c)
{
    int pos = 0;
    bool hasChar = false;
    while (c[pos] != '\0') {
        if (c[pos] != ' ') {
            hasChar = true;
        }
        pos++;
    }
    return hasChar;
}

struct command * makeTree(struct node_list *nl)
{
    if (DEBUG) printf("** IN MAKE TREE **\n");
    
    struct stack *operandStack = NULL;
    struct stack *operatorStack = NULL;
    if (DEBUG) printf("*Created stacks\n");
    
    if (!stackIsEmpty(operatorStack)) {
        printf("WRONG!\n");
    }
    
    unsigned int topOperator = 0;
    int parenDepth = 0;
    bool isLastCommandSubshell = false;
    struct node *currentNode = nl->f_node;
    
    while (currentNode != NULL) {
        // If simple command, add to operand stack
        if (currentNode->type == SIMPLE) {
            if (DEBUG) printf("In simple command logic\n");
            struct command *currentCommand = (struct command *)checked_malloc(sizeof(struct command));
            currentCommand->type = SIMPLE_COMMAND;
            currentCommand->status = -1;
            currentCommand->input = currentNode->simpleCommand->input;
            currentCommand->output = currentNode->simpleCommand->output;
            currentCommand->u.word = currentNode->simpleCommand->u.word;
            if (DEBUG) printf(" (Simple) command type:%d\n", currentCommand->type);
            pushStack(&operandStack, currentCommand);
            isLastCommandSubshell = false;
        }
        // Otherwise, add to operator stack
        else {
            //if (DEBUG) printf("In operator stack logic\n");
            
            if (currentNode->type == RIGHT_PAREN && !(currentNode->leftType)) {
                if (DEBUG) printf("  Right Paren\n");
                return NULL;
            }
            else if (currentNode->type == LEFT_PAREN && currentNode->leftType) {
                //if (DEBUG) printf("  Left Paren\n");
                if (isLastCommandSubshell) {
                    return NULL;
                }
                parenDepth++;
                
                // Create a new node list up to matching right parenthesis
                struct node *prevNode = currentNode;
                currentNode = currentNode->next;
                struct node *parenStartListPos = currentNode;
                
                while (currentNode != NULL) {
                    //if (DEBUG) printf("   Loop\n");
                    if (currentNode->type == LEFT_PAREN && currentNode->leftType) {
                        if (DEBUG) printf("    Found a Left Paren\n");
                        parenDepth++;
                    } else if (currentNode->type == RIGHT_PAREN) {
                        if (DEBUG) printf("    Found a Right Paren\n");
                        parenDepth--;
                    }
                    if (parenDepth == 0) {
                        // End the list at matching right parenthesis
                        prevNode->next = NULL;
                        break;
                    }
                    prevNode = prevNode->next;
                    currentNode = currentNode->next;
                }
                // If non-matching parenthesis
                if (parenDepth != 0) {
                    return NULL;
                }
                
                // Make subshell command
                struct node_list *parenNodeList = (struct node_list *)checked_malloc(sizeof(struct node_list));
                parenNodeList->f_node = parenStartListPos;
                parenNodeList->l_node = prevNode;
                
                struct command *subshellCommand = makeTree(parenNodeList);
                if (subshellCommand != NULL) {
                    struct command *subshellCommandParent = (struct command *)checked_malloc(sizeof(struct command));
                    subshellCommandParent->type = SUBSHELL_COMMAND;
                    subshellCommandParent->u.subshell_command = subshellCommand;
                    pushStack(&operandStack, subshellCommandParent);
                    if (DEBUG) printf("       Pushed subshell command\n");
                    
                }
                // Relink the list after subshell command is created
                isLastCommandSubshell = true;
                prevNode->next = currentNode;
            }
            // Same or lower priority
            else if (currentNode->type <= topOperator) {
                topOperator = currentNode->type;
                if (currentNode->type == PIPE || currentNode->type == AND || currentNode->type == OR || currentNode->type == SEMI) {
                    if (DEBUG) printf("  Pipe, And, Or\n");
                    struct command *operandCommand = popStack(&operatorStack);
                    if (DEBUG) printf("  popped operator\n");
                    
                    // Special case for sequence command (;) because it can have 1 subcommand or 2 subcommands
                    if (operandCommand->type == SEQUENCE_COMMAND) {
                        //operandCommand->u.command[0] = 0;
                        operandCommand->u.command[0] = popStack(&operandStack);
                        if (operandCommand->u.command[0] == NULL) {
                            return NULL;
                        }
                        if (!stackIsEmpty(operandStack)) {
                            operandCommand->u.command[1] = operandCommand->u.command[0];
                            operandCommand->u.command[0] = popStack(&operandStack);
                        } else {
                            operandCommand->u.command[1] = '\0';
                        }
                    } else {
                        operandCommand->u.command[1] = popStack(&operandStack);
                        operandCommand->u.command[0] = popStack(&operandStack);
                        if (operandCommand->u.command[0] == NULL || operandCommand->u.command[1] == NULL) {
                            if (DEBUG) {printf("**Checked null commands\n");}
                            return NULL;
                        }
                    }
                    
                    if (DEBUG) printf("  pushing operand\n");
                    pushStack(&operandStack, operandCommand);
                    if (DEBUG) printf("  pushed operand\n");
                    
                    struct command *operatorCommand = (struct command *)checked_malloc(sizeof(struct command));
                    operatorCommand->status = -1;
                    operatorCommand->input = 0; operatorCommand->output = 0;
                    if (currentNode->type == PIPE) {
                        operatorCommand->type = PIPE_COMMAND;
                    }
                    else if (currentNode->type == AND && currentNode->andType) {
                        operatorCommand->type = AND_COMMAND;
                    }
                    else if (currentNode->type == OR) {
                        operatorCommand->type = OR_COMMAND;
                    }
                    else if (currentNode->type == SEMI) {
                        operatorCommand->type = SEQUENCE_COMMAND;
                    }
                    pushStack(&operatorStack, operatorCommand);
                    if (DEBUG) printf("  pushed operator");
                }
                isLastCommandSubshell= false;
                // Higher priority
            } else {
                topOperator = currentNode->type;
                if (DEBUG) printf("  Higher Priority\n");
                struct command *operatorCommand = (struct command *)checked_malloc(sizeof(struct command));
                operatorCommand->status = -1;
                operatorCommand->input = 0; operatorCommand->output = 0;
                if (currentNode->type == PIPE) {
                    operatorCommand->type = PIPE_COMMAND;
                }
                else if (currentNode->type == AND && currentNode->andType) {
                    if (DEBUG) printf("AND!!!!\n");
                    operatorCommand->type = AND_COMMAND;
                }
                else if (currentNode->type == OR) {
                    if (DEBUG) printf("OR!!!!\n");
                    operatorCommand->type = OR_COMMAND;
                }
                else if (currentNode->type == SEMI) {
                    operatorCommand->type = SEQUENCE_COMMAND;
                }
                pushStack(&operatorStack, operatorCommand);
                isLastCommandSubshell= false;
            }
        }
        
        // Get next node
        currentNode = currentNode->next;
    }
    
    if (DEBUG) printf("Out of 1st while loop\n");
    
    // Process the rest of the operator
    while (!stackIsEmpty(operatorStack)) {
        // Pop operator
        struct command *tempCommand = popStack(&operatorStack);
        if (DEBUG) printf("*Popped operator\n");
        
        /* Sequence command special cases
         * if only 1 cmd, then format: cmd1;
         * if 2 cmd's, then cmd1=second pop ; cmd2=first pop
         */
        if (tempCommand->type == SEQUENCE_COMMAND) {
            tempCommand->u.command[0] = popStack(&operandStack);
            if (tempCommand->u.command[0] == NULL) {
                return NULL;
            }
            if (!stackIsEmpty(operandStack)) {
                tempCommand->u.command[1] = tempCommand->u.command[0];
                tempCommand->u.command[0] = popStack(&operandStack);
            } else {
                tempCommand->u.command[1] = '\0';
            }
        } else {
            tempCommand->u.command[1] = popStack(&operandStack);
            tempCommand->u.command[0] = popStack(&operandStack);
            if (tempCommand->u.command[0] == NULL || tempCommand->u.command[1] == NULL) {
                if (DEBUG) printf("**Checked null commands\n");
                return NULL;
            }
        }
        
        // Push to operandStack
        pushStack(&operandStack, tempCommand);
    }
    
    if (DEBUG) printf("Out of 2nd while loop\n");
    
    struct command *rootCommand = popStack(&operandStack);
    if (rootCommand == NULL) {
        return NULL;
    } else {
        return rootCommand;
    }
    
}

// TODO: need to pass in linNum for errorMsg. Could return NULL instead
// Converts tokens into either operators or simple commands
struct node_list * convertTokens(struct token_list *tl)
{
    if (DEBUG) printf("IN CONVERT TOKENS\n");
    // Create a node list
    struct node_list *nodeList = (struct node_list *)checked_malloc(sizeof(struct node_list));
    nodeList->f_node = NULL;
    nodeList->l_node = NULL;
    
    int tokenCount = 0;
    bool isLastTokenSimple = false;
    bool isInputEmpty = true;
	bool isLastTokenParen = false;
    
    while (tl->f_token) {
        // If token is an operator, create an operator
        struct node *n = (struct node *)checked_malloc(sizeof(struct node));
        n->next = NULL;
        n->andType = false; n->leftType = false;
        
        // Operator
        if (isSpecialToken(tl->f_token->word[0])) {
            if (DEBUG) printf("Current token is an operator\n");
            if (tokenCount == 0 && tl->f_token->word[0] != '(') {
                return NULL;
            }
            
            n->simpleCommand = NULL;
            switch (tl->f_token->word[0]) {
                case '(':
                    if (isLastTokenSimple) {
                        return NULL;
                    }
                    n->type = LEFT_PAREN;
                    n->leftType = true;
                    break;
                case ')':
                    n->type = RIGHT_PAREN;
					isLastTokenParen=true;
                    break;
                case '&':
                    n->type = AND;
                    n->andType = true;
                    break;
                case '|':
                    if (tl->f_token->word[1] == '|') {
                        n->type = OR;
                    }
                    else {
                        n->type = PIPE;
                    }
                    break;
                case ';':
                    if ((!isLastTokenSimple)&&(!isLastTokenParen)) {
                        return NULL;
                    }
                    n->type = SEMI;
                    break;
                default:
                    break;
            }
            isLastTokenSimple = false;
        }
        // Simple
        else {
            if (DEBUG) printf("Current token is a simple command\n");
            n->type = SIMPLE;
            isLastTokenSimple = true;
            
            // Create a simple command
            struct command *simpleCommand = (struct command *)checked_malloc(sizeof(struct command));
            simpleCommand->type = SIMPLE_COMMAND;
            simpleCommand->status = -1;
            simpleCommand->input = 0;
            simpleCommand->output = 0;
            simpleCommand->u.word = (char **)checked_malloc(sizeof(char *)*10);
            size_t commandWordSize = 10;
            
            // Parse the token into words and put it in command->u.word
            
            int wordCounter = 0;
            int tokenWordPos = 0;
            int newWordPos = 0;
            int inputPos = 0; int outputPos = 0;
            bool newWord = false;
            bool inputRedirect = false; bool outputRedirect = false;
            bool inputWord = false; bool outputWord= false;
            char *tokenWord = tl->f_token->word;
            
            while (1) {
                //if (DEBUG) printf("Token word char: %c\n", tokenWord[tokenWordPos]);
                // TODO: check if all syntax error for IO redirection is implemented
                if (DEBUG) printf("  tokenCount:%d tokenWordPos:%d newWordPos:%d wordCounter:%d\n", tokenCount, tokenWordPos, newWordPos, wordCounter);
                // If at end of line
                if (tokenWord[tokenWordPos] == '\0') {
                    // Add last word
                    if (inputWord) {
                        if (DEBUG) printf("  Adding word to command->input\n");
                        simpleCommand->input[inputPos] = '\0';
                        inputRedirect = false; inputWord = false;
                        if (DEBUG) printf("  command->input:%s intputRedirect:false\n", simpleCommand->input);
                    }
                    else if (outputWord) {
                        if (DEBUG) printf("  Adding word to command->output\n");
                        // If output word is empty
                        if (outputPos == 0) {
                            return NULL;
                        }
                        simpleCommand->output[outputPos] = '\0';
                        inputRedirect = false; outputWord = false;
                        if (DEBUG) printf("  command->output:%s outputRedirect:false\n", simpleCommand->output);
                    }
                    else if (newWord) {
                        simpleCommand->u.word[wordCounter][newWordPos] = '\0';
                        if (DEBUG) printf("  Adding word:%s to command->word[%d]  newWordPos=%d\n", simpleCommand->u.word[wordCounter], wordCounter, newWordPos);
                        wordCounter++;
                        simpleCommand->u.word[wordCounter] = NULL;
                        newWord = false;
                    } else {
                        simpleCommand->u.word[wordCounter] = NULL;
                    }
                    
                    break;
                }
                else if (tokenWord[tokenWordPos] == '<') {
                    if (DEBUG) printf("  <<<<<<<<<\n");
                    // If in word
                    if (outputRedirect || outputWord) {
                        return NULL;
                    }
                    
                    if (newWord) {
                        simpleCommand->u.word[wordCounter][newWordPos] = '\0';
                        if (DEBUG) printf("  Added word: %s to index: %d\n", simpleCommand->u.word[wordCounter], wordCounter);
                        wordCounter++;
                        newWord = false;
                    }
                    simpleCommand->u.word[wordCounter] = NULL;
                    
                    // if IO redirection is at beginning
                    if (wordCounter == 0 || inputRedirect) {
                        return NULL;
                    }
                    
                    simpleCommand->input = (char *)checked_malloc(sizeof(char)*100);
                    inputPos = 0;
                    inputRedirect = true;
                    tokenWordPos++;
                    
                    if (DEBUG) printf("  inputRedirect:true\n");
                }
                else if (tokenWord[tokenWordPos] == '>') {
                    if (DEBUG) printf("  >>>>>>>>>>>>>\n");
                    if (DEBUG) printf("inputRedirect:%d inputWord:%d outputRedirect:%d outputWord:%d\n", inputRedirect, inputWord, outputRedirect, outputWord);
                    
                    if (inputRedirect && isInputEmpty) {
                        return NULL;
                    }
                    
                    // If last char was part of inputWord
                    if (inputWord) {
                        simpleCommand->input[inputPos] = '\0';
                        inputRedirect = false; inputWord = false;
                    }
                    if (newWord) {
                        simpleCommand->u.word[wordCounter][newWordPos] = '\0';
                        if (DEBUG) printf("  convertTokens - Added word: %s to index: %d\n", simpleCommand->u.word[wordCounter], wordCounter);
                        wordCounter++;
                        newWord = false;
                    }
                    simpleCommand->u.word[wordCounter] = NULL;
                    
                    if (wordCounter == 0 || outputRedirect) {
                        return NULL;
                    }
                    
                    simpleCommand->output = (char *)checked_malloc(sizeof(char)*100);
                    outputPos = 0;
                    inputRedirect = false;
                    outputRedirect = true;
                    if (DEBUG) printf("  outputRedirect:true\n");
                    tokenWordPos++;
                    
                }
                // If char is equal to white space
                else if (tokenWord[tokenWordPos] == ' ') {
                    if (DEBUG) printf("   SPACE SPACE SPACE\n");
                    
                    if (inputRedirect && inputWord) {
                        simpleCommand->input[inputPos] = '\0';
                        inputWord = false;
                        if (DEBUG) printf("  command->input:%s inputRedirect:false\n", simpleCommand->input);
                    }
                    else if (outputRedirect && outputWord) {
                        simpleCommand->output[outputPos] = '\0';
                        outputWord = false;
                        if (DEBUG) printf("  command->output:%s outputRedirect:false\n", simpleCommand->output);
                    }
                    // If currently a word, parse the word
                    else if (newWord) {
                        simpleCommand->u.word[wordCounter][newWordPos] = '\0';
                        if (DEBUG) printf("  Adding word:%s to command->word[%d]  newWordPos=%d\n", simpleCommand->u.word[wordCounter], wordCounter, newWordPos);
                        
                        if (DEBUG) {
                            printf("%s-",simpleCommand->u.word[wordCounter]);
                            int tempPos = 0;
                            while (simpleCommand->u.word[wordCounter][tempPos] != '\0') {
                                printf("%c", simpleCommand->u.word[wordCounter][tempPos]);
                                tempPos++;
                            }
                            printf("|\n");
                        }
                        tokenWordPos++;
                        wordCounter++;
                        newWord = false; newWordPos = 0;
                    }
                    // If not a word
                    else {
                        if (DEBUG) printf("  Blank whitespace\n");
                        tokenWordPos++;
                    }
                    
                }
                // If char is a character
                else {
                    if (inputRedirect) {
                        if (DEBUG) printf("     inputWord\n");
                        simpleCommand->input[inputPos] = tokenWord[tokenWordPos];
                        inputPos++; tokenWordPos++;
                        inputWord = true; isInputEmpty = false;
                    }
                    else if (outputRedirect) {
                        if (DEBUG) printf("     outputWord\n");
                        simpleCommand->output[outputPos] = tokenWord[tokenWordPos];
                        outputPos++; tokenWordPos++;
                        outputWord = true;
                    }
                    // If not in a word, set newWord and create a new word
                    else if (!newWord) {
                        simpleCommand->u.word[wordCounter] = (char *)checked_malloc(sizeof(char)*100);
                        newWordPos = 0;
                        simpleCommand->u.word[wordCounter][newWordPos] = tokenWord[tokenWordPos];
                        newWordPos++;
                        tokenWordPos++;
                        newWord = true;
                    }
                    // Already in a word
                    else {
                        simpleCommand->u.word[wordCounter][newWordPos] = tokenWord[tokenWordPos];
                        newWordPos++;
                        tokenWordPos++;
                    }
                }
            }
            n->simpleCommand = simpleCommand;
        }
        
        // Add node to node list
        if (DEBUG) printf("    End of a token iteration: Adding node type: %d\n", n->type);
        addNodeToNodeList(nodeList, n);
        
        // Check to next token and free current token
        struct token *temp = tl->f_token;
        tl->f_token = tl->f_token->next;
        free(temp);
        tokenCount++;
    }
    if (DEBUG) printf("\n");
    return nodeList;
}


command_stream_t
make_command_stream (int (*get_next_byte) (void *),
                     void *get_next_byte_argument)
{
    if (DEBUG) printf("Testing: Beginning of make_command_stream\n");
    
    struct command_stream *commandStream = (struct command_stream *)checked_malloc(sizeof(struct command_stream));
    
    // Get initial line for processing
	char *line=(char *)checked_malloc(sizeof(char *) * bufSize);
    line = getLineFromScript(*get_next_byte, get_next_byte_argument);
    
    // Make a token list
    struct token_list *tokenList = (struct token_list *)checked_malloc(sizeof(struct token_list));
    tokenList->f_token = NULL;
    tokenList->l_token = NULL;
    
    // Counter variables
    int linePos = 0;
    int startPos = 0;
    int lineNum = 1;
    int parenDepth = 0;
    
    bool isToken = false;
    bool newWord = false;
    bool skippedLine = false;
    bool isLastTokenSpecial = false; bool lastLineNull = true;
    bool blankLine = true;
    
    while (line != NULL) {
        if (DEBUG) printf("Line %d text: %s\n", lineNum, line);
        lastLineNull = false;
        while (1) {
            //if (DEBUG) printf("Looking at char:%c int:%d\n", line[linePos], (int)line[linePos]);
            // Check if line is empty or a comment
            if (line[0] == '\0' || line[0] == '#') {
                //if (DEBUG) printf("Empty new line detected\n");
                skippedLine = true;
                lineNum++;
                break;
            }
            // Check if char is a special token (excluding IO redirection)
            if (isSpecialToken(line[linePos])) {
                // If current in a word (simple), make a token up to current linePos
                if (linePos == 0 && line[linePos] != '(') {
                    errorMsg(lineNum);
                }
                // If already in the process of a parsing a word, create token for current word and add to token list
                if (newWord) {
                    char *subString = getSubstring(line, startPos, linePos-1);
                    //if (DEBUG) printf("Substring: %s\n", subString);
                    if (DEBUG) printf("Token Not Blank, Adding\n");
                    struct token *newSimpleToken=(struct token *)checked_malloc(sizeof(struct token));
                    newSimpleToken->word = subString;
                    newSimpleToken->next = NULL;
                    //if (DEBUG) printf("Token word: %s\n", newSimpleToken->word);
                    addTokenToTokenList(tokenList, newSimpleToken);
                }
                startPos = linePos;
                
                // Make a token for special token
                struct token *newSpecialToken=(struct token *)checked_malloc(sizeof(struct token));
                newSpecialToken->next = NULL;
                // Case for &&
                if (line[linePos] == '&') {
                    if (line[linePos+1] == '&') {
                        newSpecialToken->word = getSubstring(line, startPos, linePos+1);
                        linePos += 2;
                        isLastTokenSpecial = true;
                        if (DEBUG) printf("Added Operator Token is a:%s\n", newSpecialToken->word);
                    }
                    else {
                        errorMsg(lineNum);
                    }
                // Case for | or ||
                } else if (line[linePos] == '|') {
                    if (line[linePos+1] == '|') {
                        //Make an OR token
                        newSpecialToken->word = getSubstring(line, startPos, linePos+1);
                        if (DEBUG) printf("Added Operator Token is a:%s\n", newSpecialToken->word);
                        linePos += 2;
                    } else {
                        //Make a PIPE token
                        newSpecialToken->word = getSubstring(line, startPos, linePos);
                        linePos++;
                    }
                    isLastTokenSpecial = true;
                } else if (line[linePos] == '(') {
                    parenDepth++;
                    newSpecialToken->word = getSubstring(line, startPos, linePos);
                    if (DEBUG) printf("Added Operator Token is a:%s\n", newSpecialToken->word);
                    linePos++;
                    isLastTokenSpecial = true;
                } else if (line[linePos] == ')') {
                    parenDepth--;
                    if (parenDepth < 0) {
                        errorMsg(lineNum);
                    }
                    if (parenDepth == 0) {
                        isLastTokenSpecial = false;
                    } else {
                        isLastTokenSpecial = true;
                    }
                    newSpecialToken->word = getSubstring(line, startPos, linePos);
                    if (DEBUG) printf("Added Operator Token is a:%s\n", newSpecialToken->word);
                    linePos++;
                } else if ((line[linePos] == ';')) {
					//Do not count ; as a special token since the line can end with this
					isLastTokenSpecial = false;
					newSpecialToken->word = getSubstring(line, startPos, linePos);
                    if (DEBUG) printf("Added Operator Token is a:%s\n", newSpecialToken->word);
                    linePos++;
				}else {
                    newSpecialToken->word = getSubstring(line, startPos, linePos);
                    if (DEBUG) printf("Added Operator Token is a:%s\n", newSpecialToken->word);
                    linePos++;
                }
                startPos = linePos;
                //if (DEBUG) printf("Token word: %s\n", newSpecialToken->word);
                
                // Add special token to token list
                addTokenToTokenList(tokenList, newSpecialToken);
                newWord = false;
                blankLine = false;
            }
            // Not a special token
            else {
                if (isValidWordChar(line[linePos]) || line[linePos] == ' ') {
                    if (!newWord && line[linePos] != ' ') {
                        newWord = true;
                        blankLine = false;
                    }
                } else {
                    errorMsg(lineNum);
                }
                linePos++;
            }
            if (line[linePos] == '#' || line[linePos] == '\0') {
                if (newWord) {
                    char *subString = getSubstring(line, startPos, linePos-1);
                    //if (DEBUG) printf("Substring: %s\n", subString);
                    struct token *newSimpleToken=(struct token *)checked_malloc(sizeof(struct token));
                    newSimpleToken->word = subString;
                    newSimpleToken->next = NULL;
                    //if (DEBUG) printf("Token word: %s\n", newSimpleToken->word);
                    // Add simple token to token list
                    addTokenToTokenList(tokenList, newSimpleToken);
                    isLastTokenSpecial = false;
                    blankLine = false;
                }
                if ((line[linePos] == '\0' && isLastTokenSpecial) || (parenDepth > 0) || blankLine) {
                    if (DEBUG) printf("! VALID LAST TOKEN !\n");
                    
                    skippedLine = true;
                    
                    if (parenDepth > 0) {
                        // Replace newline with a semi-colon for subshell spanning multiple lines
                        if (DEBUG) printf ("Replacing newline with ';'\n");
                        struct token *newSemicolonToken=(struct token *)checked_malloc(sizeof(struct token));
                        newSemicolonToken->next = NULL;
                        char *semicolon = (char *)checked_malloc(sizeof(char *)*2);
                        semicolon[0] = ';';
                        semicolon[1] = '\0';
                        newSemicolonToken->word = semicolon;
                        addTokenToTokenList(tokenList, newSemicolonToken);
                    }
                }
                break;
            }
        }
        
        //DEBUG: tokenList
        if (DEBUG) {
            printf("*** Complete tokenList ***\n");
            int tokenCounter = 0;
            struct token *tokenListHead = tokenList->f_token;
            struct token *tokenListTail = tokenList->l_token;
            while (tokenList->f_token != NULL) {
                printf("Word %d:%s\n", tokenCounter, tokenList->f_token->word);
                tokenList->f_token=tokenList->f_token->next;
                tokenCounter++;
            }
            tokenList->f_token = tokenListHead;
            tokenList->l_token = tokenListTail;
            printf("\n\n");
        }
        
        // If line was not empty or a comment
        if (!skippedLine) {
            // Convert the token list into a node list which is a list of commands (in the struct command format)
            struct node_list *nodeList = convertTokens(tokenList);
            if (nodeList == NULL) {
                errorMsg(lineNum);
            }
            
            // DEBUG: nodeList
            /*
            if (DEBUG) {
                printf("*** Start nodeList ***\n");
                printf("Returned nodeList address:%p\n", nodeList);
                int nodeListCounter = 0;
                while (nodeList->f_node != NULL) {
                    printf("Node %d, Type:%d\n", nodeListCounter, nodeList->f_node->type);
                    if (nodeList->f_node->type == SIMPLE) {
                        int count = 0;
                        while (nodeList->f_node->simpleCommand->u.word[count] != NULL) {
                            printf("Word:%s\n", nodeList->f_node->simpleCommand->u.word[count]);
                            int countB = 0;
                            while (nodeList->f_node->simpleCommand->u.word[count][countB] != '\0') {
                                printf("countB:%d-:%c\n", countB, nodeList->f_node->simpleCommand->u.word[count][countB]);
                                countB++;
                            }
                            count++;
                        }
                    }
                    nodeListCounter++;
                    nodeList->f_node=nodeList->f_node->next;
                    }
                printf("*** Finish Complete nodeList ***\n\n");
            }
            */
            
            // Prioritize and create the command tree structure from the node list
            struct command *rootCommand = makeTree(nodeList);
            if (rootCommand == NULL) {
                printf(" null rootCommand\n");
                errorMsg(lineNum);
            }
            
            // Add returned command to command stream
            addCommandToCommandStream(commandStream, rootCommand);
        }
        // Variable clean up and proceed to next line
        free(line);
        line = getLineFromScript(*get_next_byte, get_next_byte_argument);
        lineNum++;
        linePos = 0; startPos = 0; isToken = false; newWord = false; skippedLine = false; lastLineNull = true; blankLine = true;
        if (DEBUG) printf("End of a loop\n");
    }
    if (parenDepth > 0 && lastLineNull) {
        printf("parenDepth:%d\n", parenDepth);
        if (DEBUG) printf("Error here\n");
        errorMsg(lineNum);
    }
    if (isLastTokenSpecial && lastLineNull) {
        if (DEBUG) printf("Multiline syntax error\n");
        errorMsg(lineNum);
    }
    if (DEBUG) printf("Finish testing\n");
    return commandStream;
}

command_t
read_command_stream (command_stream_t s)
{
    return removeCommandFromCommandStream(s);
}
