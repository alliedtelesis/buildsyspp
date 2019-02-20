/******************************************************************************
 Copyright 2013 Allied Telesis Labs Ltd. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#include <buildsys.h>

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

struct params {
	Package *P;
	int fd;
};

static void *pipe_data_thread(void *t)
{
	struct params *p = (struct params *) t;
	while(pipe_data(p->fd, p->P)) {
		// do nothing
	}
	close(p->fd);
	free(p);
	return NULL;
}

int buildsys::run(Package * P, char *program, char *argv[], const char *path,
		  char *newenvp[])
{
#ifdef LOG_COMMANDS
	log(P, "Running %s", program);
#endif

	int fds[2];
	if(P->getWorld()->areOutputPrefix()) {
		int res = pipe(fds);
		struct params *p = (struct params *) malloc(sizeof(struct params));
		pthread_t tid;

		if(res != 0) {
			log(P, "pipe() failed: %s", strerror(errno));
		}
		p->fd = fds[0];
		p->P = P;
		pthread_create(&tid, NULL, pipe_data_thread, p);
		pthread_detach(tid);
	}
	// call the program ...
	int pid = fork();
	if(pid < 0) {		// something bad happened ...
		log(P, "fork() failed: %s", strerror(errno));
		exit(-1);
	} else if(pid == 0) {	// child process
		if(P->getWorld()->areOutputPrefix()) {
			close(fds[0]);
			dup2(fds[1], STDOUT_FILENO);
			dup2(fds[1], STDERR_FILENO);
			close(fds[1]);
		}
		if(chdir(path) != 0) {
			log(P, "chdir '%s' failed", path);
			exit(-1);
		}
		if(newenvp != NULL)
			execvpe(program, argv, newenvp);
		else
			execvp(program, argv);
		log(P, "Failed Running %s", program);
		exit(-1);
	} else {
		if(P->getWorld()->areOutputPrefix()) {
			close(fds[1]);
		}
		int status = 0;
		waitpid(pid, &status, 0);
		// check return status ...
		if(WEXITSTATUS(status) < 0) {
			log(P, "Error Running %s (path = %s, return code = %i)",
			    program, path, status);
		}
		return WEXITSTATUS(status);
	}
	return -1;
}
