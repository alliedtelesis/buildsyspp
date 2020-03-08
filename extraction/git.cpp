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

#include "include/buildsys.h"

static bool refspec_is_commitid(const std::string &refspec)
{
	if(refspec.length() != 40) {
		return false;
	}

	for(char const &c : refspec) {
		if(isxdigit(c) == 0) {
			return false;
		}
	}

	return true;
}

static std::string git_hash_ref(const std::string &gdir, const std::string &refspec)
{
	std::string cmd = "cd " + gdir + " && git rev-parse " + refspec;
	FILE *f = popen(cmd.c_str(), "r");
	if(f == nullptr) {
		throw CustomException("git rev-parse ref failed");
	}
	char commit[41] = {};
	fread(commit, sizeof(char), 40, f);
	pclose(f);

	return std::string(commit);
}

static std::string git_hash(const std::string &gdir)
{
	return git_hash_ref(gdir, "HEAD");
}

static std::string git_diff_hash(const std::string &gdir)
{
	std::string cmd = "cd " + gdir + " && git diff HEAD | sha1sum";
	FILE *f = popen(cmd.c_str(), "r");
	if(f == nullptr) {
		throw CustomException("git diff | sha1sum failed");
	}
	char delta_hash[41] = {};
	fread(delta_hash, sizeof(char), 40, f);
	pclose(f);

	return std::string(delta_hash);
}

static std::string git_remote(const std::string &gdir, const std::string &remote)
{
	std::string cmd =
	    "cd " + gdir + " && git config --local --get remote." + remote + ".url";
	FILE *f = popen(cmd.c_str(), "r");
	if(f == nullptr) {
		throw CustomException("git config --local --get remote. .url failed");
	}
	char output[1025] = {};
	fread(output, sizeof(char), 1024, f);
	pclose(f);

	return std::string(output);
}

GitDirExtractionUnit::GitDirExtractionUnit(const std::string &git_dir,
                                           const std::string &to_dir)
{
	this->uri = git_dir;
	this->hash = git_hash(this->uri);
	this->toDir = to_dir;
}

bool GitDirExtractionUnit::isDirty()
{
	if(!filesystem::is_directory(this->localPath())) {
		/* If the source directory doesn't exist, then it can't be dirty */
		return false;
	}

	auto cmd = boost::format{"cd %1% && git diff --quiet HEAD"} % this->localPath();
	return (std::system(cmd.str().c_str()) != 0);
}

std::string GitDirExtractionUnit::dirtyHash()
{
	return git_diff_hash(this->localPath());
}

GitExtractionUnit::GitExtractionUnit(const std::string &remote, const std::string &_local,
                                     std::string _refspec, Package *_P)
{
	this->uri = remote;
	this->local = _P->getWorld()->getWorkingDir() + "/source/" + _local;
	this->refspec = std::move(_refspec);
	this->P = _P;
	this->fetched = false;
}

bool GitExtractionUnit::updateOrigin()
{
	std::string location = this->uri;
	std::string source_dir = this->local;
	std::string remote_url = git_remote(source_dir, "origin");

	if(remote_url != location) {
		PackageCmd pc(source_dir, "git");
		pc.addArg("remote");
		// If the remote doesn't exist, add it
		if(remote_url.empty()) {
			pc.addArg("add");
		} else {
			pc.addArg("set-url");
		}
		pc.addArg("origin");
		pc.addArg(location);
		if(!pc.Run(this->P)) {
			throw CustomException("Failed: git remote set-url origin");
		}

		pc = PackageCmd(source_dir, "git");
		pc.addArg("fetch");
		pc.addArg("origin");
		pc.addArg("--tags");
		if(!pc.Run(this->P)) {
			throw CustomException("Failed: git fetch origin --tags");
		}
	}

	return true;
}

