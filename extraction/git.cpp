/******************************************************************************
 Copyright 2019 Allied Telesis Labs Ltd. All rights reserved.
 
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

static bool refspec_is_commitid(std::string refspec)
{
	const char *refspec_str = refspec.c_str();
	if(strlen(refspec_str) != 40) {
		return false;
	}
	for(int i = 0; i < 40; i++) {
		if(!isxdigit(refspec_str[i])) {
			return false;
		}
	}
	return true;
}

static char *git_hash_ref(const char *gdir, const char *refspec)
{
	char *cmd = NULL;
	if(asprintf(&cmd, "cd %s && git rev-parse %s", gdir, refspec) < 0) {
		throw MemoryException();
	}
	FILE *f = popen(cmd, "r");
	if(f == NULL) {
		throw CustomException("git rev-parse ref failed");
	}
	char *Commit = (char *) calloc(41, sizeof(char));
	fread(Commit, sizeof(char), 40, f);
	pclose(f);
	free(cmd);
	return Commit;
}

static char *git_hash(const char *gdir)
{
	return git_hash_ref(gdir, "HEAD");
}

static char *git_diff_hash(const char *gdir)
{
	char *cmd = NULL;
	if(asprintf(&cmd, "cd %s && git diff HEAD | sha1sum", gdir) < 0) {
		throw MemoryException();
	}
	FILE *f = popen(cmd, "r");
	if(f == NULL) {
		throw CustomException("git diff | sha1sum failed");
	}
	char *Commit = (char *) calloc(41, sizeof(char));
	fread(Commit, sizeof(char), 40, f);
	pclose(f);
	free(cmd);
	return Commit;
}

static char *git_remote(const char *gdir, const char *remote)
{
	char *cmd = NULL;
	if(asprintf(&cmd, "cd %s && git config --local --get remote.%s.url", gdir, remote) <
	   0) {
		throw MemoryException();
	}
	FILE *f = popen(cmd, "r");
	if(f == NULL) {
		throw CustomException("git config --local --get remote. .url failed");
	}
	char *Remote = (char *) calloc(1025, sizeof(char));
	fread(Remote, sizeof(char), 1024, f);
	pclose(f);
	free(cmd);
	return Remote;
}

GitDirExtractionUnit::GitDirExtractionUnit(const char *git_dir, const char *to_dir)
{
	this->uri = std::string(git_dir);
	char *Hash = git_hash(git_dir);
	this->hash = new std::string(Hash);
	free(Hash);
	this->toDir = std::string(to_dir);
}

GitDirExtractionUnit::GitDirExtractionUnit()
{
}

bool GitDirExtractionUnit::isDirty()
{
	char *cmd = NULL;
	asprintf(&cmd, "cd %s && git diff --quiet HEAD", this->localPath().c_str());
	int res = system(cmd);
	free(cmd);
	return (res != 0);
}

std::string GitDirExtractionUnit::dirtyHash()
{
	char *phash = git_diff_hash(this->localPath().c_str());
	std::string ret(phash);
	free(phash);
	return ret;
}

GitExtractionUnit::GitExtractionUnit(const char *remote, const char *local,
				     std::string refspec, Package * P)
{
	this->uri = std::string(remote);

	char *source_dir = NULL;
	asprintf(&source_dir, "%s/source/%s", P->getWorld()->getWorkingDir()->c_str(),
		 local);
	this->local = std::string(source_dir);
	free(source_dir);

	this->refspec = refspec;
	this->P = P;

	this->fetched = false;
}

bool GitExtractionUnit::updateOrigin()
{
	char *location = strdup(this->uri.c_str());
	char *source_dir = strdup(this->local.c_str());
	char *remote_url = git_remote(source_dir, "origin");

	if(strcmp(remote_url, location) != 0) {
		std::unique_ptr < PackageCmd > pc(new PackageCmd(source_dir, "git"));
		pc->addArg("remote");
		// If the remote doesn't exist, add it
		if(strcmp(remote_url, "") == 0) {
			pc->addArg("add");
		} else {
			pc->addArg("set-url");
		}
		pc->addArg("origin");
		pc->addArg(location);
		if(!pc->Run(this->P)) {
			throw CustomException("Failed: git remote set-url origin");
		}
		// Forcibly fetch if the remote url has change,
		// if the ref is origin the user wont get what they wanted otherwise
		pc.reset(new PackageCmd(source_dir, "git"));
		pc->addArg("fetch");
		pc->addArg("origin");
		pc->addArg("--tags");
		if(!pc->Run(this->P)) {
			throw CustomException("Failed: git fetch origin --tags");
		}
	}

	free(location);
	free(source_dir);
	free(remote_url);
	return true;
}

bool GitExtractionUnit::fetch(BuildDir * d)
{
	char *location = strdup(this->uri.c_str());
	char *source_dir = strdup(this->local.c_str());
	const char *cwd = d->getWorld()->getWorkingDir()->c_str();

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
		/* Update the origin */
		this->updateOrigin();
		/* Check if the commit is already present */
		std::string cmd =
		    "cd " + std::string(source_dir) + "; git cat-file -e " + this->refspec +
		    " 2>/dev/null";
		if(system(cmd.c_str()) != 0) {
			/* If not, fetch everything from origin */
			pc->addArg("fetch");
			pc->addArg("origin");
			pc->addArg("--tags");
			if(!pc->Run(this->P)) {
				throw CustomException("Failed: git fetch origin --tags");
			}
		}
	} else {
		pc->addArg("clone");
		pc->addArg("-n");
		pc->addArg(location);
		pc->addArg(source_dir);
		if(!pc->Run(this->P))
			throw CustomException("Failed to git clone");
	}

	if(this->refspec.compare("HEAD") == 0) {
		// Don't touch it
	} else {
		std::string cmd =
		    "cd " + std::string(source_dir) +
		    "; git show-ref --quiet --verify -- refs/heads/" + this->refspec;
		if(system(cmd.c_str()) == 0) {
			char *head_hash = git_hash_ref(source_dir, "HEAD");
			char *branch_hash = git_hash_ref(source_dir, this->refspec.c_str());
			if(strcmp(head_hash, branch_hash)) {
				throw CustomException("Asked to use branch: " +
						      this->refspec + ", but " +
						      source_dir +
						      " is off somewhere else");
			}
			free(head_hash);
			free(branch_hash);
		} else {
			pc.reset(new PackageCmd(source_dir, "git"));
			// switch to refspec
			pc->addArg("checkout");
			pc->addArg("-q");
			pc->addArg("--detach");
			pc->addArg(this->refspec.c_str());
			if(!pc->Run(this->P))
				throw CustomException("Failed to checkout");
		}
	}
	bool res = true;

	char *Hash = git_hash(source_dir);

	if(this->hash) {
		if(strcmp(this->hash->c_str(), Hash) != 0) {
			log(this->P,
			    "Hash mismatch for %s\n(committed to %s, providing %s)\n",
			    this->uri.c_str(), this->hash->c_str(), Hash);
			res = false;
		}
	} else {
		this->hash = new std::string(Hash);
	}

	free(Hash);

	free(source_dir);
	free(location);

	this->fetched = res;

	return res;
}

