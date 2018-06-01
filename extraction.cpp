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
	char *cmd = NULL;
	asprintf(&cmd, "cd %s && git rev-parse HEAD", gdir);
	FILE *f = popen(cmd, "r");
	char *Commit = (char *) calloc(41, sizeof(char));
	fread(Commit, sizeof(char), 40, f);
	pclose(f);
	free(cmd);
	return Commit;
}

static char *git_diff_hash(const char *gdir)
{
	char *cmd = NULL;
	asprintf(&cmd, "cd %s && git diff HEAD | sha1sum", gdir);
	FILE *f = popen(cmd, "r");
	char *Commit = (char *) calloc(41, sizeof(char));
	fread(Commit, sizeof(char), 40, f);
	pclose(f);
	free(cmd);
	return Commit;
}

static char *git_remote(const char *gdir, const char *remote)
{
	char *cmd = NULL;
	asprintf(&cmd, "cd %s && git config --local --get remote.%s.url", gdir, remote);
	FILE *f = popen(cmd, "r");
	char *Remote = (char *) calloc(1025, sizeof(char));
	fread(Remote, sizeof(char), 1024, f);
	pclose(f);
	free(cmd);
	return Remote;
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

void Extraction::prepareNewExtractInfo(Package * P, BuildDir * bd)
{
	if(this->extracted) {
		log(P, "Already extracted");
		return;
	}

	if(bd) {
		// Create the new extraction info file
		char *exinfoFname = NULL;
		asprintf(&exinfoFname, "%s/.extraction.info.new", bd->getPath());
		std::ofstream exInfo(exinfoFname);
		this->print(exInfo);
		exInfo.close();
		free(exinfoFname);
	}
}

bool Extraction::extractionRequired(Package * P, BuildDir * bd)
{
	if(this->extracted) {
		return false;
	}

	char *cmd = NULL;
	asprintf(&cmd, "cmp -s %s/.extraction.info.new %s/.extraction.info",
		 bd->getPath(), bd->getPath());
	int res = system(cmd);
	free(cmd);
	cmd = NULL;

	// if there are changes,
	if(res != 0 || P->isCodeUpdated()) {	// Extract our source code
		return true;
	}

	return false;
}

bool Extraction::extract(Package * P, BuildDir * bd)
{
	log(P, "Extracting sources and patching");
	for(size_t i = 0; i < this->EU_count; i++) {
		if(!EUs[i]->extract(P, bd))
			return false;
	}

	// mv the file into the regular place
	char *oldfname = NULL;
	char *newfname = NULL;
	asprintf(&oldfname, "%s/.extraction.info.new", bd->getPath());
	asprintf(&newfname, "%s/.extraction.info", bd->getPath());
	rename(oldfname, newfname);
	free(oldfname);
	free(newfname);

	return true;
};

ExtractionInfoFileUnit *Extraction::extractionInfo(Package * P, BuildDir * bd)
{
	char *extractionInfoFname = NULL;
	asprintf(&extractionInfoFname, "%s/.extraction.info", bd->getShortPath());
	ExtractionInfoFileUnit *ret = new ExtractionInfoFileUnit(extractionInfoFname);
	free(extractionInfoFname);
	return ret;
}

CompressedFileExtractionUnit::CompressedFileExtractionUnit(FetchUnit * f)
{
	this->fetch = f;
	this->uri = f->relative_path();
}

CompressedFileExtractionUnit::CompressedFileExtractionUnit(const char *fname)
{
	this->fetch = NULL;
	this->uri = std::string(fname);
}

std::string CompressedFileExtractionUnit::HASH()
{
	if(this->hash == NULL) {
		if(this->fetch) {
			this->hash = new std::string(this->fetch->HASH());
		} else {
			char *Hash = hash_file(this->uri.c_str());
			this->hash = new std::string(Hash);
			free(Hash);
		}
	}
	return *this->hash;
};


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
	this->hash = new std::string(Hash);
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
	this->hash = new std::string(Hash);
	free(Hash);
}

bool FileCopyExtractionUnit::extract(Package * P, BuildDir * bd)
{
	const char *path = this->uri.c_str();
	std::unique_ptr < PackageCmd > pc(new PackageCmd(bd->getPath(), "cp"));
	pc->addArg("-pRLuf");

	if(path[0] == '/') {
		pc->addArg(path);
	} else {
		pc->addArgFmt("%s/%s", WORLD->getWorkingDir()->c_str(), path);
	}

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

GitExtractionUnit::GitExtractionUnit(const char *remote, const char *local,
				     std::string refspec, Package * P)
{
	this->uri = std::string(remote);

	char *source_dir = NULL;
	asprintf(&source_dir, "%s/source/%s", WORLD->getWorkingDir()->c_str(), local);
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
	const char *cwd = WORLD->getWorkingDir()->c_str();

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

	pc.reset(new PackageCmd(source_dir, "git"));
	// switch to refspec
	pc->addArg("checkout");
	pc->addArg("-q");
	pc->addArg("--detach");
	pc->addArg(this->refspec.c_str());
	if(!pc->Run(this->P))
		throw CustomException("Failed to checkout");

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

RequireFileUnit::RequireFileUnit(const char *fname)
{
	this->uri = std::string(fname);
	char *Hash = hash_file(fname);
	this->hash = std::string(Hash);
	free(Hash);
}

ExtractionInfoFileUnit::ExtractionInfoFileUnit(const char *fname)
{
	this->uri = std::string(fname);
	char *new_fname = NULL;
	asprintf(&new_fname, "%s.new", fname);
	char *Hash = hash_file(new_fname);
	this->hash = std::string(Hash);
	free(Hash);
	free(new_fname);
}

BuildInfoFileUnit::BuildInfoFileUnit(const char *fname, std::string hash)
{
	this->uri = std::string(fname);
	this->hash = hash;
}

OutputInfoFileUnit::OutputInfoFileUnit(const char *fname)
{
	this->uri = std::string(fname);
	char *Hash = hash_file(fname);
	this->hash = std::string(Hash);
	free(Hash);
}

bool FeatureValueUnit::print(std::ostream & out)
{
	if(!WORLD->isIgnoredFeature(this->feature)) {
		out << this->type() << " " << this->
		    feature << " " << this->value << std::endl;
	}
	return true;
}
