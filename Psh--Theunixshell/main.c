/*
Functionalities implemented 
1.Displays prompt for typing command 
2.Basic command : ls pwd cd  quit exit echo clear
3.Non inbuilt commands using execvp system call 
4.Output redirection  commands 
5.Input redirection commands 
6.Input and outpur redirection commands
7.Pipe commands
8.Signal handling (ctrl+c) 
9.Support for history command 
10.Facility to recall previous commands using up arrow key
11.File autocomplete while typing commands
12.Print environment variables using echo command
13.Implemented our own commands:  $touch file_name.x -template and $vi file_name.x -template ; 
	 these command when given template as argument creates (in case of touch ) and open ( in case of vi) the file with the set template . 
*/

/********** Including Necessary Header Files **********/

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

/***********ANSI Colors****************************/
#define ANSI_COLOR_RED     "\x1b[31m"
#define ANSI_COLOR_RESET   "\x1b[0m"
#define ANSI_COLOR_GREEN "\x1b[92m"
#define ANSI_COLOR_BLUE "\x1b[94m"
#define ANSI_COLOR_DEF "\x1B[0m"
#define ANSI_COLOR_CYAN "\x1b[96m"

/********** Declaring Global variables **********/

int fd;
static char *args[512];   //contains token after strtok applied to input buffer with delimeter as space
static char prompt[512];  
char *history_file;     //used for storing history
char *input_buffer;    //used for reading input 
char *cmd_exec[512];   //contains token after strtok applied to input buffer with delimeter as '|'
int flag, len;
char cwd[1024];  //contains the result of getcwd() ;get current working directory
pid_t pid;
int environmment_flag;
int output_redirection, input_redirection;
int status;
char *input_redirection_file; //stores file name from which the input is redirected 
char *output_redirection_file;  //stores filename onto which the output is redirected

/********** Declaring Function Prototypes **********/

void clear_variables(); 
void print_history_list ();
void environment();
void set_environment_variables();
void change_directory();
char *skipwhite (char* );
void tokenize_by_space (char *);
void tokenize_redirect_input_output(char *);
void tokenize_redirect_input(char *);
char * tokenize_redirect_output(char *);
char *skip_double_quote(char* );
static int execute_inbuild_commands(char *, int, int, int);
void tokenize_by_pipe ();
static int execute_command(int, int, int, char *);
void shell_prompt(); 
void sigintHandler(int );
void check_for_bckgrnd ();
void echo_call(char *);
void present_dir();
void sigstopHandler(int );


/********** Function Definitions **********/

/*This function is the implementation of pwd command */
void present_dir(){
	char pwd[256];
	if (getcwd(pwd, sizeof(pwd)) == NULL)
      perror("getcwd() error");
    else
      printf("current working directory is: %s\n", pwd);
}
/*This function is used to get system environment variables */
void environment()
{
  int i =1, index=0;
  char env_val[1000], *value;
  while(args[1][i]!='\0')
              {
                   env_val[index]=args[1][i];
                   index++;
                    i++;
              }
  env_val[index]='\0';
  value=getenv(env_val);
  if(!value)
      printf("\n");
  else printf("%s\n", value);
}

