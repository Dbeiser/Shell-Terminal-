#ifndef _PARSER_H_
#define _PARSER_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
 
// Parsed command representation
#define EXEC  1 // normal command
#define REDIR 2 // redirection (">", "<")
#define PIPE  3 // pipe ("|")
#define LIST  4 // multimple separate commands in one line (";")
#define BACK  5 // background execution ("&")

#define MAXARGS 10

#define MAX_BACKROUND_PROCS 100
extern pid_t backround_procs[MAX_BACKROUND_PROCS];
extern int backround_procs_count;

#define PANIC(fmt, args...)                                                                 \
        do {                                                                                \
            fprintf(stderr, "[%d:%s()] "fmt, __LINE__, __FUNCTION__, ##args);               \
            exit(-1);                                                                       \
        } while(0)  

#define MSG(fmt, args...)                                                                   \
        fprintf(stderr, fmt, ##args)

#define SHELL_PROMPT "myshell> "

struct cmd {
  int type;
};

struct execcmd { // normal command
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd { // redirection (">", "<")
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd_to_close;
};

struct pipecmd { // pipe ("|")
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd { // multiple separate commands in one line (";")
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd { // background execution ("&")
  int type;
  struct cmd *cmd; 
};

void sigintHandler(int signum);
void shellCommandExecuter(struct cmd *cmd); // our function
void executeCommandHelper(struct cmd *cmd); // helper execute function
void commandExec(struct execcmd *execcmd);
void commandPipe(struct pipecmd *pipecmd); // 3.3 command pipeline
void commandList(struct listcmd *listcmd); // 3.2 multiple commands, same line
void commandRedir(struct redircmd *redircmd); // 3.4 input output redirection
void commandBack(struct backcmd *backcmd);// 3.5 back
void backReaper(void); // reaper for back command
int getcmd(char *buf, int nbuf);
int gettoken(char **ps, char *es, char **q, char **eq);

struct cmd* init_execcmd(void);
struct cmd* init_redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd);
struct cmd* init_pipecmd(struct cmd *left, struct cmd *right);
struct cmd* init_backcmd(struct cmd *subcmd);
struct cmd* init_listcmd(struct cmd *left, struct cmd *right);

struct cmd* parsecmd(char *s);
struct cmd* parseline(char **ps, char *es);
struct cmd* parsepipe(char **ps, char *es);
struct cmd* parseexec(char **ps, char *es);
struct cmd* parseredirs(struct cmd *cmd, char **ps, char *es);

struct cmd* nulterminate(struct cmd *cmd);

#endif // #ifndef _PARSER_H_
