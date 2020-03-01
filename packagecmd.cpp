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

bool PackageCmd::Run(Package *P)
{
	if(this->skip) {
		return true;
	}

	std::vector<std::string> ne;
	if(!this->envp.empty()) {
		// collect the current enviroment
		size_t e = 0;
		while(environ[e] != nullptr) {
			ne.emplace_back(environ[e]);
			e++;
		}
		// append any new enviroment to it
		for(auto &env : this->envp) {
			ne.push_back(env);
		}
	}

	bool res = true;
	if(run(P, this->args[0], this->args, this->path, ne) != 0) {
		res = false;
	}

	if(!res) {
		this->printCmd();
	}

	return res;
}

void PackageCmd::printCmd()
{
	printf("Path: %s\n", this->path.c_str());
	for(size_t i = 0; i < this->args.size(); i++) {
		printf("Arg[%zi] = '%s'\n", i, this->args[i].c_str());
	}
}
