#include <fcntl.h>
#include <mush.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#define READ_END 0
#define WRITE_END 1


int inputOpen(char *name) {
    int fd;

    fd = open(name, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    return fd;
}


int outputOpen(char *name) {
    int m, fd;

    m = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IWOTH | S_IROTH;
    fd = open(name, O_RDWR | O_CREAT | O_TRUNC, m);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    return fd;
}


void handler(int signum) {
    reset();
}


int main(int argc, char *argv[]) {
    FILE *infile;
    sigset_t sigset;
    sigset_t oldSigset;
    struct sigaction sa;

    int inPipe[2];
    int outPipe[2];
    pid_t childPID;
    char *line;
    pipeline myPipeline;
    struct clstage *prevStage;
    struct clstage *stage;
    struct clstage *nextStage;
    int i;

    mode_t m;
    int inFD;
    int outFD;


    /* block interrupts until right before launching children */
    sigemptyset(sigset);
    sigaddset(sigset, SIGINT);
    sigprocmask(SIG_SETMASK, &sigset, &oldSigset);

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigaction(SIGINT, sa, NULL);


    if (argc == 1) {
        infile = stdin;
    }
    else if (argc == 2) {
        infile = fopen(argv[1], "r");
    }
    else {
        fprintf(stderr, "usage: mush2 [infile]\n");
        exit(EXIT_FAILURE);
    }


    /* handle a command, probably put this in a loop */


    /* poll infile???? */
    line = readLongString(infile);
    if (line == NULL) {
        continue;
    }
    myPipeline = crack_pipeline(line);

    i = 0;
    prevStage = NULL;
    stage = (myPipeline->stage)[i];
    nextStage = (myPipeline->stage)[i + 1];
    while (stage != NULL) {
        if (prevStage == NULL) {
            if (stage->inname == NULL) {
                inFD = STDIN_FILENO;
            }
            else {
                inFD = inputOpen(stage->inname);
            }
        }
        else {
            if (stage->inname == NULL) {
                /* use read end of pipe */
            }
            else {
                inFD = inputOpen(stage->inname);
            }
        }

        if (nextStage == NULL) {
            if (stage->outname == NULL) {
                outFD = STDOUT_FILENO;
            }
            else {
                outFD = outputOpen(stage->outname);
            }
        }
        else {
            if (stage->outname == NULL) {
                /* pipe() and use write end */
            }
            else {
                outFD = outputOpen(stage->outname);
            }
        }

        childPID = fork();
        if (childPID == -1) {
            perror("fork");
            exit(EXIT_FAILURE);
        }

        if (!childPID) {
            /* pipe shit */
            dup2()

            execvp((stage->argv)[0], myPipleine->stage->argv);
            /* exec failed */
            perror((stage->argv)[0]);
            _exit(EXIT_FAILURE);
        }

        prevStage = stage;
        stage = (myPipeline->stage)[i];
        nextStage = (myPipeline->stage)[i + 1];
    }





    free_pipeline(myPipeline);
    yylex_destroy();
    return 0;
}

