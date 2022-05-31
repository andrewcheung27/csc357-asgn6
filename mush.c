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


int inputOpen(char *name) {
    int fd;

    fd = open(name, O_RDONLY);
    if (fd == -1) {
        perror("open");
        /* TODO: clean up and reset instead of exiting shell */
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
        /* TODO: clean up and reset instead of exiting shell */
        exit(EXIT_FAILURE);
    }

    return fd;
}


/* TODO: handle SIGINT */
void handler(int signum) {
    reset();
}


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

    else {
        path = getenv("HOME");
        if (path == NULL) {
            /* use getpwuid to look up home directory */
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


int main(int argc, char *argv[]) {
    FILE *infile;
    sigset_t sigset;
    sigset_t oldSigset;
    struct sigaction sa;

    int i;
    int *fds;
    int fdsLen;
    int in;
    int out;
    int newPipe[2];
    int numChildren;
    pid_t child;
    int status;

    char *line;
    pipeline myPipeline;
    struct clstage *prevStage;
    struct clstage *stage;


    /* block interrupts until right before launching children */
    sigemptyset(sigset);
    sigaddset(sigset, SIGINT);
    sigprocmask(SIG_SETMASK, &sigset, &oldSigset);
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler;
    sigaction(SIGINT, sa, NULL);


    /* arg things */
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


    /* TODO: put this in the shell loop (poll infile???) */
    /* TODO: built-in cd command */
    line = readLongString(infile);
    if (line == NULL) {
        /* TODO: handle readLongString error or EOF differently */
        continue;
    }
    myPipeline = crack_pipeline(line);
    free(line);

    fdsLen = myPipeline->length * 2;
    fds = (int *) malloc(sizeof(int) * fdsLen);
    if (fds == NULL) {
        perror("malloc");
        /* todo: clean up and reset instead of exiting */
        exit(EXIT_FAILURE);
    }
    numChildren = 0;
    newPipe[READ_END] = -1;
    newPipe[WRITE_END] = -1;
    prevStage = NULL;
    stage = (myPipeline->stage)[numChildren];


    /* built-in cd command */
    if (!strcmp((stage->argv)[0], "cd")) {
        if (tryCD(stage->argc, stage->argv) == -1) {
            reset();
        }
    }


    /* fill in file descriptors table (fds),
     * making pipes and opening files as needed */
    while (numChildren < myPipeline->length) {
        in = 2 * numChildren;
        out = 2 * numChildren + 1;

        /* if there is an inname, open and read from that */
        if (stage->inname != NULL) {
            fds[in] = inputOpen(stage->inname);
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
    free_pipeline(myPipeline);


    /* unblock interrupts for the children */
    sigprocmask(SIG_SETMASK, &oldSigset, &sigset);


    /* glorious birth! */
    for (i = 0; i < myPipeline->length; i++) {
        in = 2 * i;
        out = 2 * i + 1;

        child = fork();
        if (child == -1) {
            perror("fork");
            /* TODO: clean up and reset instead of exiting */
            exit(EXIT_FAILURE);
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
            for (i = 0; i < fdsLen; i++) {
                close(fds[i]);
            }

            /* launch the program */
            execvp((stage->argv)[0], myPipleine->stage->argv);

            /* _exit from child if exec failed */
            perror((stage->argv)[0]);
            _exit(EXIT_FAILURE);
        }
    }
    free(fds);


    /* close write ends of the children (odd indices), so they don't hang */
    for (i = WRITE_END; i < fdsLen; i += 2) {
        close(fds[i]);
    }


    /* wait for all the children */
    for (i = 0; i < numChildren; i++) {
        wait(&status);
    }
    /* TODO: end loop, reset shell */


    /* cleanup */
    yylex_destroy();
    return 0;
}

