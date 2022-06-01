/* the Minimally Useful Shell (MUSH) */


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
#define SHELL_PROMPT "8=P "

extern int errno;
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
            perror("chdir");
            return -1;
        }
    }

    /* if path wasn't specified, change to home directory */
    else {
        path = getenv("HOME");
        if (path == NULL) {
            p = getpwuid(getuid());
            if (p == NULL) {
                fprintf(stderr, "unable to determine home directory\n");
                return -1;
            }
            res = chdir(p->pw_dir);
            free(p);
            if (res == -1) {
                perror("chdir");
                return -1;
            }
        }
    }

    return 0;
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
        exit(EXIT_FAILURE);
    }
    numChildren = 0;
    newPipe[READ_END] = -1;
    newPipe[WRITE_END] = -1;
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    prevStage = NULL;
    stage = (myPipeline->stage)[numChildren];


    /* fill in file descriptors table (fds),
     * making pipes and opening files as needed */
    while (numChildren < myPipeline->length) {
        /* built-in cd command */
        if (!strcmp((stage->argv)[0], "cd")) {
            if (tryCD(stage->argc, stage->argv) == -1) {
                /* tryCD printed error */
                free(fds);
                return -1;
            }
        }

        in = 2 * numChildren;
        out = 2 * numChildren + 1;

        /* if there is an inname, open and read from that */
        if (stage->inname != NULL) {
            fds[in] = inputOpen(stage->inname);
            if (fds[in] == -1) {
                fprintf(stderr, "could not open `%s`: %s\n", name, strerror(errno));
                free(fds);
                return -1;
            }
        }
        /* for NULL inname, read from stdin or pipe */
        else {
            if (prevStage == NULL) {
                fds[in] = STDIN_FILENO;
            }
            else {
                fds[in] = newPipe[READ_END];
            }
        }

        /* if there is an outname, open and write to that */
        if (stage->outname != NULL) {
            fds[out] = outputOpen(stage->outname);
            if (fds[out] == -1) {
                fprintf(stderr, "could not open `%s`: %s\n", name, strerror(errno));
                free(fds);
                return -1;
            }
        }
        /* for NULL outname, write to stdout or pipe */
        else {
            if (numChildren + 1 == myPipeline-> length) {
                fds[out] = STDOUT_FILENO;
            }
            else {
                pipe(newPipe);
                fds[out] = newPipe[WRITE_END];
            }
        }

        numChildren++;
        prevStage = stage;
        stage = (myPipeline->stage)[numChildren];
    }


    /* launch children */
    for (i = 0; i < myPipeline->length; i++) {
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
            if (fds[in] != STDIN_FILENO) {
                dup2(fds[in], STDIN_FILENO);
            }
            if (outFD != STDOUT_FILENO) {
                dup2(fds[out], STDOUT_FILENO);
            }

            /* clean up duplicate FDs */
            for (i = 0; i < myPipeline->length * 2; i++) {
                close(fds[i]);
            }

            /* unblock interrupts and exec child process */
            sigprocmask(SIG_UNBLOCK, &sigset, 0);
            execvp((stage->argv)[0], myPipleine->stage->argv);

            /* _exit from child if exec failed */
            perror((stage->argv)[0]);
            _exit(EXIT_FAILURE);
        }
    }


    /* close write ends of the children (odd indices), so they don't hang */
    for (i = WRITE_END; i < myPipeline->length * 2; i += 2) {
        close(fds[i]);
    }


    /* wait for all the children */
    i = 0;
    while (i < numChildren) {
        wait(&status);
        if (WIFEXITED(status)) {
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


    /* arg things */
    if (argc == 1) {
        infile = stdin;
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


    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);
    /* set up SIGINT handler */
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigaction(SIGINT, sa, NULL);

    while (!feof(infile)) {
        printf("%s", SHELL_PROMPT);

        /* read command into pipeline */
        line = readLongString(infile);
        if (line == NULL) {
            fprintf(stderr, "input could not be read\n");
            continue;
        }
        myPipeline = crack_pipeline(line);
        free(line);
        if (myPipeline == NULL) {
            fprintf(stderr, "failed to create pipeline\n");
        }
        /* abandon command line if there was an interrupt */
        if (interrupted) {
            interrupted = 0;
            continue;
        }

        /* block interrupts while setting up to launch children */
        sigprocmask(SIG_BLOCK, &sigset, 0);

        /* execute the processes */
        gloriousBirth(argc, argv, myPipeline);

        free_pipeline(myPipeline);
        fflush(stdout);
        sigprocmask(SIG_UNBLOCK, &sigset, 0);
    }


    /* cleanup */
    fclose(infile);
    yylex_destroy();
    return 0;
}

