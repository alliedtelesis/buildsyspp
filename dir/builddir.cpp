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

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

BuildDir::BuildDir(Package * P)
{
	char *gname = strdup(P->getNS()->getName().c_str());
	char *pname = strdup(P->getName().c_str());
	char *pwd = strdup(P->getWorld()->getWorkingDir()->c_str());
	this->WORLD = P->getWorld();

	int res = mkdir("output", 0700);
	if((res < 0) && (errno != EEXIST)) {
		throw DirException("output", strerror(errno));
	}

	std::string path = string_format("output/%s", gname);
	res = mkdir(path.c_str(), 0700);
	if((res < 0) && (errno != EEXIST)) {
		throw DirException(path.c_str(), strerror(errno));
	}

	path = string_format("output/%s/staging", gname);
	res = mkdir(path.c_str(), 0700);
	if((res < 0) && (errno != EEXIST)) {
		throw DirException(path.c_str(), strerror(errno));
	}

	path = string_format("output/%s/install", gname);
	res = mkdir(path.c_str(), 0700);
	if((res < 0) && (errno != EEXIST)) {
		throw DirException(path.c_str(), strerror(errno));
	}

	{
		const char *tmp = strrchr(pname, '/');
		if(tmp != NULL) {
			char *subpart = strdup(pname);
			subpart[tmp - pname] = '\0';
			path = string_format("mkdir -p output/%s/staging/%s", gname,
					     subpart);
			system(path.c_str());
			path = string_format("mkdir -p output/%s/install/%s", gname,
					     subpart);
			system(path.c_str());
			free(subpart);
		}
	}
	path = string_format("mkdir -p output/%s/%s", gname, pname);
	res = system(path.c_str());
	path = string_format("%s/output/%s/%s/work", pwd, gname, pname);
	res = mkdir(path.c_str(), 0700);
	if((res < 0) && (errno != EEXIST)) {
		throw DirException(path.c_str(), strerror(errno));
	}
	this->path = path;
	path = string_format("output/%s/%s/work", gname, pname);
	this->rpath = path;
	path = string_format("%s/output/%s/%s/new", pwd, gname, pname);
	res = mkdir(path.c_str(), 0700);
	if((res < 0) && (errno != EEXIST)) {
		throw DirException(path.c_str(), strerror(errno));
	}
	this->new_path = path;
	path = string_format("%s/output/%s/%s/staging", pwd, gname, pname);
	res = mkdir(path.c_str(), 0700);
	if(res < 0) {
		if(errno != EEXIST) {
			throw DirException(path.c_str(), strerror(errno));

		}
	}
	this->staging = path;

	path = string_format("%s/output/%s/%s/new/staging", pwd, gname, pname);
	res = mkdir(path.c_str(), 0700);
	if(res < 0) {
		if(errno != EEXIST) {
			throw DirException(path.c_str(), strerror(errno));

		}
	}
	this->new_staging = path;
	path = string_format("%s/output/%s/%s/new/install", pwd, gname, pname);
	res = mkdir(path.c_str(), 0700);
	if(res < 0) {
		if(errno != EEXIST) {
			throw DirException(path.c_str(), strerror(errno));

		}
	}
	this->new_install = path;

	free(gname);
	free(pname);
	free(pwd);
}

void BuildDir::clean()
{
	std::string cmd = string_format("rm -fr %s", this->path.c_str());
	system(cmd.c_str());

	int res = mkdir(this->path.c_str(), 0700);
	if(res < 0) {
		// We should complain here
	}
}

void BuildDir::cleanStaging()
{
	std::string cmd = string_format("rm -fr %s", this->staging.c_str());
	system(cmd.c_str());
}
