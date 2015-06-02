/****************************************************************************************************************************
 Copyright 2013 Allied Telesis Labs Ltd. All rights reserved.
 
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the 
distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************************************************************/

#include <buildsys.h>

static char * git_hash(const char *gdir)
{
	char *pwd = getcwd(NULL,0);
	char *cmd = NULL;
	asprintf(&cmd, "git rev-parse HEAD");
	chdir(gdir);
	FILE *f = popen(cmd, "r");
	free(cmd);
	char *Commit = (char *)calloc(41, sizeof(char));
	fread(Commit, sizeof(char), 40, f);
	chdir(pwd);
	pclose(f);
	return Commit;
}

static char * git_diff_hash(const char *gdir)
{
	char *pwd = getcwd(NULL,0);
	char *cmd = NULL;
	asprintf(&cmd, "git diff HEAD | sha1sum");
	chdir(gdir);
	FILE *f = popen(cmd, "r");
	free(cmd);
	char *Commit = (char *)calloc(41, sizeof(char));
	fread(Commit, sizeof(char), 40, f);
	chdir(pwd);
	pclose(f);
	return Commit;
}

bool Extraction::add(ExtractionUnit *eu)
{
	ExtractionUnit **t = this->EUs;
	this->EU_count++;
	this->EUs = (ExtractionUnit **)realloc(t, sizeof(ExtractionUnit *) * this->EU_count);
	if(this->EUs == NULL)
	{
		this->EUs = t;
		this->EU_count--;
		return false;
	}
	this->EUs[this->EU_count-1] = eu;
	return true;
}

TarExtractionUnit::TarExtractionUnit(const char *fname)
{
	this->uri = std::string(fname);
	char *Hash = hash_file(fname);
	this->hash = std::string(Hash);
	free(Hash);
}

bool TarExtractionUnit::extract(Package *P, BuildDir *bd)
{
	char **argv = NULL;
	char *pwd = getcwd(NULL, 0);

	int res = mkdir("dl", 0700);
	if((res < 0) && (errno != EEXIST))
	{
		throw CustomException("Error: Creating download directory");
	}
	char *t_full_path = NULL;
	asprintf(&t_full_path, "%s/%s", pwd, this->uri.c_str());
	free(pwd);
	pwd = NULL;
	argv = (char **)calloc(4, sizeof(char *));
	argv[0] = strdup("tar");
	argv[1] = strdup("xf");
	argv[2] = t_full_path;
	
	if(run(P->getName().c_str(), (char *)"tar", argv , bd->getPath(), NULL) != 0)
		throw CustomException("Failed to extract file");
	
	if(argv != NULL)
	{
		int i = 0;
		while(argv[i] != NULL)
		{
			free(argv[i]);
			i++;
		}
		free(argv);
	}
	return true;
}

ZipExtractionUnit::ZipExtractionUnit(const char *fname)
{
	this->uri = std::string(fname);
	char *Hash = hash_file(fname);
	this->hash = std::string(Hash);
	free(Hash);
}

bool ZipExtractionUnit::extract(Package *P, BuildDir *bd)
{
	char **argv = NULL;
	char *pwd = getcwd(NULL, 0);

	int res = mkdir("dl", 0700);
	if((res < 0) && (errno != EEXIST))
	{
		throw CustomException("Error: Creating download directory");
	}
	char *t_full_path = NULL;
	asprintf(&t_full_path, "%s/%s", pwd, this->uri.c_str());
	free(pwd);
	pwd = NULL;
	argv = (char **)calloc(4, sizeof(char *));
	argv[0] = strdup("unzip");
	argv[1] = strdup("-o");
	argv[2] = t_full_path;
	if(run(P->getName().c_str(), (char *)"unzip", argv , bd->getPath(), NULL) != 0)
		throw CustomException("Failed to extract file");

	if(argv != NULL)
	{
		int i = 0;
		while(argv[i] != NULL)
		{
			free(argv[i]);
			i++;
		}
		free(argv);
	}
	return true;
}


PatchExtractionUnit::PatchExtractionUnit(int level, char *pp, char *uri)
{
	this->uri = std::string(uri);
	char *Hash = hash_file(uri);
	this->hash = std::string(Hash);
	free(Hash);
	this->level = level;
	this->patch_path = strdup(pp);
}

