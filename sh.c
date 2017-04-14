#include "rc.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <readline/readline.h>

struct cmd_list {
	size_t count;
	struct cmd *first;
	struct cmd *last;
};

struct cmd {
	int fd_in;    // file descriptor to redirect input (if -1 the don't)
	int fd_out;   // file descriptor to redirect output (if -1 the don't)
	char **args;  // argument list
	struct cmd *prev;
	struct cmd *next; 
};

struct int_arr {
	size_t size;
	int val[];
};

static struct int_arr *sh_pipes;
static struct cmd_list *sh_cmds;

static int count_parts(char const *buf, char const * delim);

static struct cmd_list * create_cmd_list(char const *buf);
static void exec_cmd_list(struct cmd_list const * clist);
static void exec_cmd(struct cmd const * cmd);
static void delete_sh_cmds(void);

static struct int_arr * create_sh_pipes(int pipes_num);
static void close_sh_pipes(void);
static void delete_sh_pipes(void);
static void set_pipes(void);


int run_shell(void)
{
	char *input = NULL;
	while ( (input = readline("% ")) )
	{
		if (!strlen(input)) {
			free(input);
			continue;
		}
		
		int pipes_num = count_parts(input, "|") - 1;

		if (pipes_num > 0)
			sh_pipes = create_sh_pipes(pipes_num);

		if (pipes_num > 0 && !sh_pipes)
			return ERROR;

		sh_cmds  = create_cmd_list(input);
		if (!sh_cmds)
			return ERROR;

		if (sh_pipes && sh_cmds)
			set_pipes();

		exec_cmd_list(sh_cmds);

		if (sh_pipes) 
			close_sh_pipes();

		while(wait(NULL) > 0)
			;

		delete_sh_pipes();
		delete_sh_cmds();
		free(input);
	}

	return SUCCESS;
}

static struct cmd_list * create_cmd_list(char const *buf)
{
	struct cmd_list *cmdlist = calloc(1, sizeof(struct cmd_list));

	int cmd_num = count_parts(buf, "|");
	struct cmd *cmd_mem = calloc(cmd_num, sizeof(struct cmd));

	cmdlist->first = cmd_mem;
	cmdlist->count = cmd_num;
	int i = 0;
	for (i = 0; i < cmd_num; i++) {
		cmdlist->last = cmd_mem + i;

		if (i) {
			cmd_mem[i - 1].next = cmd_mem + i;
			cmd_mem[i].prev = cmd_mem + i - 1;
		}
	}

	char *str1, *str2;
	char *saveptr1, *saveptr2;

	for (i = 0, str1 = (char*)buf; ; i++, str1 = NULL) {
		char *token = strtok_r(str1, "|", &saveptr1);
		if (token == NULL)
			break;

		int arg_num = count_parts(token, " ");
		cmd_mem[i].args = calloc(arg_num + 1, sizeof(char **));

		int j = 0;
		for (j = 0, str2 = token; ; j++, str2 = NULL) {
			char *subtoken = strtok_r(str2, " ", &saveptr2);
			if (subtoken == NULL)
				break;

			cmd_mem[i].args[j] = strdup(subtoken);
		}
	}

	return cmdlist;
}

static int count_parts(char const *buf, const char* delim )
{
	char *buf_cp = strdup(buf);
	if (!buf || !buf_cp || *buf == '\0')
		return 0;

	strtok(buf_cp, delim);
	int ret = 1;
	for (ret = 1; strtok(NULL, delim) != NULL ; ret++) {
	}

	free(buf_cp);

	return ret;
}

static struct int_arr * create_sh_pipes(int pipes_num)
{
	if (pipes_num <= 0)
		return NULL;

	int fds_num = pipes_num * 2;

	struct int_arr *arr = malloc( sizeof(struct int_arr) 
	                             + sizeof(int) * fds_num);

	for(int i = 0; i < fds_num; i += 2) {
		if (pipe(arr->val + i)) 
			perror("pipe");
	}

	arr->size = fds_num;

	return  arr;
}

static void delete_sh_pipes(void)
{
	if (!sh_pipes)
		return;

	close_sh_pipes();
	free(sh_pipes); 
	sh_pipes = NULL;
}

static void close_sh_pipes(void)
{
	if (!sh_pipes)
		return;

	for (size_t i = 0; i < sh_pipes->size; i++)
		close(sh_pipes->val[i]);
}

static void set_pipes(void)
{
	if (!sh_pipes)
		return;

	int i = 0;
	for (struct cmd *cmd = sh_cmds->first; cmd; cmd = cmd->next) {
		cmd->fd_in = -1;
		cmd->fd_out = -1;

		if (sh_pipes) {
			if (cmd->next)
				cmd->fd_out = sh_pipes->val[i + 1];

			if (i != 0)
				cmd->fd_in = sh_pipes->val[i - 2];

			i += 2;
		}
	}
}

static void exec_cmd_list(struct cmd_list const * clist)
{
	for (struct cmd *cmd = clist->first; cmd; cmd = cmd->next)
		exec_cmd(cmd);
}

static void exec_cmd(struct cmd const * cmd)
{
	pid_t pid = fork();

	if (-1 == pid) {
		perror("fork");
		return;
	}

	if (pid)
		return;

	if (cmd->fd_in >= 0)
		dup2(cmd->fd_in, 0);

	if (cmd->fd_out >= 0)
		dup2(cmd->fd_out, 1);

	close_sh_pipes();
	execvp(cmd->args[0], cmd->args); 
	perror(cmd->args[0]);
	_exit(EXIT_FAILURE);
}

static void delete_sh_cmds(void)
{
	if (!sh_cmds)
		return;
	for (struct cmd *cmd = sh_cmds->first; cmd; cmd = cmd->next) {
		if (cmd->args) {
			char **args = cmd->args;
			while (*args) {
				free(*args);
				args++;
			}

			free(cmd->args);
		}
	}

	free(sh_cmds->first);
	free(sh_cmds);
	sh_cmds = NULL;
}

