#include "parser.h"

char whitespace[] = " \t\r\n\v";

char symbols[] = "<|>&;";

pid_t backround_procs[MAX_BACKROUND_PROCS];
int backround_procs_count = 0;
// to keep track of background processes
pid_t current_fg_pid = -1; // global variable to track foreground process PID

// signal handler for SIGINT
void sigintHandler(int signum) 
{    
    if (current_fg_pid > 0) 
    {
        // a foreground process is running
        kill(current_fg_pid, SIGINT); // send SIGINT to the foreground process
        printf("\n\n");
    } 
    else 
    {
        // no foreground process is running, do nothing
        printf("\n");
        printf("\nCtrl-C catched. But currently there is no foreground process running.\n");
        printf("\n");
        printf("myshell> "); // print the shell prompt again
    }
}

/*
    Main function to take the given command and run it in the shell/child
    Paramter: takes in cmd structure

        1 - Creates a child process which will act as the shell 
        
        2 - Within shell/child, switch cases depending on what type of command it is, each type correlates to one of the parts of the project
            - Look at parser.h to see the make-up of each type of command, will prob be helpful when calling exec functions and what-not
            - Child does whatever it does, then calls exit(), exit is 0 if successful, and non zero if an error occured

        3 - Main process/parent then calls wait() to reap the child, and output the error code of the child if it was not 0
*/
void shellCommandExecuter(struct cmd *cmd)
{
    if(cmd->type == BACK || cmd->type == LIST)
    {
        executeCommandHelper(cmd);
    }
    else
    {
        pid_t shellfork = fork(); // create a child process for foreground commands

        if(shellfork == 0) // child process
        {
            executeCommandHelper(cmd); // execute the command
            exit(0); // exit child

        }
        else if(shellfork > 0) // parent process 
        {
            int status; 
            current_fg_pid = shellfork; // to track the foreground process
            waitpid(shellfork, &status, 0); // reap the child process
            current_fg_pid = -1; // reset after reaping the child process

            if (WIFSIGNALED(status)) 
            {
                int sig = WTERMSIG(status);
                if (sig != SIGINT) // skip printing for SIGINT
                {
                    printf("Child terminated by signal %d (%s).\n", sig, strsignal(sig));
                }
            }
            else if (WIFEXITED(status)) 
            {
                int exitCode = WEXITSTATUS(status);
                if (exitCode != 0) 
                {
                    printf("Non-zero exit code (%d) detected.\n", exitCode);
                }
            }
            backReaper();
        }
        else // fork failed
        {
            perror("fork failed");
            exit(3);
        }
    }
}

// helper function to execute commands
void executeCommandHelper(struct cmd *cmd)
{
    if(!cmd)
    {
        fprintf(stderr, "Error: NULL command\n");
        exit(1);
    }

    switch(cmd->type)
    {
        case EXEC:
            commandExec((struct execcmd *)cmd);
            break;

        case REDIR:
            commandRedir((struct redircmd *)cmd);
            break;

        case PIPE:
            commandPipe((struct pipecmd *)cmd);
            break;

        case LIST:
            commandList((struct listcmd *)cmd);
            break;

        case BACK:
            commandBack((struct backcmd *)cmd);
            break;

        default:
            fprintf(stderr, "Unknown command type\n");
            exit(1);
    }

    return;
}

// to execute EXEC commands
void commandExec(struct execcmd *execcmd)
{
    if(execcmd->argv[0] == NULL)
    {
        fprintf(stderr, "Error: Empty command\n");
        exit(1);
    }
    
    // prevent opening another shell within the shell
    if (strcmp(execcmd->argv[0], "./myshell") == 0)
    {
        fprintf(stderr, "Error: Cannot open another myshell\n");
        exit(1);
    }

    // run the command
    if (execvp(execcmd->argv[0], execcmd->argv) == -1) 
    { 
        switch (errno)
        {
            case ENOENT: // command not found
                fprintf(stderr, "bash: %s: No such file or directory\n", execcmd->argv[0]);
                exit(127);
                break;

            case EACCES: // permission denied
                fprintf(stderr, "bash: %s: Permission denied\n", execcmd->argv[0]);
                exit(126);
                break;

            case ENOEXEC: // not an executable
                fprintf(stderr, "bash: %s: Exec format error\n", execcmd->argv[0]);
                exit(126);
                break;

            default: // other errors
                perror("Execvp failed");
                exit(1);
        }
    }
}

void backReaper()
{

    for(int i = 0; i < backround_procs_count; i++)
    {
        pid_t bg_pid = backround_procs[i];
        int bg_status;
        pid_t result = waitpid(bg_pid,&bg_status, WNOHANG);
        
        if(result == 0)
        {
            continue;
        }
    
        if(result == bg_pid)
        {
	  
            for(int j = i; j < backround_procs_count - 1; j++)
            {
   
                backround_procs[j] = backround_procs[j + 1];
	
            }
            
            backround_procs_count--;
            i--;
        }
        else
        {
            printf(" \n waitpid failed \n");
        }
    }
}
void commandBack(struct backcmd *backcmd)
{
    pid_t pid = fork();

    if (pid == 0)  // child process
    {
        // execute the background command
        executeCommandHelper(backcmd->cmd);
        exit(0);
    }
    else if (pid > 0)  // parent
    {
        if(backround_procs_count < MAX_BACKROUND_PROCS)
        {
            backround_procs[backround_procs_count++] = pid;
        }
        else
        {
            printf("\n too many backround procs \n");
        }
        
        return;
    }
    
    else  // fork failed
    {
        perror("fork failed");
        exit(1); 
    }
}