bool PatchExtractionUnit::extract(Package *P, BuildDir *bd)
{
	char *pwd = getcwd(NULL, 0);
	char **argv = (char **)calloc(7, sizeof(char *));
	argv[0] = strdup("patch");
	asprintf(&argv[1], "-p%i", this->level);
	argv[2] = strdup("-stN");
	argv[3] = strdup("-i");
	
	char *p_full_path = NULL;
	asprintf(&p_full_path, "%s/%s", pwd, this->uri.c_str());
	free(pwd);
	pwd = NULL;
	
	argv[4] = p_full_path;
	p_full_path = NULL;
	argv[5] = strdup("--dry-run");
	if(run(P->getName().c_str(), (char *)"patch", argv , patch_path, NULL) != 0)
	{
		log(P->getName().c_str(), "Patch file: %s", argv[4]);
		throw CustomException("Will fail to patch");
	}
	free(argv[5]);
	argv[5] = NULL;
	if(run(P->getName().c_str(), (char *)"patch", argv , patch_path, NULL) != 0)
		throw CustomException("Truely failed to patch");

	if(argv != NULL)
	{
		int i = 0;
		while(argv[i] != NULL)
		{
			free(argv[i]);
			i++;
		}
		free(argv);
	}

	return true;
}

FileCopyExtractionUnit::FileCopyExtractionUnit(const char *fname)
{
	this->uri = std::string(fname);
	char *Hash = hash_file(fname);
	this->hash = std::string(Hash);
	free(Hash);
}

bool FileCopyExtractionUnit::extract(Package *P, BuildDir *bd)
{
	char *pwd = getcwd(NULL, 0);
	char **argv = NULL;
	
	char *f_full_path = NULL;
	asprintf(&f_full_path, "%s/%s", pwd, this->uri.c_str());
	free(pwd);
	pwd = NULL;
	
	argv = (char **)calloc(5, sizeof(char *));
	argv[0] = strdup("cp");
	argv[1] = strdup("-a");
	argv[2] = f_full_path;
	f_full_path = NULL;
	argv[3] = strdup(".");
	
	if(run(P->getName().c_str(), (char *)"cp", argv , bd->getPath(), NULL) != 0)
		throw CustomException("Failed to copy file");
	
	if(argv != NULL)
	{
		int i = 0;
		while(argv[i] != NULL)
		{
			free(argv[i]);
			i++;
		}
		free(argv);
	}
	return true;
}

GitDirExtractionUnit::GitDirExtractionUnit(const char *git_dir, const char *to_dir)
{
	this->uri = std::string(git_dir);
	char *Hash = git_hash(git_dir);
	this->hash = std::string(Hash);
	free(Hash);
	this->toDir = std::string(to_dir);
}

bool GitDirExtractionUnit::isDirty()
{
	char *pwd = getcwd(NULL,0);
	char *cmd = NULL;
	asprintf(&cmd, "git diff --quiet HEAD");
	chdir(this->localPath().c_str());
	int res = system(cmd);
	free(cmd);
	chdir(pwd);
	return (res != 0);
}

std::string GitDirExtractionUnit::dirtyHash()
{
	char *phash = git_diff_hash(this->localPath().c_str());
	std::string ret(phash);
	free(phash);
	return ret;
}

bool LinkGitDirExtractionUnit::extract(Package *P, BuildDir *bd)
{
	char **argv = (char **)calloc(5, sizeof(char *));
	int argp = 0;
	argv[argp++] = strdup("ln");
	argv[argp++] = strdup("-sfT");
	if(this->uri[0] == '.')
	{
		char *pwd = getcwd(NULL, 0);
		asprintf(&argv[argp++], "%s/%s", pwd, this->uri.c_str());
		free(pwd);
	} else {
		argv[argp++] = strdup(this->uri.c_str());
	}
	argv[argp++] = strdup(this->toDir.c_str());
	if(run(P->getName().c_str(), argv[0], argv , bd->getPath(), NULL) != 0)
	{
		throw CustomException("Operation failed");
	}
	return true;
}

bool CopyGitDirExtractionUnit::extract(Package *P, BuildDir *bd)
{
	char **argv = (char **)calloc(5, sizeof(char *));
	int argp = 0;
	argv[argp++] = strdup("cp");
	argv[argp++] = strdup("-dpRuf");
	if(this->uri[0] == '.')
	{
		char *pwd = getcwd(NULL, 0);
		asprintf(&argv[argp++], "%s/%s", pwd, this->uri.c_str());
		free(pwd);
	} else {
		argv[argp++] = strdup(this->uri.c_str());
	}
	argv[argp++] = strdup(this->toDir.c_str());
	if(run(P->getName().c_str(), argv[0], argv , bd->getPath(), NULL) != 0)
	{
		throw CustomException("Operation failed");
	}
	return true;
}

