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

static bool pipe_data(int fd, Package *P)
{
	if(!P)
		return false;

	// get the data until there is a breakchar ...
	char *cdata = NULL;
	size_t recvd = 0;

	while(1) {
		cdata = (char *) realloc(cdata, recvd + 2);
		ssize_t res = read(fd, (void *) ((unsigned long) cdata + recvd), 1); // Yum Yum Yum
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

static void pipe_data_thread(Package *P, int fd)
{
	while(pipe_data(fd, P)) {
		// do nothing
	}
	close(fd);
}

static void exec_process(const std::string &program,
                         const std::vector<std::string> &args = {},
                         const std::vector<std::string> &env = {})
{
	using namespace std::placeholders;

	std::vector<const char *> pargs(args.size());
	std::vector<const char *> penv(env.size());

	std::transform(args.begin(), args.end(), pargs.begin(),
	               std::bind(&std::string::data, _1));
	pargs.push_back(nullptr);

	std::transform(env.begin(), env.end(), penv.begin(), std::bind(&std::string::data, _1));
	penv.push_back(nullptr);

	execvpe(program.c_str(), (char *const *) pargs.data(), (char *const *) penv.data());
}

int buildsys::run(Package *P, const std::string &program,
                  const std::vector<std::string> &argv, const std::string &path,
                  const std::vector<std::string> &newenvp)
{
#ifdef LOG_COMMANDS
	log(P, "Running %s", program);
#endif

	int fds[2];
	if(P->getWorld()->areOutputPrefix()) {
		int res = pipe(fds);

		if(res != 0) {
			log(P, "pipe() failed: %s", strerror(errno));
		}

		std::thread thr(pipe_data_thread, P, fds[0]);
		thr.detach();
	}
	// call the program ...
	int pid = fork();
	if(pid < 0) { // something bad happened ...
		log(P, "fork() failed: %s", strerror(errno));
		exit(-1);
	} else if(pid == 0) { // child process
		if(P->getWorld()->areOutputPrefix()) {
			close(fds[0]);
			dup2(fds[1], STDOUT_FILENO);
			dup2(fds[1], STDERR_FILENO);
			close(fds[1]);
		}
		if(chdir(path.c_str()) != 0) {
			log(P, "chdir '%s' failed", path.c_str());
			exit(-1);
		}
		exec_process(program, argv, newenvp);
		log(P, "Failed Running %s", program.c_str());
		exit(-1);
	} else {
		if(P->getWorld()->areOutputPrefix()) {
			close(fds[1]);
		}
		int status = 0;
		waitpid(pid, &status, 0);
		// check return status ...
		if(WEXITSTATUS(status) < 0) {
			log(P, "Error Running %s (path = %s, return code = %i)", program.c_str(),
			    path.c_str(), status);
		}
		return WEXITSTATUS(status);
	}
	return -1;
}