/*This function is used to implement echo command*/
void echo_call(char *echo_val) {
  int i=0, index=0;
  environmment_flag=0;
  char new_args[1024],env_val[1000], *str[10];
  str[0]=strtok(echo_val," "); //delimeter is space,contains echo  
  str[1]=strtok(NULL, ""); //delimeter is noting ,contains evrything after echo 
  strcpy(env_val, args[1]);
  if(str[1]==NULL)
  {  printf("\n");
      return; 
   }
 
 if (strchr(str[1], '$')) 
                  {
                  environmment_flag=1;
                  }
  
  memset(new_args, '\0', sizeof(new_args));
  i=0; 
  
  while(str[1][i]!='\0')
    {
      if(str[1][i]=='"')
      {
      index=0;     
      while(str[1][i]!='\0')
          {
          if(str[1][i]!='"')
                {
                new_args[index]=str[1][i];
                 flag=1;
                index++;
                }
          i++;
          }             
      }
      else if(str[1][i]==39) // ' ascii value is 39
      {
      index=0;
      while(str[1][i]!='\0')
          {
          if(str[1][i]!=39)
                {
                new_args[index]=str[1][i];
                flag=1;
                index++;
                }
          i++;
          }               
      }
      else if(str[1][i]!='"')
        {
          new_args[index]=str[1][i];
          index++;
          i++;
        }
      else i++;    
    }
new_args[index]= '\n';
index++;
new_args[index]='\0';
if(environmment_flag==1)
	environment();
else
	printf("%s",new_args);

}
/* This function handles the interrupt signals ctrl-c*/

void sigintHandler(int sig_num) {
    write(STDOUT_FILENO,"\nAborted by signal Interrupt...\n",32);
    fflush(stdout);
    sleep(1);
    exit(0);
    return;

}

/* This function initializes the global variables */
void clear_variables() {
	fd = 0;
	flag = 0; // used for exit and quit
	len = 0;
	output_redirection = 0;
	input_redirection = 0;
	cwd[0] = '\0';  //used to store the current working directory 
	prompt[0] = '\0';
 	pid = 0;
	environmment_flag = 0;
}

/* This function prints the comand history when "history" command is given (ref http://www.math.utah.edu/docs/info/hist_2.html)*/
void print_history_list () {	
  	register HIST_ENTRY **the_list;
    register int i;

    the_list = history_list ();
    if (the_list)
    	for (i = 0; the_list[i]; i++)
            printf (ANSI_COLOR_GREEN"%d: %s\n"ANSI_COLOR_RESET, i + history_base, the_list[i]->line);   //history_base :The logical offset of the first entry in the history list.
    return;
}
  
/* This function is used to create the Shell Prompt */
void shell_prompt() {

	strcpy(prompt,"\x1b[31m");
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		strcat(prompt, "TheShell: ");
		strcat(prompt, cwd);
		strcat(prompt, "$ ");
		strcat(prompt, "\x1b[0m");
	}
	else {

		perror("Error in getting curent working directory: ");
	}
	return;
}

/* This function is used to skip the white spaces in the input string */

char *skipwhite (char* str) {

	int i = 0, j = 0;
	char *temp;
	if (NULL == (temp = (char *) malloc(sizeof(str)*sizeof(char)))) {
		perror("Memory Error: ");
		return NULL;
	}

	while(str[i++]) {

		if (str[i-1] != ' ')
			temp[j++] = str[i-1];
	}
	temp[j] = '\0';
	return temp;
}

/* This function is used to skip the double quote characters (") in the
   input string */

char * skip_double_quote (char *str) {

	int i = 0, j = 0;
	char *temp;
	if (NULL == (temp = (char *) malloc(sizeof(str)*sizeof(char)))) {
		perror("Memory Error: ");
		return NULL;
	}

	while(str[i++]) {

		if (str[i-1] != '"')
			temp[j++] = str[i-1];
	}
	temp[j] = '\0';
	return temp;
}

/* This function is used to change directory when "cd" command is 
   executed */

void change_directory() {
//	printf("change directory\n");
	char *home_dir = "/home";

	if ((args[1]==NULL) || (!(strcmp(args[1], "~") && strcmp(args[1], "~/"))))
		chdir(home_dir);
	else if (chdir(args[1]) < 0)
		perror("No such file or directory: ");

}

/* This function is used to execute the inbuild commands(cd ,exit,quit,history). It also calls 
   the "execute_command" function when the command to be executed doesn't
   fall under inbuild commands */