bool GitExtractionUnit::fetch(Package *P)
{
	char *location = strdup(this->uri.c_str());
	char *repo_name = strrchr(location, '/');
	if(repo_name != NULL)
	{
		repo_name = strdup(repo_name+1);
		char *dotgit = strstr(repo_name,".git");
		if (dotgit)
		{
			dotgit[0] = '\0';
		}
		if (strlen(repo_name) == 0)
		{
			free(repo_name);
			repo_name = strrchr(strrchr(location, '/'), '/');
			repo_name = strdup(repo_name);
		}
	}

	char *source_dir = NULL;
	char *cwd = getcwd(NULL, 0);
	asprintf(&source_dir, "%s/source/%s", cwd, repo_name);

	this->local = std::string(source_dir);

	bool exists = false;
	{
		struct stat s;
		int err = stat(source_dir, &s);
		if (err == 0 && S_ISDIR(s.st_mode))
		{
			exists = true;
		}
	}
	char **argv = (char **)calloc(6, sizeof(char *));
	argv[0] = strdup("git");
	if (exists)
	{
		argv[1] = strdup("fetch");
		argv[2] = strdup("origin");
		argv[3] = strdup("--tags");
		if(run(P->getName().c_str(), (char*)"git", argv, source_dir, NULL) != 0)
		{
			throw CustomException("Failed: git fetch origin --tags");
		}
	} else {
		argv[1] = strdup("clone");
		argv[2] = strdup("-n");
		argv[3] = strdup(location);
		argv[4] = strdup(source_dir);
		if(run(P->getName().c_str(), (char *)"git", argv , cwd, NULL) != 0)
			throw CustomException("Failed to git clone");
	}

	if(argv != NULL)
	{
		int i = 0;
		while(argv[i] != NULL)
		{
			free(argv[i]);
			i++;
		}
		free(argv);
	}

	// switch to refspec
	argv = (char **)calloc(6, sizeof(char *));
	argv[0] = strdup("git");
	argv[1] = strdup("checkout");
	argv[2] = strdup("-q");
	argv[3] = strdup("--detach");
	argv[4] = strdup(this->refspec.c_str());
	if(run(P->getName().c_str(), (char *)"git", argv, source_dir, NULL) != 0)
		throw CustomException("Failed to checkout");

	if(argv != NULL)
	{
		int i = 0;
		while(argv[i] != NULL)
		{
			free(argv[i]);
			i++;
		}
		free(argv);
	}

	free(repo_name);
	free(location);
	free(source_dir);

	return true;
}

bool GitExtractionUnit::extract (Package *P, BuildDir *bd)
{
	// copy to work dir
	char **argv = (char **)calloc(5, sizeof(char *));
	argv[0] = strdup("cp");
	argv[1] = strdup("-dpRuf");
	argv[2] = strdup(this->localPath().c_str());
	argv[3] = strdup(".");
	if(run(P->getName().c_str(), (char *)"cp", argv, bd->getPath(), NULL) != 0)
		throw CustomException("Failed to checkout");

	if(argv != NULL)
	{
		int i = 0;
		while(argv[i] != NULL)
		{
			free(argv[i]);
			i++;
		}
		free(argv);
	}

	return true;
}

bool BuildDescription::add(BuildUnit *bu)
{
	BuildUnit **t = this->BUs;
	this->BU_count++;
	this->BUs = (BuildUnit **)realloc(t, sizeof(BuildUnit *) * this->BU_count);
	if(this->BUs == NULL)
	{
		this->BUs = t;
		this->BU_count--;
		return false;
	}
	this->BUs[this->BU_count-1] = bu;
	return true;
}

PackageFileUnit::PackageFileUnit(const char *fname)
{
	this->uri = std::string(fname);
	char *Hash = hash_file(fname);
	this->hash = std::string(Hash);
	free(Hash);
}

ExtractionInfoFileUnit::ExtractionInfoFileUnit(const char *fname)
{
	this->uri = std::string(fname);
	char *Hash = hash_file(fname);
	this->hash = std::string(Hash);
	free(Hash);
}

BuildInfoFileUnit::BuildInfoFileUnit(const char *fname)
{
	this->uri = std::string(fname);
	char *Hash = hash_file(fname);
	this->hash = std::string(Hash);
	free(Hash);
}

OutputInfoFileUnit::OutputInfoFileUnit(const char *fname)
{
	this->uri = std::string(fname);
	char *Hash = hash_file(fname);
	this->hash = std::string(Hash);
	free(Hash);
}
