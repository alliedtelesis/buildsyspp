/****************************************************************************************************************************
 Copyright 2013 Allied Telesis Labs Ltd. All rights reserved.
 
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the
distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************************************************************/

#include <buildsys.h>

#ifdef UNDERSCORE

static bool pipe_data(int fd, Package * P)
{
	if(!P)
		return false;

	// get the data until there is a breakchar ...
	char *cdata = NULL;
	size_t recvd = 0;

	while(1) {
		cdata = (char *) realloc(cdata, recvd + 2);
		ssize_t res = read(fd, (void *) ((unsigned long) cdata + recvd), 1);	// Yum Yum Yum
		if(res <= 0) {
			if(recvd == 0) {
				free(cdata);
				return false;
			}
			break;
		} else {
			recvd++;
			if(cdata[recvd - 1] == '\n') {
				cdata[recvd - 1] = '\0';
				break;
			}
		}
	}
	if(cdata != NULL) {
		if(recvd != 0) {
			program_output(P, cdata);
		}
		free(cdata);
	}
	return true;
}


static void *pipe_data_thread(us_thread * t)
{
	while(pipe_data(t->info, (Package *) t->priv)) {
		// do nothing
	}
	close(t->info);
	return NULL;
}
#endif

int buildsys::run(Package * P, char *program, char *argv[], const char *path,
		  char *newenvp[])
{
#ifdef LOG_COMMANDS
	log(P, (char *) "Running %s", program);
#endif

#ifdef UNDERSCORE
	int fds[2];
	if(WORLD->areOutputPrefix()) {
		int res = pipe(fds);

		if(res != 0) {
			log(P, (char *) "pipe() failed: %s", strerror(errno));
		}

		us_thread_create(pipe_data_thread, fds[0], P);
	}
#endif
	// call the program ...
	int pid = fork();
	if(pid < 0) {		// something bad happened ...
		log(P, (char *) "fork() failed: %s", strerror(errno));
		exit(-1);
	} else if(pid == 0) {	// child process
#ifdef UNDERSCORE
		if(WORLD->areOutputPrefix()) {
			close(fds[0]);
			dup2(fds[1], STDOUT_FILENO);
			dup2(fds[1], STDERR_FILENO);
			close(fds[1]);
		}
#endif
		if(chdir(path) != 0) {
			log(P, (char *) "chdir '%s' failed", path);
			exit(-1);
		}
		if(newenvp != NULL)
			execvpe(program, argv, newenvp);
		else
			execvp(program, argv);
		log(P, (char *) "Failed Running %s", program);
		exit(-1);
	} else {
#ifdef UNDERSCORE
		if(WORLD->areOutputPrefix()) {
			close(fds[1]);
		}
#endif
		int status = 0;
		waitpid(pid, &status, 0);
		// check return status ...
		if(WEXITSTATUS(status) < 0) {
			log(P, (char *) "Error Running %s (path = %s, return code = %i)",
			    program, path, status);
		}
		return WEXITSTATUS(status);
	}
	return -1;
}