static int execute_inbuild_commands (char *cmd_exec, int input, int isfirst, int islast) {
	char *new_cmd_exec;
	new_cmd_exec = strdup(cmd_exec);   //The function strdup() is used to duplicate a string
	tokenize_by_space (cmd_exec);   //after tokenize by space ,stored in args[]

	//
	if (args[0] != NULL) {
		if (!(strcmp(args[0], "exit") && strcmp(args[0], "quit")))
			exit(0);

		if (!strcmp("cd", args[0])) {

			change_directory();
			return 1;
		}
		if(!strcmp("pwd",args[0])){
			present_dir();
			return 0;
		}
		if (!strcmp(args[0], "history")) {
			
			print_history_list();
			return 1;
		}
	}
		//
	if ((args[0] != NULL)&&(args[2]!=NULL)) {
		if (!(strcmp(args[0], "exit") && strcmp(args[0], "quit")))
			exit(0);

		if (!strcmp("cd", args[0])) {

			change_directory();
			return 1;
		}
		if(!strcmp("pwd",args[0])){
			present_dir();
			return 0;
		}
		if (!strcmp(args[0], "history")) {
			
			print_history_list();
			return 1;
		}

		/*for commands  touch file.x -template  vi file.x -template*/
		

		if((!strcmp("touch",args[0]) || !strcmp("vi",args[0])) && (!strcmp("-template",args[2]) )){
			pid_t tmpl_pid=fork();
			if(tmpl_pid == 0){
			long int len=strlen(args[1]);
			if(args[1][len-1]=='c'){
				char dir[100];
				getcwd(dir, sizeof(dir));
				strcat(dir,"/");
				strcat(dir,args[1]);
				char *cmd = "cp";
				char *argv[4];
				argv[0] = "cp";
				argv[1] = "/home/kay/Psh--Theunixshell/template_files/template.c";
				argv[2] = dir;
				argv[3] = NULL;
				execvp(cmd, argv);
			}
			else if((args[1][len-1]=='p') &&(args[1][len-2]=='p')&&(args[1][len-3]=='c')){
				char dir[100];
				getcwd(dir, sizeof(dir));
				strcat(dir,"/");
				strcat(dir,args[1]);
				char *cmd = "cp";
				char *argv[4];
				argv[0] = "cp";
				argv[1] = "/home/kay/Psh--Theunixshell/template_files/template.cpp";
				argv[2] = dir;
				argv[3] = NULL;
				execvp(cmd, argv);
				
			}
			else if((args[1][len-1]=='a') &&(args[1][len-2]=='v')&&(args[1][len-3]=='a')&&(args[1][len-4]=='j')){
				char dir[100];
				getcwd(dir, sizeof(dir));
				strcat(dir,"/");
				strcat(dir,args[1]);
				char *cmd = "cp";
				char *argv[4];
				argv[0] = "cp";
				argv[1] = "/home/kay/Psh--Theunixshell/template_files/template.java";
				argv[2] = dir;
				argv[3] = NULL;
				execvp(cmd, argv);
				
			}
			else if((args[1][len-1]=='s')&&(args[1][len-2]=='c')){
				char dir[100];
				getcwd(dir, sizeof(dir));
				strcat(dir,"/");
				strcat(dir,args[1]);
				char *cmd = "cp";
				char *argv[4];
				argv[0] = "cp";
				argv[1] = "/home/kay/Psh--Theunixshell/template_files/template.cs";
				argv[2] = dir;
				argv[3] = NULL;
				execvp(cmd, argv);
				
			}
			else{
				printf("template does not exist\n");
			}
			if(args[0]=="vi" ){
				args[2]=NULL;

			}
		exit(0);
	}


else{
	waitpid(tmpl_pid,0,0);
	if(!strcmp(args[0],"touch"))
		return 1;
	else if(!strcmp(args[0],"vi")){
				args[2]=NULL;

			}
}
}


	}

	return (execute_command(input, isfirst, islast, new_cmd_exec));
}

/* This function is used to parse the input when both input redirection 
   ["<"] and output redirection [">"] are present */

