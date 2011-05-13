#include <buildsys.h>

int run(char *program, char *argv[], const char *path, char *newenvp[])
{
	// call the program ...
	int pid = fork();
	if(pid < 0)
	{ // something bad happened ...
		fprintf(stderr, "fork() failed: %s\n", strerror(errno));
		exit(-1);
	}
	else if(pid == 0)
	{ // child process
		if(chdir(path) != 0)
		{
			fprintf(stderr, "chdir '%s' failed\n", path);
			exit(-1);
		}
		if(newenvp != NULL)
			execvpe(program, argv, newenvp);
		else
			execvp(program, argv);
		fprintf(stderr, "Failed running %s\n", program);
		exit(-1);
	}
	else
	{
		int status = 0;
		waitpid(pid, &status, 0);
		// check return status ...
		if(status < 0)
		{
			fprintf(stderr, "Error running %s in %s\n", program, path);
		}
		return status;
	}
	return -1;
}
