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

static char *git_hash(const char *gdir)
{
	chdir(gdir);
	FILE *f = popen("git rev-parse HEAD", "r");
	char *Commit = (char *) calloc(41, sizeof(char));
	fread(Commit, sizeof(char), 40, f);
	chdir(WORLD->getWorkingDir()->c_str());
	pclose(f);
	return Commit;
}

static char *git_diff_hash(const char *gdir)
{
	chdir(gdir);
	FILE *f = popen("git diff HEAD | sha1sum", "r");
	char *Commit = (char *) calloc(41, sizeof(char));
	fread(Commit, sizeof(char), 40, f);
	chdir(WORLD->getWorkingDir()->c_str());
	pclose(f);
	return Commit;
}

bool Extraction::add(ExtractionUnit * eu)
{
	ExtractionUnit **t = this->EUs;
	this->EU_count++;
	this->EUs =
	    (ExtractionUnit **) realloc(t, sizeof(ExtractionUnit *) * this->EU_count);
	if(this->EUs == NULL) {
		this->EUs = t;
		this->EU_count--;
		return false;
	}
	this->EUs[this->EU_count - 1] = eu;
	return true;
}

TarExtractionUnit::TarExtractionUnit(const char *fname)
{
	this->uri = std::string(fname);
	char *Hash = hash_file(fname);
	this->hash = std::string(Hash);
	free(Hash);
}

bool TarExtractionUnit::extract(Package * P, BuildDir * bd)
{
	std::unique_ptr < PackageCmd > pc(new PackageCmd(bd->getPath(), "tar"));

	int res = mkdir("dl", 0700);
	if((res < 0) && (errno != EEXIST)) {
		throw CustomException("Error: Creating download directory");
	}
	pc->addArg("xf");
	pc->addArgFmt("%s/%s", WORLD->getWorkingDir()->c_str(), this->uri.c_str());

	if(!pc->Run(P))
		throw CustomException("Failed to extract file");

	return true;
}

ZipExtractionUnit::ZipExtractionUnit(const char *fname)
{
	this->uri = std::string(fname);
	char *Hash = hash_file(fname);
	this->hash = std::string(Hash);
	free(Hash);
}

