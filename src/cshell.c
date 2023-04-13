/**
 * AUTHOR:  Arnav Marchareddy
 * 
 * FILE:    cshell.c
*/

#define _XOPEN_SOURCE 700

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <pwd.h>
#include <signal.h>

#define BLUE "\x1b[34;1m"
#define DEFAULT "\x1b[0m"

volatile sig_atomic_t interrupted = 0;

/*
Given a string, it splits it by a delimiter and returns a string array (char**)
*/
char **str_split(char *str, char *delim, int *len)
{
    char **tokens = NULL;
    int token_count = 0;

    char *token = strtok(str, delim);

    while (token != NULL)
    {
        // allocate memory for a new string to hold the current word
        char *word = malloc(strlen(token) + 1);
        strcpy(word, token);

        int word_len = strlen(word);
        if (word[word_len - 1] == '\n')
        {
            word[word_len - 1] = '\0';
        }

        // resize the array of words to add the new char pointer
        token_count++;
        tokens = realloc(tokens, sizeof(char *) * token_count);
        tokens[token_count - 1] = word;

        // move on to the next word
        token = strtok(NULL, " ");
    }

    *len = token_count;

    return tokens;
}

/*It frees the string array of tokens iteratively*/
void free_char_array(char **array, int size)
{
    for (int i = 0; i < size; i++)
    {
        free(array[i]);
    }

    free(array);
}

/*This method handles the captured sigint*/
void handle_sigint(int sig)
{
    if (sig == SIGINT)
    {
        interrupted = 1;
    }
}

int main(int argc, char *argv[])
{
    struct sigaction sa;
    sa.sa_flags = 0;
    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);

    char *cwd = NULL;

    while (1)
    {
        char *input = NULL;
        size_t input_size = 0;

        /*cwd is set here with any error being handled*/
        if ((cwd = getcwd(NULL, 0)) == NULL && errno == EINTR)
        {
            errno = 0;
            continue;
        }

        printf("%s[%s]%s> ", BLUE, cwd, DEFAULT);
        fflush(stdout); //stdout is flushed after the prompt is printed to flush the buffer to stdout
        free(cwd);

        /*The line is read here with error being handled accordingly*/
        if (getline(&input, &input_size, stdin) == -1)
        {
            free(input);

            if (interrupted == 1 || errno == EINTR) //interrupt is checked here in case getline was interrupted
            {
                interrupted = 0;
                errno = 0;

                clearerr(stdin); //any error in stdin is cleared for the next iteration

                printf("\n");
                fflush(stdout);

                continue;
            }
            else /*Any other errors are handled here*/
            {
                fprintf(stderr, "Error: Failed to read from stdin. %s.\n", strerror(errno));

                errno = 0;

                exit(EXIT_FAILURE);
            }
        }

        if (*input == '\n') { //if the user simply presses enter, then a new prompt should be displayed
            free(input);

            continue;
        }


        int tokens_len = 0;
        char **tokens = str_split(input, " ", &tokens_len);

        if (strcmp(tokens[0], "cd") == 0) // handle cd cmd
        {
            if (tokens_len > 2) // too many arguments
            {
                fprintf(stderr, "Error: Too many arguments to cd.\n");
            }
            else
            {
                char *target_dir = tokens[1];

                if (tokens[1] == NULL || strcmp(tokens[1], "~") == 0) // get home dir
                {
                    uid_t uid = getuid();
                    struct passwd *pw = getpwuid(uid);

                    if (pw == NULL)
                    {
                        fprintf(stderr, "Error: Cannot get passwd entry. %s.\n", strerror(errno));
                    }

                    target_dir = pw->pw_dir;
                }

                if (chdir(target_dir) == -1) // handle error with changing dir
                {
                    fprintf(stderr, "Error: Cannot change directory to %s. %s.\n", tokens[1], strerror(errno));
                }
            }
        }
        else if (strcmp(tokens[0], "exit") == 0) // handle exit cmd
        {
            free(input);
            free_char_array(tokens, tokens_len);

            exit(EXIT_SUCCESS);
        }
        else
        {
            // fork the process
            pid_t pid = fork();

            if (pid == -1)
            {
                fprintf(stderr, "Error: fork() failed. %s.\n", strerror(errno));

                exit(EXIT_FAILURE);
            }
            else if (pid == 0)
            {
                // child process
                // execute the command with execvp
                if (interrupted == 1)
                {
                    interrupted = 0;

                    free(input);
                    free_char_array(tokens, tokens_len);

                    exit(EXIT_FAILURE);
                }

                if (execvp(tokens[0], tokens) == -1)
                {
                    fprintf(stderr, "Error: exec() failed. %s.\n", strerror(errno));

                    free(input);
                    free_char_array(tokens, tokens_len);

                    exit(EXIT_FAILURE);
                }

                free(input);
                free_char_array(tokens, tokens_len);

                exit(EXIT_SUCCESS);
            }
            else
            {
                // parent process
                int status;

                if (interrupted == 1)
                {
                    interrupted = 0;

                    free(input);
                    free_char_array(tokens, tokens_len);

                    continue;
                }

                if (waitpid(pid, &status, 0) == -1)
                {
                    fprintf(stderr, "Error: wait() failed. %s.\n", strerror(errno));
                }
            }
        }

        /*Free anything before the next iteration*/
        free(input);
        free_char_array(tokens, tokens_len);
    }

    return 0;
}