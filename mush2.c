/* the Minimally Useful Shell (MUSH) */


#include <errno.h>
#include <fcntl.h>
#include <mush.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define READ_END 0
#define WRITE_END 1
#define NO_DUP -1
#define SHELL_PROMPT "8-P "

char interrupted = 0;


/* opens infile */
int inputOpen(char *name) {
    int fd;

    fd = open(name, O_RDWR);
    if (fd == -1) {
        return -1;
    }

    return fd;
}


/* opens outfile with O_CREAT */
int outputOpen(char *name) {
    int m, fd;

    m = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IWOTH | S_IROTH;
    fd = open(name, O_RDWR | O_CREAT | O_TRUNC, m);
    if (fd == -1) {
        return -1;
    }

    return fd;
}


/* assigned to SIGINT, sets global interrupted variable to true */
void handler(int signum) {
    interrupted = 1;
}


/* built-in cd command using chdir */
int tryCD(int argc, char *argv[]) {
    char *path;
    struct passwd *p;
    int res;

    if (argc > 2) {
        fprintf(stderr, "usage: cd [directory]\n");
        return -1;
    }

    if (argc == 2) {
        if (chdir(argv[1]) == -1) {
            perror(argv[1]);
            return -1;
        }
        return 0;
    }

    /* if path wasn't specified, change to home directory with HOME env */
    path = getenv("HOME");
    if (path != NULL && chdir(path) != -1) {
        return 0;
    }

    /* if HOME didn't work, try looking up password entry */
    p = getpwuid(getuid());
    if (p == NULL) {
        fprintf(stderr, "unable to determine home directory\n");
        return -1;
    }
    res = chdir(p->pw_dir);
    if (res == -1) {
        perror(p->pw_dir);
    }
    free(p);
    return res;
}


/* launches the child processes in a pipeline */
int gloriousBirth(int argc, char *argv[], pipeline myPipeline) {
    int i;
    int *fds;
    int in;
    int out;
    int newPipe[2];
    int numChildren;
    pid_t child;
    int status;
    sigset_t sigset;
    struct clstage *prevStage;
    struct clstage *stage;

    fds = (int *) malloc(sizeof(int) * myPipeline->length * 2);
    if (fds == NULL) {
        perror("malloc");
        return -1;
    }
    numChildren = 0;
    newPipe[READ_END] = -1;
    newPipe[WRITE_END] = -1;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    prevStage = NULL;
    stage = myPipeline->stage;

    /* fill in file descriptors table (fds),
     * making pipes and opening files as needed */
    while (numChildren < myPipeline->length) {
        in = 2 * numChildren;
        out = 2 * numChildren + 1;

        /* built-in cd command */
        if (!strcmp((stage->argv)[0], "cd")) {
            i = tryCD(stage->argc, stage->argv);
            if (i == -1) {
                /* tryCD printed error */
                free(fds);
                return -1;
            }
            fds[in] = -1;
            fds[out] = -1;
            numChildren++;
            prevStage = stage;
            stage = myPipeline->stage + numChildren;
            continue;
        }

        /* if there is an inname, open and read from that */
        if (stage->inname != NULL) {
            fds[in] = inputOpen(stage->inname);
            if (fds[in] == -1) {
                fprintf(stderr, "could not open `%s`: %s\n",
                        stage->inname, strerror(errno));
                free(fds);
                return -1;
            }
        }
        /* for NULL inname, read from stdin or pipe */
        else {
            if (prevStage == NULL) {
                fds[in] = NO_DUP;
            }
            else {
                fds[in] = newPipe[READ_END];
            }
        }

        /* if there is an outname, open and write to that */
        if (stage->outname != NULL) {
            fds[out] = outputOpen(stage->outname);
            if (fds[out] == -1) {
                fprintf(stderr, "could not open `%s`: %s\n",
                        stage->outname, strerror(errno));
                free(fds);
                return -1;
            }
        }
        /* for NULL outname, write to stdout or pipe */
        else {
            if (numChildren + 1 == myPipeline->length) {
                fds[out] = NO_DUP;
            }
            else {
                pipe(newPipe);
                fds[out] = newPipe[WRITE_END];
            }
        }

        numChildren++;
        prevStage = stage;
        stage = myPipeline->stage + numChildren;
    }

    /* launch children */
    for (i = 0; i < myPipeline->length; i++) {
        stage = myPipeline->stage + i;

        if (!strcmp(stage->argv[0], "cd")) {
            numChildren--;
            continue;
        }

        in = 2 * i;
        out = 2 * i + 1;

        child = fork();
        if (child == -1) {
            fprintf(stderr, "fork `%s`: %s\n", argv[0], strerror(errno));
            return -1;
        }

        /* child */
        if (child == 0) {
            /* I/O redirection */
            if (fds[in] != NO_DUP) {
                dup2(fds[in], STDIN_FILENO);
            }
            if (fds[out] != NO_DUP) {
                dup2(fds[out], STDOUT_FILENO);
            }

            /* clean up duplicate FDs */
            for (i = 0; i < myPipeline->length * 2; i++) {
                close(fds[i]);
            }

            /* unblock interrupts and exec child process */
            sigprocmask(SIG_UNBLOCK, &sigset, 0);
            execvp(stage->argv[0], stage->argv);

            /* _exit from child if exec failed */
            perror((stage->argv)[0]);
            _exit(EXIT_FAILURE);
        }
    }

    /* close write ends of the children (odd indices), so they don't hang */
    for (i = 1; i < myPipeline->length * 2; i += 2) {
        close(fds[i]);
    }

    /* wait for all the children */
    i = 0;
    sigprocmask(SIG_UNBLOCK, &sigset, 0);
    while (i < numChildren) {
        if (wait(&status) != -1) {
            i++;
        }
    }

    free(fds);
    return 0;
}