void commandList(struct listcmd *listcmd)
{
    if(listcmd->left->type == BACK ||listcmd->left->type == LIST)
    {
        executeCommandHelper(listcmd->left);
    }
    else
    {
        pid_t left_pid = fork();
        if(left_pid == 0)
        {
            executeCommandHelper(listcmd->left);
            exit(0);
        }

        waitpid(left_pid,NULL,0);
        backReaper();
    }

    if(listcmd->right->type == BACK || listcmd->right->type == LIST)
    {
        executeCommandHelper(listcmd->right);
    }
    else
    {
        pid_t right_pid = fork();

    if(right_pid == 0)
    {
        executeCommandHelper(listcmd->right);
        exit(0);
    }

        waitpid(right_pid,NULL,0);
        backReaper();
    }

    return;
}

void commandRedir(struct redircmd *redircmd)
{
    int fd = open(redircmd->file,redircmd->mode,0644); //0644 is saying read and write)
    
    if(fd < 0)
    {
        exit(1);
    }
    
    dup2(fd,redircmd->fd_to_close);
    close(fd);
    executeCommandHelper(redircmd->cmd);
    
    return;
 } 
 
void commandPipe(struct pipecmd *pipecmd)
{
    int pipefds[2];
    
    if(pipe(pipefds) < 0) 
    {
        printf("\n pipe failed \n" );
        exit(1);
    }

    pid_t left_pid = fork();
    if(left_pid == 0)
    {
        close(pipefds[0]); //close read
        dup2(pipefds[1], STDOUT_FILENO); // redirect STDOUT
        close(pipefds[1]); // close write
        executeCommandHelper(pipecmd->left);
        exit(0);
    }
    
    pid_t right_pid = fork();
    if(right_pid == 0)
    {
        close(pipefds[1]); //close write
        dup2(pipefds[0], STDIN_FILENO) ;
        close(pipefds[0]); // close read
        executeCommandHelper(pipecmd->right);
        exit(0);
    }
    
    close(pipefds[0]);
    close(pipefds[1]);
    waitpid(left_pid,NULL,0);
    waitpid(right_pid,NULL,0);
    
    return;
    }


/*
    The peak() function 
        Skips whitespace and checks for specific symbols in the command string.
        removes leading whitespace chars and 
        checks if the first non-whitespace char is in the string toks or not.
    -----
    ps: start of the string
    es: end of the string
    toks: string to check

    char *strchr(const char *s, int c):
        The strchr() function returns a pointer to the first occurrence of
        the character c in the string s.
 */
int peek(char **ps, char *es, char *toks)
{
    char *s;

    s = *ps;
    while(s < es && strchr(whitespace, *s)) // skipping the leading whitespace chars
    {
        s++;
    }
    
    *ps = s;

    return *s && strchr(toks, *s);
}

char* gets(char *buf, int max)
{
    int i, cc;
    char c;

    for(i=0; i+1 < max;)
    {
        cc = read(0, &c, 1);
        if(cc < 1) break;
            buf[i++] = c;

    if(c == '\n' || c == '\r') 
        break;
    }

    buf[i] = '\0';
    return buf;
}

int getcmd(char *buf, int nbuf)
{
    MSG("%s", SHELL_PROMPT);
    memset(buf, 0, nbuf);
    gets(buf, nbuf);
    if(buf[0] == 0) // EOF
    {
        return -1;
    }
    
    return 0;
}

/*
Entry point for parsing a command string, 
invoking parseline to process the command line.
*/ 
struct cmd* parsecmd(char *s)
{
    char *es;
    struct cmd *cmd;

    es = s + strlen(s);
    cmd = parseline(&s, es);
    
    peek(&s, es, ""); // remove trailing whitespace chars from the command line

    if(s != es){
        MSG("leftovers: %s\n", s);
        PANIC("syntax\n");
    }
    
    nulterminate(cmd);

    return cmd;
}

struct cmd* parseline(char **ps, char *es)
{
    struct cmd *cmd;

    cmd = parsepipe(ps, es);

    while(peek(ps, es, "&")){
        gettoken(ps, es, 0, 0);
        cmd = init_backcmd(cmd);
    }
    
    if(peek(ps, es, ";")){
        gettoken(ps, es, 0, 0);
        cmd = init_listcmd(cmd, parseline(ps, es));
    }
    
    return cmd;
}

struct cmd* parsepipe(char **ps, char *es)
{
    struct cmd *cmd;

    cmd = parseexec(ps, es);

    if(peek(ps, es, "|"))
    {
        gettoken(ps, es, 0, 0);
        cmd = init_pipecmd(cmd, parsepipe(ps, es));
    }
    
