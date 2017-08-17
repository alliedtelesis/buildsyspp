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
	char *pwd = strdup(WORLD->getWorkingDir()->c_str());

	int res = mkdir("output", 0700);
	if((res < 0) && (errno != EEXIST)) {
		throw DirException("output", strerror(errno));
	}

	char *path = NULL;
	if(asprintf(&path, "output/%s", gname) == -1) {
		fprintf(stderr, "output/global-name path: %s\n", strerror(errno));
	}
	res = mkdir(path, 0700);
	if((res < 0) && (errno != EEXIST)) {
		throw DirException(path, strerror(errno));
	}
	free(path);
	path = NULL;
	if(asprintf(&path, "output/%s/staging", gname) == -1) {
		fprintf(stderr, "Failed creating output/global-name/staging path: %s\n",
			strerror(errno));
	}
	res = mkdir(path, 0700);
	if((res < 0) && (errno != EEXIST)) {
		throw DirException(path, strerror(errno));
	}
	free(path);
	path = NULL;
	if(asprintf(&path, "output/%s/install", gname) == -1) {
		fprintf(stderr, "Failed creating output/global-name/install path: %s\n",
			strerror(errno));
	}
	res = mkdir(path, 0700);
	if((res < 0) && (errno != EEXIST)) {
		throw DirException(path, strerror(errno));
	}
	free(path);
	{
		const char *tmp = strrchr(pname, '/');
		if(tmp != NULL) {
			char *subpart = strdup(pname);
			subpart[tmp - pname] = '\0';
			asprintf(&path, "mkdir -p output/%s/staging/%s", gname, subpart);
			system(path);
			free(path);
			path = NULL;
			asprintf(&path, "mkdir -p output/%s/install/%s", gname, subpart);
			system(path);
			free(path);
			path = NULL;
			free(subpart);
		}
	}
	path = NULL;
	if(asprintf(&path, "mkdir -p output/%s/%s", gname, pname) == -1) {
		fprintf(stderr,
			"Failed creating output/global-name/package-name path: %s\n",
			strerror(errno));
	}
	res = system(path);
	free(path);
	path = NULL;
	if(asprintf(&path, "%s/output/%s/%s/work", pwd, gname, pname) == -1) {
		fprintf(stderr,
			"Failed creating output/global-name/package-name/work path: %s\n",
			strerror(errno));
	}
	res = mkdir(path, 0700);
	if((res < 0) && (errno != EEXIST)) {
		throw DirException(path, strerror(errno));
	}
	this->path = std::string(path);
	free(path);
	path = NULL;
	if(asprintf(&path, "output/%s/%s/work", gname, pname) == -1) {
		fprintf(stderr,
			"Failed creating output/global-name/package-name/work path: %s\n",
			strerror(errno));
	}
	this->rpath = std::string(path);
	free(path);
	path = NULL;
	if(asprintf(&path, "%s/output/%s/%s/new", pwd, gname, pname) == -1) {
		fprintf(stderr,
			"Failed creating output/global-name/package-name/new path: %s\n",
			strerror(errno));
	}
	res = mkdir(path, 0700);
	if((res < 0) && (errno != EEXIST)) {
		throw DirException(path, strerror(errno));
	}
	this->new_path = std::string(path);
	free(path);
	path = NULL;
	if(asprintf(&path, "%s/output/%s/%s/staging", pwd, gname, pname) == -1) {
		fprintf(stderr,
			"Failed creating output/global-name/package-name/staging path: %s\n",
			strerror(errno));
	}
	res = mkdir(path, 0700);
	if(res < 0) {
		if(errno != EEXIST) {
			throw DirException(path, strerror(errno));

		}
	}
	this->staging = std::string(path);
	free(path);
	path = NULL;
	if(asprintf(&path, "%s/output/%s/%s/new/staging", pwd, gname, pname) == -1) {
		fprintf(stderr,
			"Failed creating output/global-name/package-name/new/staging path: %s\n",
			strerror(errno));
	}
	res = mkdir(path, 0700);
	if(res < 0) {
		if(errno != EEXIST) {
			throw DirException(path, strerror(errno));

		}
	}
	this->new_staging = std::string(path);
	free(path);
	path = NULL;
	if(asprintf(&path, "%s/output/%s/%s/new/install", pwd, gname, pname) == -1) {
		fprintf(stderr,
			"Failed creating output/global-name/package-name/new/install path: %s\n",
			strerror(errno));
	}
	res = mkdir(path, 0700);
	if(res < 0) {
		if(errno != EEXIST) {
			throw DirException(path, strerror(errno));

		}
	}
	this->new_install = std::string(path);
	free(path);

	free(gname);
	free(pname);
	free(pwd);
}

void BuildDir::clean()
{
	char *cmd = NULL;
	asprintf(&cmd, "rm -fr %s", this->path.c_str());
	system(cmd);
	free(cmd);
	int res = mkdir(this->path.c_str(), 0700);
	if(res < 0) {
		// We should complain here
	}
}
