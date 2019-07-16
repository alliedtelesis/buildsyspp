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

static bool directory_exists(const std::string & dir)
{
	struct stat info;

	if(stat(dir.c_str(), &info) != 0) {
		/* Nothing here */
		return false;
	} else if(S_ISDIR(info.st_mode)) {
		/* Actually a directory */
		return true;
	}
	/* Something other than a directory */
	return false;
}

static bool refspec_is_commitid(const std::string & refspec)
{
	if(refspec.length() != 40) {
		return false;
	}

	for(char const &c:refspec) {
		if(!isxdigit(c)) {
			return false;
		}
	}

	return true;
}

static std::string git_hash_ref(const char *gdir, const char *refspec)
{
	std::string cmd = string_format("cd %s && git rev-parse %s", gdir, refspec);
	FILE *f = popen(cmd.c_str(), "r");
	if(f == NULL) {
		throw CustomException("git rev-parse ref failed");
	}
	char *commit = (char *) calloc(41, sizeof(char));
	fread(commit, sizeof(char), 40, f);
	pclose(f);

	std::string ret = std::string(commit);
	free(commit);

	return ret;
}

static std::string git_hash(const char *gdir)
{
	return git_hash_ref(gdir, "HEAD");
}

static std::string git_diff_hash(const char *gdir)
{
	std::string cmd = string_format("cd %s && git diff HEAD | sha1sum", gdir);
	FILE *f = popen(cmd.c_str(), "r");
	if(f == NULL) {
		throw CustomException("git diff | sha1sum failed");
	}
	char *delta_hash = (char *) calloc(41, sizeof(char));
	fread(delta_hash, sizeof(char), 40, f);
	pclose(f);
	std::string ret(delta_hash);
	free(delta_hash);
	return ret;
}

static std::string git_remote(const std::string & gdir, const std::string & remote)
{
	std::string cmd = string_format("cd %s && git config --local --get remote.%s.url",
					gdir.c_str(), remote.c_str());
	FILE *f = popen(cmd.c_str(), "r");
	if(f == NULL) {
		throw CustomException("git config --local --get remote. .url failed");
	}
	char *Remote = (char *) calloc(1025, sizeof(char));
	fread(Remote, sizeof(char), 1024, f);
	pclose(f);
	std::string ret(Remote);
	free(Remote);
	return ret;
}

GitDirExtractionUnit::GitDirExtractionUnit(const char *git_dir, const char *to_dir)
{
	this->uri = std::string(git_dir);
	this->hash = git_hash(this->uri.c_str());
	this->toDir = std::string(to_dir);
}

GitDirExtractionUnit::GitDirExtractionUnit()
{
}

bool GitDirExtractionUnit::isDirty()
{
	if(!directory_exists(this->localPath())) {
		/* If the source directory doesn't exist, then it can't be dirty */
		return false;
	}

	std::string cmd = string_format("cd %s && git diff --quiet HEAD",
					this->localPath().c_str());
	int res = system(cmd.c_str());
	return (res != 0);
}

std::string GitDirExtractionUnit::dirtyHash()
{
	return git_diff_hash(this->localPath().c_str());
}

GitExtractionUnit::GitExtractionUnit(const char *remote, const char *local,
				     std::string refspec, Package * P)
{
	this->uri = std::string(remote);
	this->local = string_format("%s/source/%s", P->getWorld()->getWorkingDir().c_str(),
				    local);
	this->refspec = refspec;
	this->P = P;
	this->fetched = false;
}

bool GitExtractionUnit::updateOrigin()
{
	std::string location = this->uri;
	std::string source_dir = this->local;
	std::string remote_url = git_remote(source_dir, "origin");

	if(strcmp(remote_url.c_str(), location.c_str()) != 0) {
		std::unique_ptr < PackageCmd >
		    pc(new PackageCmd(source_dir.c_str(), "git"));
		pc->addArg("remote");
		// If the remote doesn't exist, add it
		if(strcmp(remote_url.c_str(), "") == 0) {
			pc->addArg("add");
		} else {
			pc->addArg("set-url");
		}
		pc->addArg("origin");
		pc->addArg(location.c_str());
		if(!pc->Run(this->P)) {
			throw CustomException("Failed: git remote set-url origin");
		}
		// Forcibly fetch if the remote url has change,
		// if the ref is origin the user wont get what they wanted otherwise
		pc.reset(new PackageCmd(source_dir.c_str(), "git"));
		pc->addArg("fetch");
		pc->addArg("origin");
		pc->addArg("--tags");
		if(!pc->Run(this->P)) {
			throw CustomException("Failed: git fetch origin --tags");
		}
	}

	return true;
}

bool GitExtractionUnit::fetch(BuildDir * d)
{
	std::string location = this->uri;
	std::string source_dir = this->local;
	const char *cwd = d->getWorld()->getWorkingDir().c_str();

	bool exists = directory_exists(source_dir);

	std::unique_ptr < PackageCmd >
	    pc(new PackageCmd(exists ? source_dir.c_str() : cwd, "git"));

	if(exists) {
		/* Update the origin */
		this->updateOrigin();
		/* Check if the commit is already present */
		std::string cmd =
		    "cd " + source_dir + "; git cat-file -e " + this->refspec +
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
		pc->addArg(location.c_str());
		pc->addArg(source_dir.c_str());
		if(!pc->Run(this->P))
			throw CustomException("Failed to git clone");
	}

	if(this->refspec.compare("HEAD") == 0) {
		// Don't touch it
	} else {
		std::string cmd =
		    "cd " + source_dir +
		    "; git show-ref --quiet --verify -- refs/heads/" + this->refspec;
		if(system(cmd.c_str()) == 0) {
			std::string head_hash = git_hash_ref(source_dir.c_str(), "HEAD");
			std::string branch_hash = git_hash_ref(source_dir.c_str(),
							       this->refspec.c_str());
			if(strcmp(head_hash.c_str(), branch_hash.c_str())) {
				throw CustomException("Asked to use branch: " +
						      this->refspec + ", but " +
						      source_dir +
						      " is off somewhere else");
			}
		} else {
			pc.reset(new PackageCmd(source_dir.c_str(), "git"));
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

	std::string hash = git_hash(source_dir.c_str());

	if(!this->hash.empty()) {
		if(strcmp(this->hash.c_str(), hash.c_str()) != 0) {
			log(this->P,
			    "Hash mismatch for %s\n(committed to %s, providing %s)\n",
			    this->uri.c_str(), this->hash.c_str(), hash.c_str());
			res = false;
		}
	} else {
		this->hash = hash;
	}

	this->fetched = res;

	return res;
}

std::string GitExtractionUnit::HASH()
{
	if(refspec_is_commitid(this->refspec)) {
		this->hash = std::string(this->refspec);
	} else {
		std::string digest_name = string_format("%s#%s", this->uri.c_str(),
							this->refspec.c_str());
		/* Check if the package contains pre-computed hashes */
		std::string Hash = P->getFileHash(digest_name);
		if(Hash.empty()) {
			this->fetch(P->builddir());
		} else {
			this->hash = Hash;
		}
	}
	return this->hash;
}

bool GitExtractionUnit::extract(Package * P, BuildDir * bd)
{
	// make sure it has been fetched
	if(!this->fetched) {
		if(!this->fetch(bd)) {
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
		pc->addArgFmt("%s/%s", P->getWorld()->getWorkingDir().c_str(),
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
		pc->addArgFmt("%s/%s", P->getWorld()->getWorkingDir().c_str(),
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
