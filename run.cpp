#include <buildsys.h>

#ifdef UNDERSCORE

static bool pipe_data(int fd, const char *pname)
{
	if(!pname) return false;
	
	
	// get the data until there is a breakchar ...
	char *cdata = NULL;
	size_t recvd = 0;
	// size_t length = 0;
	while(1)
	{
		cdata = (char *)realloc(cdata, recvd + 2);
		ssize_t res = read(fd, (void *)((unsigned long)cdata + recvd), 1); // Yum Yum Yum
		if(res <= 0)
		{
			if(recvd == 0)
			{
				free(cdata);
				return false;
			}
			break;
		}
		else
		{
			recvd++;
			if(cdata[recvd-1] == '\n')
			{
				// length = recvd;
				cdata[recvd-1] = '\0';
				break;
			}
		}
	}
	if(cdata != NULL)
	{
		if(recvd != 0)
		{
			program_output(pname, cdata);
		}
		free(cdata);
	}
	return true;
}


static void* pipe_data_thread(us_thread *t)
{
	while(pipe_data(t->info, (char *)t->priv))
	{
		// do nothing
	}
	close(t->info);
	free(t->priv);
	return NULL;
}
#endif

int buildsys::run(const char *package, char *program, char *argv[], const char *path, char *newenvp[])
{
#ifdef LOG_COMMANDS
	log(package, (char *)"Running %s", program);
	
#endif

#ifdef UNDERSCORE
	int fds[2];
	if(WORLD->areOutputPrefix())
	{
		int res = pipe(fds);
	
		if(res != 0)
		{
			log(package, (char *)"pipe() failed: %s", strerror(errno));
		}
	
		us_thread_create(pipe_data_thread, fds[0], strdup(package));
	}
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
#ifdef UNDERSCORE
		if(WORLD->areOutputPrefix())
		{
			close(fds[0]);
			dup2(fds[1], STDOUT_FILENO);
			dup2(fds[1], STDERR_FILENO);
			close(fds[1]);
		}
#endif
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
#ifdef UNDERSCORE
		if(WORLD->areOutputPrefix())
		{
			close(fds[1]);
		}
#endif
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
