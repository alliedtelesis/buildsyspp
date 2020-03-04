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

#include "include/buildsys.h"

static void pipe_data(int fd, Package *P)
{
	if(P == nullptr) {
		return;
	}

	// get the data until there is a breakchar ...
	std::string recv_buf;
	char recv_byte;

	while(true) {
		ssize_t res = read(fd, reinterpret_cast<void *>(&recv_byte), 1);
		if(res <= 0) {
			if(!recv_buf.empty()) {
				program_output(P, recv_buf);
			}
			break;
		} else {
			if(recv_byte == '\n') {
				recv_buf.push_back('\0');
				// Print the line, clear the string and start again
				program_output(P, recv_buf);
				recv_buf.clear();
			} else {
				recv_buf.push_back(recv_byte);
			}
		}
	}
}

static void pipe_data_thread(Package *P, int fd)
{
	pipe_data(fd, P);
	close(fd);
}

static void exec_process(const std::string &program,
                         const std::vector<std::string> &args = {},
                         const std::vector<std::string> &env = {})
{
	std::vector<const char *> pargs(args.size());
	std::vector<const char *> penv(env.size());

	std::transform(args.begin(), args.end(), pargs.begin(),
	               [](const std::string &s) { return s.c_str(); });
	pargs.push_back(nullptr);

	std::transform(env.begin(), env.end(), penv.begin(),
	               [](const std::string &s) { return s.c_str(); });
	penv.push_back(nullptr);

	execvpe(program.c_str(), const_cast<char *const *>(pargs.data()),
	        const_cast<char *const *>(penv.data()));
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