std::string GitExtractionUnit::HASH()
{
	char *digest_name = NULL;
	asprintf(&digest_name, "%s#%s", this->uri.c_str(), this->refspec.c_str());
	/* Check if the package contains pre-computed hashes */
	char *Hash = P->getFileHash(digest_name);
	free(digest_name);
	if(Hash) {
		this->hash = new std::string(Hash);
		free(Hash);
	} else if(refspec_is_commitid(this->refspec)) {
		this->hash = new std::string(this->refspec);
	} else {
		this->fetch(NULL);
	}
	return *this->hash;
}

bool GitExtractionUnit::extract(Package * P, BuildDir * bd)
{
	// make sure it has been fetched
	if(!this->fetched) {
		if(!this->fetch(NULL)) {
			return false;
		}
	}
	// copy to work dir
	std::unique_ptr < PackageCmd > pc(new PackageCmd(bd->getPath(), "cp"));
	pc->addArg("-dpRuf");
	pc->addArg(this->localPath());
	pc->addArg(".");
	if(!pc->Run(P))
		throw CustomException("Failed to checkout");

	return true;
}

bool LinkGitDirExtractionUnit::extract(Package * P, BuildDir * bd)
{
	std::unique_ptr < PackageCmd > pc(new PackageCmd(bd->getPath(), "ln"));

	pc->addArg("-sfT");

	if(this->uri[0] == '.') {
		pc->addArgFmt("%s/%s", P->getWorld()->getWorkingDir()->c_str(),
			      this->uri.c_str());
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
		pc->addArgFmt("%s/%s", P->getWorld()->getWorkingDir()->c_str(),
			      this->uri.c_str());
	} else {
		pc->addArg(this->uri);
	}
	pc->addArg(this->toDir);

	if(!pc->Run(P)) {
		throw CustomException("Operation failed");
	}

	return true;
}
