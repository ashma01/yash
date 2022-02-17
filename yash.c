/**
Ctr-c  SIGINT
Ctrl-z SIGTSTP
**/

#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>

#define fg 1
#define bg 0

typedef enum
{
    RUNNING,
    STOPPED,
    DONE
} JobStatus;

struct processList
{
    char *processString;
    int groupId;
    pid_t cpid;
    char *inputPath;
    char *outputPath;
    char **processArgs;
    struct processList *next;
};

struct jobList
{
    int jobId;
    char *jobCommand;
    int jobCount;
    char *jobSign;
    JobStatus jobStatus;
    struct processList *process;
    struct jobList *next;
};

struct jobList *deleteJobByPid(struct jobList **headRef, int pid);
int *getPidList(struct jobList **headRef);
const char *getJobsStatus(JobStatus jobStatus);
bool search(struct jobList **headRef, int pid);
void printdone(struct jobList *root);
struct jobList *newNode(int jobId, char *jobCommand, JobStatus jobStatus, int jobCount, struct processList *process);
void pushJob(struct jobList **root, int jobId, char *jobCommand, JobStatus jobStatus, int jobCount, struct processList *process);
struct jobList *popJob(struct jobList **root);
void printJobs(struct jobList *root);
struct jobList *deleteJobByStatus(struct jobList **headRef, JobStatus jobStatus);
int checkIfShellCommands(char *inString);
char **parseStringStrtok(char *str, char *delim);
struct processList *parseSubCommand(char **subCommand);
struct processList *parseStringforPipes(char **parsedCmdArray);
int exexuteCommands(struct processList *rootProcess, int infd, int outfd, int job_type);
int executeParsedCommand(struct processList *rootProcess, int job_type);
void executeShellCommands(char *inString);
char **parsecommands(char *inString);
void sigChildHandler(int signo);
int isEmpty(struct jobList *root);
void checkPrevJobCount(struct jobList **temp);
void changePrevJobSign(struct jobList **jobRef);

struct jobList *rootJob = NULL;
struct processList *rootProcess = NULL;
pid_t cpid;
int wpid;
int wstatus;
int globalJobNumber = 0;

int isEmpty(struct jobList *root)
{
    return !root;
}

bool search(struct jobList **headRef, int pid)
{
    struct jobList *temp = *headRef, *prev; // Initialize current
    while (temp != NULL)
    {
        if (temp->jobId == pid)
            return true;
        temp = temp->next;
    }
    return false;
}

int *getPidList(struct jobList **headRef)
{
    struct jobList *temp = *headRef;
    int *pidlist = (int *)malloc(sizeof(int) * 1000);
    int index = 0;

    if (headRef == NULL)
    {
        return NULL;
    }
    while (temp != NULL)
    {
        pidlist[index] = temp->jobId;
        temp = temp->next;
        index++;
    }
    return pidlist;
}

void printdone(struct jobList *root)
{

    if (root != NULL)
    {

        printf("\n[%d] %s %s\n", root->jobCount, "done", root->jobCommand);
    }
}
void sigChildHandler(int signo)
{

    int *pidlist;
    pidlist = getPidList(&rootJob);
    if (pidlist != NULL)
    {
        int arraySize = sizeof(pidlist);
        int intSize = sizeof(pidlist[0]);
        int length = arraySize / intSize;
        int pidsize = 0;
        int index = 0;
        while (index < length)
        {

            int status, pid;
            if (pidlist[index] != 0)
            {
                pid = waitpid(pidlist[index], &status, WNOHANG);
                struct jobList *jobObj =
                    (struct jobList *)
                        malloc(sizeof(struct jobList));
                jobObj = deleteJobByPid(&rootJob, pid);
                printdone(jobObj);
            }
            index++;
        }
    }
}

struct jobList *newNode(int jobId, char *jobCommand, JobStatus jobStatus, int jobCount, struct processList *process)
{
    struct jobList *job =
        (struct jobList *)
            malloc(sizeof(struct jobList));
    job->jobCommand = jobCommand;
    job->jobId = jobId;
    job->jobStatus = jobStatus;
    job->jobCount = jobCount;
    job->process = process;
    // job->jobSign = "+";
    job->next = NULL;
    return job;
}