bool ZipExtractionUnit::extract(Package * P, BuildDir * bd)
{
	std::unique_ptr < PackageCmd > pc(new PackageCmd(bd->getPath(), "unzip"));

	int res = mkdir("dl", 0700);
	if((res < 0) && (errno != EEXIST)) {
		throw CustomException("Error: Creating download directory");
	}

	pc->addArg("-o");
	pc->addArgFmt("%s/%s", WORLD->getWorkingDir()->c_str(), this->uri.c_str());

	if(!pc->Run(P))
		throw CustomException("Failed to extract file");

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

bool PatchExtractionUnit::extract(Package * P, BuildDir * bd)
{
	std::unique_ptr < PackageCmd > pc_dry(new PackageCmd(this->patch_path, "patch"));
	std::unique_ptr < PackageCmd > pc(new PackageCmd(this->patch_path, "patch"));

	pc_dry->addArgFmt("-p%i", this->level);
	pc->addArgFmt("-p%i", this->level);

	pc_dry->addArg("-stN");
	pc->addArg("-stN");

	pc_dry->addArg("-i");
	pc->addArg("-i");

	const char *pwd = WORLD->getWorkingDir()->c_str();
	pc_dry->addArgFmt("%s/%s", pwd, this->uri.c_str());
	pc->addArgFmt("%s/%s", pwd, this->uri.c_str());

	pc_dry->addArg("--dry-run");

	if(!pc_dry->Run(P)) {
		log(P->getName().c_str(), "Patch file: %s", this->uri.c_str());
		throw CustomException("Will fail to patch");
	}

	if(!pc->Run(P))
		throw CustomException("Truely failed to patch");

	return true;
}

FileCopyExtractionUnit::FileCopyExtractionUnit(const char *fname)
{
	this->uri = std::string(fname);
	char *Hash = hash_file(fname);
	this->hash = std::string(Hash);
	free(Hash);
}

bool FileCopyExtractionUnit::extract(Package * P, BuildDir * bd)
{
	std::unique_ptr < PackageCmd > pc(new PackageCmd(bd->getPath(), "cp"));
	pc->addArg("-pRuf");

	pc->addArgFmt("%s/%s", WORLD->getWorkingDir()->c_str(), this->uri.c_str());

	pc->addArg(".");

	if(!pc->Run(P)) {
		throw CustomException("Failed to copy file");
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
	chdir(this->localPath().c_str());
	int res = system("git diff --quiet HEAD");
	chdir(WORLD->getWorkingDir()->c_str());
	return (res != 0);
}

std::string GitDirExtractionUnit::dirtyHash()
{
	char *phash = git_diff_hash(this->localPath().c_str());
	std::string ret(phash);
	free(phash);
	return ret;
}

bool LinkGitDirExtractionUnit::extract(Package * P, BuildDir * bd)
{
	std::unique_ptr < PackageCmd > pc(new PackageCmd(bd->getPath(), "ln"));

	pc->addArg("-sfT");

	if(this->uri[0] == '.') {
		pc->addArgFmt("%s/%s", WORLD->getWorkingDir()->c_str(), this->uri.c_str());
	} else {
		pc->addArg(this->uri.c_str());
	}
	pc->addArg(this->toDir);

	if(!pc->Run(P)) {
		throw CustomException("Operation failed");
	}

	return true;
}

bool CopyGitDirExtractionUnit::extract(Package * P, BuildDir * bd)
{
	std::unique_ptr < PackageCmd > pc(new PackageCmd(bd->getPath(), "cp"));
	pc->addArg("-dpRuf");

	if(this->uri[0] == '.') {
		pc->addArgFmt("%s/%s", WORLD->getWorkingDir()->c_str(), this->uri.c_str());
	} else {
		pc->addArg(this->uri);
	}
	pc->addArg(this->toDir);

	if(!pc->Run(P)) {
		throw CustomException("Operation failed");
	}

	return true;
}

bool GitExtractionUnit::fetch(Package * P)
{
	char *location = strdup(this->uri.c_str());
	char *repo_name = strrchr(location, '/');
	if(repo_name != NULL) {
		repo_name = strdup(repo_name + 1);
		char *dotgit = strstr(repo_name, ".git");
		if(dotgit) {
			dotgit[0] = '\0';
		}
		if(strlen(repo_name) == 0) {
			free(repo_name);
			repo_name = strrchr(strrchr(location, '/'), '/');
			repo_name = strdup(repo_name);
		}
	}

	char *source_dir = NULL;
	const char *cwd = WORLD->getWorkingDir()->c_str();
	asprintf(&source_dir, "%s/source/%s", cwd, repo_name);

	this->local = std::string(source_dir);

	bool exists = false;
	{
		struct stat s;
		int err = stat(source_dir, &s);
		if(err == 0 && S_ISDIR(s.st_mode)) {
			exists = true;
		}
	}

	std::unique_ptr < PackageCmd > pc(new PackageCmd(exists ? source_dir : cwd, "git"));

	if(exists) {
		pc->addArg("fetch");
		pc->addArg("origin");
		pc->addArg("--tags");
		if(!pc->Run(P)) {
			throw CustomException("Failed: git fetch origin --tags");
		}
	} else {
		pc->addArg("clone");
		pc->addArg("-n");
		pc->addArg(location);
		pc->addArg(source_dir);
		if(!pc->Run(P))
			throw CustomException("Failed to git clone");
	}

	pc.reset(new PackageCmd(source_dir, "git"));
	// switch to refspec
	pc->addArg("checkout");
	pc->addArg("-q");
	pc->addArg("--detach");
	pc->addArg(this->refspec.c_str());
	if(!pc->Run(P))
		throw CustomException("Failed to checkout");

	free(repo_name);
	free(location);
	free(source_dir);

	return true;
}

bool GitExtractionUnit::extract(Package * P, BuildDir * bd)
{
	// copy to work dir
	std::unique_ptr < PackageCmd > pc(new PackageCmd(bd->getPath(), "cp"));
	pc->addArg("-dpRuf");
	pc->addArg(this->localPath());
	pc->addArg(".");
	if(!pc->Run(P))
		throw CustomException("Failed to checkout");

	return true;
}

bool BuildDescription::add(BuildUnit * bu)
{
	BuildUnit **t = this->BUs;
	this->BU_count++;
	this->BUs = (BuildUnit **) realloc(t, sizeof(BuildUnit *) * this->BU_count);
	if(this->BUs == NULL) {
		this->BUs = t;
		this->BU_count--;
		return false;
	}
	this->BUs[this->BU_count - 1] = bu;
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