void tokenize_redirect_input_output (char *cmd_exec) {

	char *val[128];
	char *new_cmd_exec, *s1, *s2;
	new_cmd_exec = strdup(cmd_exec);

	int m = 1;
	val[0] = strtok(new_cmd_exec, "<");
	while ((val[m] = strtok(NULL,">")) != NULL) m++;

	s1 = strdup(val[1]);
	s2 = strdup(val[2]);

	input_redirection_file = skipwhite(s1);
	output_redirection_file = skipwhite(s2);

	tokenize_by_space (val[0]);
	return;
}

/* This function is used to parse the input when only input redirection
   ["<"] is present */

void tokenize_redirect_input (char *cmd_exec) {

	char *val[128];
	char *new_cmd_exec, *s1;
	new_cmd_exec = strdup(cmd_exec);

	int m = 1;
	val[0] = strtok(new_cmd_exec, "<");
	while ((val[m] = strtok(NULL,"<")) != NULL) m++;

	s1 = strdup(val[1]);
	input_redirection_file = skipwhite(s1);

	tokenize_by_space (val[0]);
	return;
}

/* This function is used to parse the input when only output redirection
   [">"] is present */

char * tokenize_redirect_output (char *cmd_exec) {

	char *val[128];
	char *new_cmd_exec, *s1;
	new_cmd_exec = strdup(cmd_exec);

	int m = 1;
	val[0] = strtok(new_cmd_exec, ">");
	while ((val[m] = strtok(NULL,">")) != NULL) m++;

	s1 = strdup(val[1]);
	output_redirection_file = skipwhite(s1); //name of the file 
	char * s2 =strdup(val[0]);
	return s2;
}

/* This function is used to create pipe and execute the non-inbuild  commands using execvp  */

static int execute_command (int input, int first, int last, char *cmd_exec) {
	

	int mypipefd[2], ret, input_fd, output_fd;
	char * val_0;
	int out_red_flag=0;
	if (-1 == (ret = pipe(mypipefd))) {
		perror("pipe error: ");
		return 1;
	}

	pid = fork();
	//child process

	if (pid == 0) {
		if (first == 1 && last == 0 && input == 0) { //first command of pipe
			dup2 (mypipefd[1], STDOUT_FILENO);  //all stdout will go to file whose file desc is mypipefd[1]
		}
		else if (first == 0 && last == 0 && input != 0) { //middle command of pipe 
			dup2 (input, STDIN_FILENO);  //all stdin will come from file whose file desc is input
			dup2 (mypipefd[1], STDOUT_FILENO); // all stdout will go to file whose file desc is mypipefd[1]
		}
		else { //last command of pipe
			dup2 (input, STDIN_FILENO); //all stdin will come from file whose file desc is input
		}
		/*The C library function char *strchr(const char *str, int c) searches for the first occurrence of the character c (an unsigned char) in the string pointed to by the argument str.*/
		if (strchr(cmd_exec, '<') && strchr(cmd_exec, '>')) {
			input_redirection = 1;
			output_redirection = 1;
			tokenize_redirect_input_output (cmd_exec);
		}
		else if (strchr(cmd_exec, '<')) {
			input_redirection = 1;
			tokenize_redirect_input (cmd_exec);
		}
		else if (strchr(cmd_exec, '>')) {
			output_redirection = 1;
			val_0= tokenize_redirect_output (cmd_exec);
		}
		
		if (output_redirection) {
			output_fd = creat(output_redirection_file, 0644);//creat only creates a file if it doesn't exist. If it already exists, it's just truncates.
			if (output_fd < 0) 
			{
				fprintf(stderr, "Failed to open %s for writing\n", output_redirection_file);
				return (EXIT_FAILURE);
			}
			dup2 (output_fd, 1);
			close (output_fd);
			output_redirection = 0;
			out_red_flag=1;
		}

		if (input_redirection) {

			if ((input_fd = open(input_redirection_file, O_RDONLY, 0)) < 0) {

				fprintf(stderr, "Failed to open %s for reading\n", input_redirection_file);
				return (EXIT_FAILURE);
			}
			dup2 (input_fd, 0);
			close (input_fd);
			input_redirection = 0;
		}
		

		if (!strcmp (args[0], "echo") ) {
			if(out_red_flag==1)
			 echo_call(val_0);
			else if(out_red_flag==0)
				echo_call(cmd_exec);

		}
		else {
			int i=execvp(args[0],args);
			if(i<0)
				fprintf(stderr,"%s: command not found\n",args[0]);

		} 
		exit(0);
	}
	//parent process
	else {

			waitpid(pid,0,0);
	}

	if (last == 1)
		close(mypipefd[0]);

	if (input != 0)
		close(input);

	close(mypipefd[1]);
	return (mypipefd[0]);
}