const char *getJobsStatus(JobStatus jobStatus)
{
    switch (jobStatus)
    {
    case RUNNING:
        return "RUNNING";
    case STOPPED:
        return "STOPPED";
    case DONE:
        return "DONE";
    }
}
struct jobList *changeJobSign(struct jobList **root)
{
    struct jobList *temp = *root;
    temp->jobSign = "-";
    return temp;
}

void pushJob(struct jobList **root, int jobId, char *jobCommand, JobStatus jobStatus, int jobCount, struct processList *process)
{
    struct jobList *newJob = newNode(jobId, jobCommand, jobStatus, jobCount, process);

    if (*root == NULL)
    {
        newJob->next = NULL;
    }
    else
    {
        newJob->next = *root;
        // *root = changeJobSign(root);
    }
    *root = newJob;
    printf("\n%d pushed to stack\n", jobId);
}

struct jobList *popJob(struct jobList **root)
{
    if (root == NULL)
    {
        return NULL;
    }
    struct jobList *temp = *root;
    *root = (*root)->next;
    return temp;
}

void printJobs(struct jobList *root)
{

    if (root != NULL)
    {
        printJobs(root->next);
        printf("\n[%d]%s %s %s\n", root->jobCount, root->jobSign, getJobsStatus(root->jobStatus), root->jobCommand);
    }
}
struct jobList *deleteJobByStatus(struct jobList **headRef, JobStatus jobStatus)
{

    struct jobList *temp = *headRef, *prev;
    int jobid;
    if (temp != NULL && temp->jobStatus == jobStatus)
    {
        *headRef = temp->next;
        return temp;
    }

    while (temp != NULL && temp->jobStatus != jobStatus)
    {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL)
        return NULL;

    prev->next = temp->next;
    return temp;
}

struct jobList *deleteJobByPid(struct jobList **headRef, int pid)
{

    struct jobList *temp = *headRef, *prev;
    int jobid;
    if (temp != NULL && temp->jobId == pid)
    {
        *headRef = temp->next;
        if (temp->jobCount == globalJobNumber)
        {
            checkPrevJobCount(&temp);
        }
        return temp;
    }

    while (temp != NULL && temp->jobId != pid)
    {
        prev = temp;
        temp = temp->next;
    }

    if (temp == NULL)
        return NULL;

    prev->next = temp->next;

    return temp;
}

void changePrevJobSign(struct jobList **jobRef)
{
    struct jobList *curNode = *jobRef;
    struct jobList *prevNode = NULL;

    if (curNode != NULL)
    {
        curNode->jobSign = "+";
    }
    while (curNode->next != NULL)
    {

        curNode = curNode->next;
        curNode->jobSign = "-";
    }
}

void checkPrevJobCount(struct jobList **temp)
{
    struct jobList *prevNode = NULL;
    struct jobList *curNode = *temp;

    while (curNode != NULL)
    {
        prevNode = curNode;
        curNode = curNode->next;
    }
    
    globalJobNumber = prevNode->jobCount + 1;
}
void initshell()
{
    printf("Welcome to the new YetAnotherShell\n");

    signal(SIGCHLD, sigChildHandler);
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);

    pid_t pid = getpid();
    setpgid(pid, pid);
    tcsetpgrp(0, pid);
}

int checkIfShellCommands(char *inString)
{
    int isShellCommands = 1;
    if (strstr(inString, "bg") || strstr(inString, "fg") || strstr(inString, "jobs"))
    {
        isShellCommands = 0;
        return isShellCommands;
    }

    return isShellCommands;
}
char **parseStringStrtok(char *str, char *delim)
{
    char **parsedString = NULL;
    char *saveptr1;
    char *token = (char *)malloc(sizeof(char) * 1000);
    token = strtok_r(str, delim, &saveptr1);
    int index = 0;
    while (token != NULL)
    {
        parsedString = realloc(parsedString, sizeof(char *) * ++index);
        if (parsedString == NULL)
        {
            exit(-1);
        }

        parsedString[index - 1] = token;
        token = strtok_r(NULL, delim, &saveptr1);
    }
    return parsedString;
}

