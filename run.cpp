#include <buildsys.h>

int buildsys::run(const char *package, char *program, char *argv[], const char *path, char *newenvp[])
{
#ifdef LOG_COMMANDS
	log(package, (char *)"Running %s", program); 
#endif
	// call the program ...
	int pid = fork();
	if(pid < 0)
	{ // something bad happened ...
		log(package, (char *)"fork() failed: %s", strerror(errno));
		exit(-1);
	}
	else if(pid == 0)
	{ // child process
		if(chdir(path) != 0)
		{
			log(package, (char *)"chdir '%s' failed", path);
			exit(-1);
		}
		if(newenvp != NULL)
			execvpe(program, argv, newenvp);
		else
			execvp(program, argv);
		log(package, (char *)"Failed Running %s", program); 
		exit(-1);
	}
	else
	{
		int status = 0;
		waitpid(pid, &status, 0);
		// check return status ...
		if(status < 0)
		{
			log(package, (char *)"Error Running %s (path = %s, return code = %i)", program, path, status); 
		}
		return status;
	}
	return -1;
}
