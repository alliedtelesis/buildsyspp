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

/**
 * Construct a Package Command
 * @param path - The path to run this command in.
 * @param app - The program to invoke.
 */
PackageCmd::PackageCmd(std::string _path, std::string _app)
    : path(std::move(_path)), app(std::move(_app))
{
	this->addArg(app);

	// collect the current environment
	size_t e = 0;
	while(environ[e] != nullptr) {           // NOLINT
		this->envp.emplace_back(environ[e]); // NOLINT
		e++;
	}
}

/**
 * The thread for printing the output of the command executed
 * via fork + exec.
 *
 * @param logger - The Logger to use to print the output.
 * @param fd - The file descriptor for the read end of the pipe used to
 *             get the output of the executed program.
 */
static void pipe_data_thread(Logger *logger, int fd)
{
	// get the data until there is a breakchar ...
	std::string recv_buf;
	char recv_byte;

	while(true) {
		ssize_t res = read(fd, reinterpret_cast<void *>(&recv_byte), 1); // NOLINT
		if(res <= 0) {
			if(!recv_buf.empty()) {
				logger->program_output(recv_buf);
			}
			break;
		}
		if(recv_byte == '\n') {
			recv_buf.push_back('\0');
			// Print the line, clear the string and start again
			logger->program_output(recv_buf);
			recv_buf.clear();
		} else {
			recv_buf.push_back(recv_byte);
		}
	}
}

/**
 * Execute the command via a new process.
 *
 * @param logger - The Logger to use to print any output.
 * @param pipe_fds - Pointer to a vector containing the file descriptors for a
 *                   pipe to write the command output to. If the vector is not
 *                   provided then the output of the command will not be captured
 *                   for logging purposes.
 */
int PackageCmd::exec_process(Logger *logger, const std::vector<int> *pipe_fds)
{
	int pid = fork();
	if(pid < 0) { // something bad happened ...
		logger->log("fork() failed: " + std::string(strerror(errno)));
		exit(-1);
	} else if(pid == 0) { // Child process
		if(pipe_fds != nullptr) {
			close(pipe_fds->at(0));
			dup2(pipe_fds->at(1), STDOUT_FILENO);
			dup2(pipe_fds->at(1), STDERR_FILENO);
			close(pipe_fds->at(1));
		}

		if(chdir(this->path.c_str()) != 0) {
			logger->log(boost::format{"chdir '%1%' failed"} % this->path);
			exit(-1);
		}

		std::vector<const char *> pargs(this->args.size());
		std::vector<const char *> penv(this->envp.size());

		std::transform(this->args.begin(), this->args.end(), pargs.begin(),
		               [](const std::string &s) { return s.c_str(); });
		pargs.push_back(nullptr);

		std::transform(this->envp.begin(), this->envp.end(), penv.begin(),
		               [](const std::string &s) { return s.c_str(); });
		penv.push_back(nullptr);

		execvpe(this->app.c_str(), const_cast<char *const *>(pargs.data()), // NOLINT
		        const_cast<char *const *>(penv.data()));                    // NOLINT

		// Should never be reached
		logger->log("Failed Running " + this->app);
		exit(-1);
	}

	// Parent process
	return pid;
}

/**
 * Run this command
 *
 * @param logger - The logger to log the command output with
 */
bool PackageCmd::Run(Logger *logger)
{
	int status = 0;

	if(this->log_output) {
		std::vector<int> pipe_fds(2);

		int res = pipe(&pipe_fds[0]);

		if(res != 0) {
			logger->log("pipe() failed: " + std::string(strerror(errno)));
		}

		int pid = this->exec_process(logger, &pipe_fds);

		close(pipe_fds[1]);
		std::thread thr(pipe_data_thread, logger, pipe_fds[0]);
		waitpid(pid, &status, 0);
		thr.join();
		close(pipe_fds[0]);
	} else {
		int pid = this->exec_process(logger, nullptr);
		waitpid(pid, &status, 0);
	}

	// check return status ...
	if(WEXITSTATUS(status) < 0) { // NOLINT
		logger->log(boost::format{"Error Running %1% (path = %2%, return code = %3%)"} %
		            this->app % this->path % status);
		this->printCmd(logger);
		return false;
	}

	return true;
}

/**
 * Print the details of the PackageCmd.
 *
 * @param logger - The Logger to print the details to.
 */
void PackageCmd::printCmd(Logger *logger) const
{
	logger->log("Path: " + this->path);
	for(size_t i = 0; i < this->args.size(); i++) {
		logger->log(boost::format{"Arg[%1%] = '%2%'"} % i % this->args[i]);
	}
}

/**
 * Disable logging the output of the command when it is executed.
 */
void PackageCmd::disableLogging()
{
	this->log_output = false;
}

/**
 * Add an enviroment variable to this command.
 *
 * @param env - The enviroment variable to append to this command.
 */
void PackageCmd::addEnv(const std::string &env)
{
	this->envp.push_back(env);
}

/**
 * Add an argument to this command.
 *
 * @param arg - The argument to append to this command.
 */
void PackageCmd::addArg(const std::string &arg)
{
	this->args.push_back(arg);
}

/**
 * Get the path configured for this command.
 *
 * @returns The path configured.
 */
const std::string &PackageCmd::getPath() const
{
	return this->path;
}

/**
 * Get the application name configured for this command.
 *
 * @returns The application name configured.
 */
const std::string &PackageCmd::getApp() const
{
	return this->app;
}

/**
 * Get the arguments configured for this command.
 *
 * @returns The arguments configured.
 */
const std::vector<std::string> &PackageCmd::getArgs() const
{
	return this->args;
}

/**
 * Get the environment configured for this command.
 *
 * @returns The environment configured.
 */
const std::vector<std::string> &PackageCmd::getEnvp() const
{
	return this->envp;
}

/**
 * Get whether logging the output is configured for this command.
 *
 * @returns Whether logging the output is configured.
 */
bool PackageCmd::getLogOutput() const
{
	return this->log_output;
}
