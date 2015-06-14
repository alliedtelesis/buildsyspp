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

void PackageCmd::addArgFmt(const char *fmt, ...)
{
	char *message = NULL;
	va_list args;
	va_start(args, fmt);
	vasprintf(&message, fmt, args);
	va_end(args);

	this->addArg(message);

	free(message);
}

bool PackageCmd::Run(Package * P)
{
	if(this->skip)
		return true;

	char **ne = NULL;
	size_t ne_cnt = 0;
	if(this->envp != NULL) {
		// collect the current enviroment
		ne = (char **) calloc(1, sizeof(char *));
		size_t e = 0;
		while(environ[e] != NULL) {
			ne_cnt++;
			ne = (char **) realloc(ne, sizeof(char *) * (ne_cnt + 1));
			ne[ne_cnt - 1] = strdup(environ[e]);
			ne[ne_cnt] = NULL;
			e++;
		}
		// append any new enviroment to it
		for(e = 0; e < this->envp_count; e++) {
			ne_cnt++;
			ne = (char **) realloc(ne, sizeof(char *) * (ne_cnt + 1));
			ne[ne_cnt - 1] = strdup(this->envp[e]);
			ne[ne_cnt] = NULL;
		}
	}
	bool res = true;
	if(run(P, this->args[0], this->args, this->path, ne) != 0)
		res = false;

	if(ne != NULL) {
		for(int i = 0; ne[i] != NULL; i++) {
			free(ne[i]);
		}
		free(ne);
	}
	return res;
}

void PackageCmd::printCmd(const char *name)
{
	printf("Path: %s\n", this->path);
	for(size_t i = 0; i < this->arg_count; i++) {
		printf("Arg[%zi] = '%s'\n", i, this->args[i]);
	}
}

PackageCmd::~PackageCmd()
{
	if(this->path)
		free(this->path);
	if(this->app)
		free(this->app);
	if(this->args) {
		for(size_t i = 0; i < this->arg_count; i++) {
			free(this->args[i]);
		}
		free(this->args);
	}
	if(this->envp) {
		for(size_t i = 0; i < this->envp_count; i++) {
			free(this->envp[i]);
		}
		free(this->envp);
	}
}