    return cmd;
}

struct cmd* parseexec(char **ps, char *es)
{
    char *q, *eq;
    int tok, argc;
    struct execcmd *cmd;
    struct cmd *ret;

    // Allocate space for the exec command
    ret = init_execcmd();
    cmd = (struct execcmd*)ret;

    argc = 0;
    ret = parseredirs(ret, ps, es);
    
    while(!peek(ps, es, "|)&;"))
    {
        if((tok=gettoken(ps, es, &q, &eq)) == 0)
            break;
        
        if(tok != 'a')
        {
            PANIC("syntax\n");
        }

        if (0 == argc && *q == 'e' && *(q+1) == 'x' && *(q+2) == 'i' && *(q+3) == 't' && q+4 == eq)
        {
            exit(0);
        }
        
        cmd->argv[argc] = q;
        cmd->eargv[argc] = eq;
        argc++;

        if(argc >= MAXARGS)
        {
            PANIC("too many args\n");
        }
        
        ret = parseredirs(ret, ps, es);
    }

    cmd->argv[argc] = 0;
    cmd->eargv[argc] = 0;

    return ret;
}

struct cmd* parseredirs(struct cmd *cmd, char **ps, char *es)
{
    int tok;
    char *q, *eq;

    while(peek(ps, es, "<>")){
        tok = gettoken(ps, es, 0, 0);
        
        if(gettoken(ps, es, &q, &eq) != 'a')
        {
            PANIC("missing file for redirection\n");
        }
        
        switch(tok){
            case '<':
                cmd = init_redircmd(cmd, q, eq, O_RDONLY, 0);
                break;

            case '>':
                cmd = init_redircmd(cmd, q, eq, O_WRONLY|O_CREAT, 1);
                break;

            case '+':  // >>
                cmd = init_redircmd(cmd, q, eq, O_WRONLY|O_CREAT, 1);
                break;
        }
    }
    
    return cmd;
}


// normal command
struct cmd* init_execcmd(void)
{
    struct execcmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = EXEC;

    return (struct cmd*)cmd;
}

// redirection (">", "<")
struct cmd* init_redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd_to_close)
{
    struct redircmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = REDIR;
    cmd->cmd = subcmd;
    cmd->file = file;
    cmd->efile = efile;
    cmd->mode = mode;
    cmd->fd_to_close = fd_to_close;
    
    return (struct cmd*)cmd;
}

// pipe ("|")
struct cmd* init_pipecmd(struct cmd *left, struct cmd *right)
{
    struct pipecmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = PIPE;
    cmd->left = left;
    cmd->right = right;

    return (struct cmd*)cmd;
}

// background execution ("&")
struct cmd* init_backcmd(struct cmd *subcmd)
{
    struct backcmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = BACK;
    cmd->cmd = subcmd;
    return (struct cmd*)cmd;
}

// multiple separate commands in one line (";")
struct cmd* init_listcmd(struct cmd *left, struct cmd *right)
{
    struct listcmd *cmd;

    cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = LIST;
    cmd->left = left;
    cmd->right = right;

    return (struct cmd*)cmd;
}


int gettoken(char **ps, char *es, char **q, char **eq)
{
    char *s;
    int ret;

    s = *ps;
    while(s < es && strchr(whitespace, *s)) // remove the leading white space chars
    {
        s++;
    }
    
    if(q)
    {
        *q = s;
    }
    
    ret = *s;
    
    switch(*s){
        case 0:
            break;
            
        case '|':
        case ';':
        case '&':
        case '<':
            s++;
            break;

        case '>':
            s++;
            if(*s == '>')
            {
                ret = '+';
                s++;
            }
            break;

        default:
            ret = 'a';
            while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
            {
                s++;
            }
            break;
    }
    
    if(eq)
    {
        *eq = s;
    }
    
    while(s < es && strchr(whitespace, *s)) // remove the trailing white space chars
    {
        s++;
    }
    
    *ps = s;
    
    return ret;
}


// NUL-terminate all the counted strings.
struct cmd* nulterminate(struct cmd *cmd)
{
    int i;
    struct backcmd *bcmd;
    struct execcmd *ecmd;
    struct listcmd *lcmd;
    struct pipecmd *pcmd;
    struct redircmd *rcmd;

    if(cmd == NULL)
    {
        return NULL;
    }
    
    switch(cmd->type){
        case EXEC:
            ecmd = (struct execcmd*)cmd;
            for(i=0; ecmd->argv[i]; i++)
            *ecmd->eargv[i] = 0;
            break;

        case REDIR:
            rcmd = (struct redircmd*)cmd;
            nulterminate(rcmd->cmd);
            *rcmd->efile = 0;
            break;

        case PIPE:
            pcmd = (struct pipecmd*)cmd;
            nulterminate(pcmd->left);
            nulterminate(pcmd->right);
            break;

        case LIST:
            lcmd = (struct listcmd*)cmd;
            nulterminate(lcmd->left);
            nulterminate(lcmd->right);
            break;

        case BACK:
            bcmd = (struct backcmd*)cmd;
            nulterminate(bcmd->cmd);
            break;
    }
    
    return cmd;
}