struct processList *parseSubCommand(char **subCommand)
{
    int index = 0;
    char *inputPath = NULL;
    char *outputPath = NULL;
    char **mainSubCommand = (char **)malloc(sizeof(char) * 100);
    int mainIndex = 0;
    int pos1 = -1;
    int pos2 = -1;

    index = 0;
    while (subCommand[index] != NULL)
    {
        if ((strstr(subCommand[index], "<") || strstr(subCommand[index], ">")) && (pos1 == -1))
        {
            pos1 = index;
            index++;
            continue;
        }
        if ((strstr(subCommand[index], "<") || strstr(subCommand[index], ">")) && (pos1 > 0) && (pos2 == -1))
        {
            pos2 = index;
            break;
        }
        index++;
    }

    if (pos2 > 0)
    {
        if (strstr(subCommand[pos1], "<"))
        {
            inputPath = (char *)malloc(100 * sizeof(char));
            inputPath = subCommand[pos1 + 1];
            index = 0;
            while (index < pos1)
            {
                mainSubCommand[mainIndex++] = subCommand[index];
                index++;
            }
        }
        else if (strstr(subCommand[pos2], "<"))
        {
            inputPath = (char *)malloc(100 * sizeof(char));
            inputPath = subCommand[pos2 + 1];
        }

        if (strstr(subCommand[pos1], ">"))
        {
            outputPath = (char *)malloc(100 * sizeof(char));
            outputPath = subCommand[pos1 + 1];
            index = 0;
            while (index < pos1)
            {
                mainSubCommand[mainIndex++] = subCommand[index];
                index++;
            }
        }
        else if (strstr(subCommand[pos2], ">"))
        {
            outputPath = (char *)malloc(100 * sizeof(char));
            outputPath = subCommand[pos1 + 1];
        }
    }
    else if ((pos2 < 0) && (pos1 > 0))
    {
        if (strstr(subCommand[pos1], "<"))
        {
            inputPath = (char *)malloc(100 * sizeof(char));
            inputPath = subCommand[pos1 + 1];
            index = 0;
            while (index < pos1)
            {
                mainSubCommand[mainIndex++] = subCommand[index];
                index++;
            }
        }
        else if (strstr(subCommand[pos1], ">"))
        {
            outputPath = (char *)malloc(100 * sizeof(char));
            outputPath = subCommand[pos1 + 1];
            index = 0;
            while (index < pos1)
            {
                mainSubCommand[mainIndex++] = subCommand[index];
                index++;
            }
        }
    }
    else if ((pos2 < 0) && (pos1 < 0))
    {
        index = 0;
        while (subCommand[index] != NULL)
        {
            mainSubCommand[mainIndex++] = subCommand[index];
            index++;
        }
    }
    mainSubCommand[index] = 0;

    struct processList *process =
        (struct processList *)
            malloc(sizeof(struct processList));

    process->inputPath = inputPath;
    process->outputPath = outputPath;
    process->processArgs = mainSubCommand;
    process->next = NULL;
    return process;
}

struct processList *parseStringforPipes(char **parsedCmdArray)
{

    char **lchild = (char **)malloc(sizeof(char) * 1000);
    char **rchild = (char **)malloc(sizeof(char) * 1000);
    // int lchildindex = 0;
    int index = 0;
    struct processList *parentProcess = NULL;
    struct processList *leftChildProcess = NULL;
    struct processList *rightChildProcess = NULL;

    int cmdArrayLength = 0;
    while (parsedCmdArray[cmdArrayLength] != NULL)
    {
        cmdArrayLength++;
    }

    while (parsedCmdArray[index] != NULL)
    {
        if (strstr(parsedCmdArray[index], "|"))
        {
            int lchildindex = 0;
            while (lchildindex < index)
            {
                lchild[lchildindex] = strdup(parsedCmdArray[lchildindex]);
                lchildindex++;
            }
            lchild[lchildindex] = NULL;
            int rchildindex = 0;
            int rindex = index + 1;
            while (rindex < cmdArrayLength)
            {
                rchild[rchildindex] = strdup(parsedCmdArray[rindex]);
                rchildindex++;
                rindex++;
            }
            rchild[rchildindex] = NULL;

            struct processList *leftChildProcess = parseSubCommand(lchild);
            struct processList *rightChildProcess = parseSubCommand(rchild);

            parentProcess = leftChildProcess;
            leftChildProcess->next = rightChildProcess;
            break;
        }
        index++;
    }