/* This function tokenizes the input string based on white-space [" "] */
void tokenize_by_space (char *str) {

	int m = 1;

	args[0] = strtok(str, " ");
	while ((args[m] = strtok(NULL," ")) != NULL) m++;
	args[m] = NULL;
}

/* This function tokenizes the input string based on pipe ["|"] */
void tokenize_by_pipe() {

	int i, n = 1, input = 0, first = 1,last =0;
   
	cmd_exec[0] = strtok(input_buffer, "|");   //Splits inputbuffer according to given delimiters (here '|') and returns the first token 
	while ((cmd_exec[n] = strtok(NULL, "|")) != NULL) n++;  //walk through other tokens

	cmd_exec[n] = NULL;
	for (i = 0; i < n-1; i++) {
		input = execute_inbuild_commands(cmd_exec[i], input, first, last);	   //execute_inbuild_commands : islast is 0
		first = 0;
	} 
	last=1;
	input = execute_inbuild_commands(cmd_exec[i], input, first, last);    //execute_inbuild_command :islast is 1 
	return;
}

/* Main function begins here */

int main() {

	system ("clear"); //clears the terminal  for unix/mac  
	char* descr = "           _.-''|''-._               Psh - The Unix Shell\n        .-'     |     `-.            Implemented by:\n      .'\\       |       /`.          Rohit Negi\n    .'   \\      |      /   `.        Kartikey Tyagi\n    \\     \\     |     /     /        Satyam Singh \n     `\\    \\    |    /    /'         Under The guidance of \n       `\\   \\   |   /   /'           Miss Sameeka Sanini  \n         `\\  \\  |  /  /'             \n        _.-`\\ \\ | / /'-._ \n       {_____`\\\\|//'_____}\n               `-'\n\n";

  printf("%s",descr);
	signal(SIGINT, sigintHandler);  //sigintHandler user defined handler which  handles interrupt SIGINT(ctrl+c)
	using_history();    //Begin a session in which the history functions might be used. This initializes the interactive variables. 

	do {

		clear_variables();  
		shell_prompt();   //initializes prompt
		input_buffer = readline (prompt); //readline will read a line from the terminal and return it, using  prompt as a prompt

		if(strcmp(input_buffer,"\n")) //strcmp returns 0 when both string equal 
			add_history (input_buffer);

		if (!(strcmp(input_buffer, "\n") && strcmp(input_buffer,"")))
			continue;

		if (!(strncmp(input_buffer, "exit", 4) && strncmp(input_buffer, "quit", 4))) {

			flag = 1;
			break;
		}
    //if input is not exit,quit,enter or space  then tokenize it by pipe
		tokenize_by_pipe();

	
			waitpid(pid,&status,0);  //waitpid — wait for a child process to stop or terminate pid-> child process's pid ,status-> info about terminated process,0->block mode

	} while(!WIFEXITED(status) || !WIFSIGNALED(status));

	if (flag == 1) {

		printf("\nThank You ! Closing Shell...\n");
		exit(0);
	}

	return 0;
}