bool GitExtractionUnit::fetch(BuildDir *d)
{
	std::string location = this->uri;
	std::string source_dir = this->local;
	std::string cwd = d->getWorld()->getWorkingDir();

	bool exists = filesystem::is_directory(source_dir);

	PackageCmd pc(exists ? source_dir : cwd, "git");

	if(exists) {
		/* Update the origin */
		this->updateOrigin();
		/* Check if the commit is already present */
		std::string cmd =
		    "cd " + source_dir + "; git cat-file -e " + this->refspec + " 2>/dev/null";
		if(std::system(cmd.c_str()) != 0) {
			/* If not, fetch everything from origin */
			pc.addArg("fetch");
			pc.addArg("origin");
			pc.addArg("--tags");
			if(!pc.Run(this->P)) {
				throw CustomException("Failed: git fetch origin --tags");
			}
		}
	} else {
		pc.addArg("clone");
		pc.addArg("-n");
		pc.addArg(location);
		pc.addArg(source_dir);
		if(!pc.Run(this->P)) {
			throw CustomException("Failed to git clone");
		}
	}

	if(this->refspec == "HEAD") {
		// Don't touch it
	} else {
		std::string cmd = "cd " + source_dir +
		                  "; git show-ref --quiet --verify -- refs/heads/" + this->refspec;
		if(std::system(cmd.c_str()) == 0) {
			std::string head_hash = git_hash_ref(source_dir, "HEAD");
			std::string branch_hash = git_hash_ref(source_dir, this->refspec);
			if(head_hash != branch_hash) {
				throw CustomException("Asked to use branch: " + this->refspec + ", but " +
				                      source_dir + " is off somewhere else");
			}
		} else {
			pc = PackageCmd(source_dir, "git");
			// switch to refspec
			pc.addArg("checkout");
			pc.addArg("-q");
			pc.addArg("--detach");
			pc.addArg(this->refspec);
			if(!pc.Run(this->P)) {
				throw CustomException("Failed to checkout");
			}
		}
	}
	bool res = true;

	std::string _hash = git_hash(source_dir);

	if(!this->hash.empty()) {
		if(this->hash != _hash) {
			log(this->P,
			    boost::format{"Hash mismatch for %1%\n(committed to %2%, providing %3%)"} %
			        this->uri % this->hash % _hash);
			res = false;
		}
	} else {
		this->hash = _hash;
	}

	this->fetched = res;

	return res;
}

std::string GitExtractionUnit::HASH()
{
	if(refspec_is_commitid(this->refspec)) {
		this->hash = this->refspec;
	} else {
		std::string digest_name = this->uri + "#" + this->refspec;
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

bool GitExtractionUnit::extract(Package *_P)
{
	// make sure it has been fetched
	if(!this->fetched) {
		if(!this->fetch(_P->builddir())) {
			return false;
		}
	}
	// copy to work dir
	PackageCmd pc(_P->builddir()->getPath(), "cp");
	pc.addArg("-dpRuf");
	pc.addArg(this->localPath());
	pc.addArg(".");
	if(!pc.Run(_P)) {
		throw CustomException("Failed to checkout");
	}

	return true;
}

bool LinkGitDirExtractionUnit::extract(Package *P)
{
	PackageCmd pc(P->builddir()->getPath(), "ln");

	pc.addArg("-sfT");

	if(this->uri.at(0) == '.') {
		std::string arg = P->getWorld()->getWorkingDir() + "/" + this->uri;
		pc.addArg(arg);
	} else {
		pc.addArg(this->uri);
	}
	pc.addArg(this->toDir);

	if(!pc.Run(P)) {
		throw CustomException("Operation failed");
	}

	return true;
}

bool CopyGitDirExtractionUnit::extract(Package *P)
{
	PackageCmd pc(P->builddir()->getPath(), "cp");
	pc.addArg("-dpRuf");

	if(this->uri.at(0) == '.') {
		std::string arg = P->getWorld()->getWorkingDir() + "/" + this->uri;
		pc.addArg(arg);
	} else {
		pc.addArg(this->uri);
	}
	pc.addArg(this->toDir);

	if(!pc.Run(P)) {
		throw CustomException("Operation failed");
	}

	return true;
}