    if (parentProcess == NULL)
    {
        struct processList *process = parseSubCommand(parsedCmdArray);
        parentProcess = process;
    }

    return parentProcess;
}

int exexuteCommands(struct processList *rootProcess, int infd, int outfd, int job_type)
{

    int status = 0;

    cpid = fork();

    if (cpid < 0)
    {
        printf("fork problem");
        return -1;
    }

    if (cpid == 0)
    {
        // child process

        // signal(SIGINT, SIG_DFL);

        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
        signal(SIGCHLD, SIG_DFL);

        rootProcess->cpid = getpid();
        if (rootProcess->groupId > 0)
        {
            setpgid(0, rootProcess->groupId);
        }
        else
        {
            rootProcess->groupId = rootProcess->cpid;
            setpgid(0, rootProcess->groupId);
        }

        if (infd != 0)
        {

            dup2(infd, STDIN_FILENO);

            close(infd);
        }
        if (outfd != 1)
        {

            dup2(outfd, STDOUT_FILENO);

            close(outfd);
        }

        if (execvp(rootProcess->processArgs[0], rootProcess->processArgs) < 0)
        {
            exit(0);
        }
        exit(0);
    }
    else
    {

        // parent process

        rootProcess->cpid = cpid;
        if (rootProcess->groupId > 0)
        {
            setpgid(cpid, rootProcess->groupId);
        }
        else
        {
            rootProcess->groupId = rootProcess->cpid;
            setpgid(cpid, rootProcess->cpid);
        }

        if (job_type == fg)
        {
            tcsetpgrp(0, rootProcess->groupId);

            int waitid = -1;
            waitid = waitpid(-rootProcess->groupId, &wstatus, WUNTRACED);

            if (waitid == -1)
            {
                perror("waitpid");
                exit(EXIT_FAILURE);
            }
            if (WIFEXITED(status))
            {
            }
            else if (WIFSIGNALED(wstatus))
            {
                printf("killed by signal %d\n", WTERMSIG(wstatus));
            }
            else if (WSTOPSIG(wstatus))
            {
                // printf("killed by WSTOPSIG ");
                pushJob(&rootJob, rootProcess->cpid, rootProcess->processString, STOPPED, ++globalJobNumber, rootProcess);
            }
            waitid = status;

            signal(SIGTTOU, SIG_IGN);
            tcsetpgrp(0, getpid());
            signal(SIGTTOU, SIG_DFL);
        }
        else if (job_type == bg)
        {
            printf("\npush bg jobs\n");
            pushJob(&rootJob, rootProcess->cpid, rootProcess->processString, RUNNING, ++globalJobNumber, rootProcess);
        }
    }
    return status;
}
int executeParsedCommand(struct processList *rootProcess, int job_type)
{
    struct processList *proc;
    int status = 0;
    int pipefd[2];
    int infd = 0;
    int outfd = 1;
    pipe(pipefd);
    while (rootProcess != NULL)
    {
        if (rootProcess->inputPath != NULL)
        {
            if ((infd = open(rootProcess->inputPath, O_RDONLY)) < 0)
            {
                fprintf(stderr, "error opening file\n");
            }
        }

        if (rootProcess->outputPath != NULL)
        {
            outfd = 1;
            outfd = open(rootProcess->outputPath, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
            if (outfd < 0)
            {
                outfd = 1;
            }
            pipefd[1] = outfd;
        }

        if (rootProcess->next == NULL)
        {
            status = exexuteCommands(rootProcess, infd, outfd, job_type);
        }
        else if (rootProcess->next != NULL)
        {

            status = exexuteCommands(rootProcess, infd, pipefd[1], 2);
            close(pipefd[1]);
            infd = pipefd[0];
        }

        rootProcess = rootProcess->next;
    }

    return status;
}

void executeShellCommands(char *inString)
{
    char *command = strdup(inString);
    if (strstr(inString, "fg"))
    {

        printf("\nin fg\n");

        struct jobList *jobObj =
            (struct jobList *)
                malloc(sizeof(struct jobList));

        jobObj = popJob(&rootJob);
        pid_t pid;
        pid = jobObj->process->groupId;

        usleep(100);
        if (jobObj->jobStatus != RUNNING)
        {
            printf("[%d] %s %s\n", jobObj->jobCount, "continued", jobObj->jobCommand);
            kill(-pid, SIGCONT);
        }
        else
        {
            printf("[%d] %s %s\n", jobObj->jobCount, getJobsStatus(RUNNING), jobObj->jobCommand);
        }
        tcsetpgrp(0, pid);

        int status = 0;

        waitpid(pid, &status, WUNTRACED);

        if (WIFEXITED(status))
        {
            printf("\nin WIFEXITED");
        }
        else if (WIFSIGNALED(status))
        {

            printf("\nin WIFSIGNALED");
        }
        else if (WSTOPSIG(status))
        {
            printf("\nin WSTOPSIG");
            pushJob(&rootJob, jobObj->process->cpid, jobObj->jobCommand, STOPPED, jobObj->jobCount, jobObj->process);
            printf("[%d] %s %s\n", jobObj->jobCount, getJobsStatus(STOPPED), jobObj->jobCommand);
        }

        signal(SIGTTOU, SIG_IGN);
        tcsetpgrp(0, getpid());
        signal(SIGTTOU, SIG_DFL);
    }
    else if (strstr(inString, "bg"))
    {
        printf("\ninside bg");

        struct jobList *jobObj =
            (struct jobList *)
                malloc(sizeof(struct jobList));

        jobObj = deleteJobByStatus(&rootJob, STOPPED);

        if (jobObj != NULL)
        {
            int wpid = -1;
            pid_t pid;
            pid = jobObj->process->groupId;
            printf("pid %d", pid);
            kill(-pid, SIGCONT);
            sleep(1);
            wpid = waitpid(-1, &wstatus, WUNTRACED | WCONTINUED);
            if (WIFCONTINUED(wstatus))
            {
                printf("\ncontinuing..");
                pushJob(&rootJob, jobObj->process->cpid, jobObj->jobCommand, RUNNING, jobObj->jobCount, jobObj->process);
                printf("[%d] %s %s\n", jobObj->jobCount, getJobsStatus(RUNNING), jobObj->jobCommand);
            }
        }
    }
    else if (strstr(inString, "jobs"))
    {
        printf("\njobs list");
        if (!isEmpty(rootJob))
        {
            changePrevJobSign(&rootJob);
            printJobs(rootJob);
        }
        else
        {
            globalJobNumber = 0;
        }
    }
}
char **parsecommands(char *inString)
{

    char *command = strdup(inString);
    int job_type = fg;
    char **parsedCommandsArray;
    int status = 0;

    if (inString[strlen(inString) - 1] == '&')
    {
        job_type = bg;
        inString[strlen(inString) - 1] = '\0';
    }

    if (checkIfShellCommands(inString) == 0)
    {
        printf("\nprocessing shell commands\n");
        executeShellCommands(inString);
    }
    else
    {
        parsedCommandsArray = parseStringStrtok(inString, " ");
        int index = 0;
        while (parsedCommandsArray[index] != NULL)
        {
            index++;
        }
        parsedCommandsArray[index] = 0;

        rootProcess = parseStringforPipes(parsedCommandsArray);
        rootProcess->processString = command;
        // printList(rootProcess);

        status = executeParsedCommand(rootProcess, job_type);
    }
    return parsedCommandsArray;
}

void cmdprompt()
{
    char *inString = (char *)malloc(sizeof(char) * 1000);
    // inString = strdup("./infinite &");
    // parsecommands(inString);
    while ((inString = readline("shell #:")))
    {
        // // printf("shell #:");
        // inString = readline("\nshell #:");
        if (strlen(inString) != 0)
        {
            parsecommands(inString);
        }
    }
}

int main()
{

    initshell();
    cmdprompt();
}