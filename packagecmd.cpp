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
		ssize_t res = read(fd, reinterpret_cast<void *>(&recv_byte), 1); // NOLINT
		if(res <= 0) {
			if(!recv_buf.empty()) {
				P->program_output(recv_buf);
			}
			break;
		}
		if(recv_byte == '\n') {
			recv_buf.push_back('\0');
			// Print the line, clear the string and start again
			P->program_output(recv_buf);
			recv_buf.clear();
		} else {
			recv_buf.push_back(recv_byte);
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

	execvpe(program.c_str(), const_cast<char *const *>(pargs.data()), // NOLINT
	        const_cast<char *const *>(penv.data()));                  // NOLINT
}

static int run(Package *P, const std::string &program, const std::vector<std::string> &argv,
               const std::string &path, const std::vector<std::string> &newenvp,
               bool log_output)
{
	std::vector<int> fds(2);
	if(log_output) {
		int res = pipe(&fds[0]);

		if(res != 0) {
			P->log("pipe() failed: " + std::string(strerror(errno)));
		}

		std::thread thr(pipe_data_thread, P, fds[0]);
		thr.detach();
	}
	// call the program ...
	int pid = fork();
	if(pid < 0) { // something bad happened ...
		P->log("fork() failed: " + std::string(strerror(errno)));
		exit(-1);
	} else if(pid == 0) { // child process
		if(log_output) {
			close(fds[0]);
			dup2(fds[1], STDOUT_FILENO);
			dup2(fds[1], STDERR_FILENO);
			close(fds[1]);
		}
		if(chdir(path.c_str()) != 0) {
			P->log(boost::format{"chdir '%1%' failed"} % path);
			exit(-1);
		}
		exec_process(program, argv, newenvp);
		P->log("Failed Running " + program);
		exit(-1);
	} else {
		if(log_output) {
			close(fds[1]);
		}
		int status = 0;
		waitpid(pid, &status, 0);
		// check return status ...
		if(WEXITSTATUS(status) < 0) { // NOLINT
			P->log(boost::format{"Error Running %1% (path = %2%, return code = %3%)"} %
			       program % path % status);
		}
		return WEXITSTATUS(status); // NOLINT
	}
	return -1;
}

bool PackageCmd::Run(Package *P)
{
	std::vector<std::string> ne;

	// collect the current enviroment
	size_t e = 0;
	while(environ[e] != nullptr) {   // NOLINT
		ne.emplace_back(environ[e]); // NOLINT
		e++;
	}

	// append any new enviroment to it
	for(auto &env : this->envp) {
		ne.push_back(env);
	}

	bool res = true;
	if(run(P, this->args[0], this->args, this->path, ne, this->log_output) != 0) {
		this->printCmd();
		res = false;
	}

	return res;
}

void PackageCmd::printCmd() const
{
	std::cout << "Path: " << this->path << std::endl;
	for(size_t i = 0; i < this->args.size(); i++) {
		std::cout << boost::format{"Arg[%1%] = '%2%'"} % i % this->args[i] << std::endl;
	}
}