int main(int argc, char *argv[]) {
    FILE *infile;
    char *line;
    pipeline myPipeline;
    sigset_t sigset;
    struct sigaction sa;
    char printPrompt = 0;

    /* arg things */
    if (argc == 1) {
        infile = stdin;
        printPrompt = 69;
    }
    else if (argc == 2) {
        infile = fopen(argv[1], "r");
        if (infile == NULL) {
            perror(argv[1]);
            exit(EXIT_FAILURE);
        }
    }
    else {
        fprintf(stderr, "usage: mush2 [infile]\n");
        exit(EXIT_FAILURE);
    }

    if (printPrompt && isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)) {
        printPrompt = 69;
    }

    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    /* set up SIGINT handler */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigaction(SIGINT, &sa, NULL);

    while (!feof(infile)) {
        if (printPrompt) {
            printf("%s", SHELL_PROMPT);
            fflush(stdout);
        }

        /* read command into pipeline */
        line = readLongString(infile);
        if (line == NULL) {
            if (printPrompt) {
                printf("\n");
                fflush(stdout);
            }
            interrupted = 0;
            continue;
        }
        myPipeline = crack_pipeline(line);
        free(line);
        if (myPipeline == NULL) {
            interrupted = 0;
            continue;
        }
        /* abandon command line if there was an interrupt */
        if (interrupted) {
            free_pipeline(myPipeline);
            if (printPrompt) {
                printf("\n");
                fflush(stdout);
            }
            interrupted = 0;
            continue;
        }
        /* print_pipeline(stderr, myPipeline); */

        /* block interrupts while launching children */
        sigprocmask(SIG_BLOCK, &sigset, 0);

        /* execute the processes */
        gloriousBirth(argc, argv, myPipeline);
        if (interrupted) {
            if (printPrompt) {
                printf("\n");
                fflush(stdout);
            }
            interrupted = 0;
        }

        free_pipeline(myPipeline);
        fflush(stdout);
    }


    /* cleanup */
    if (printPrompt) {
        printf("Successfully exited.");
        printf("You're welcome, and thank you very mush2!\n");
        fflush(stdout);
    }
    fclose(infile);
    yylex_destroy();
    return 0;
}

