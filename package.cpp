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

std::list < Package * >::iterator Package::dependsStart()
{
	return this->depends.begin();
}

std::list < Package * >::iterator Package::dependsEnd()
{
	return this->depends.end();
}

BuildDir *Package::builddir()
{
	if(this->bd == NULL) {
		this->bd = new BuildDir(this);
	}

	return this->bd;
}

char *Package::absolute_fetch_path(const char *location)
{
	char *src_path = NULL;
	const char *cwd = WORLD->getWorkingDir()->c_str();
	if(location[0] == '/' || !strncmp(location, "dl/", 3) || location[0] == '.') {
		asprintf(&src_path, "%s/%s", cwd, location);
	} else {
		asprintf(&src_path, "%s/%s/%s/%s", cwd, this->getOverlay().c_str(),
			 this->getName().c_str(), location);
	}
	return src_path;
}

char *Package::relative_fetch_path(const char *location)
{
	char *src_path = NULL;
	if(location[0] == '/' || !strncmp(location, "dl/", 3) || location[0] == '.') {
		src_path = strdup(location);
	} else {
		asprintf(&src_path, "%s/package/%s/%s", this->getOverlay().c_str(),
			 this->getName().c_str(), location);
	}
	return src_path;
}


void Package::resetBD()
{
	if(this->bd != NULL) {
		delete this->bd;
		this->bd = new BuildDir(this);
	}
}

void Package::printLabel(std::ostream & out)
{
	out << "[label=\"";

	out << this->getName() << "\\n";
	out << this->getNS()->getName().c_str() << "\\n";
	out << "Cmds:" << this->commands.size() << "\\n";
	out << "Time: " << this->run_secs << "s";

	out << "\"]";
}

bool Package::process()
{
	if(this->visiting == true) {
		log(this, "dependency loop!!!");
		return false;
	}
	if(this->processed == true)
		return true;

	this->visiting = true;

	log(this, "Processing (%s)", this->file.c_str());

	this->build_description->add(new PackageFileUnit(this->file.c_str()));

	if(!interfaceSetup(this->lua)) {
		error("interfaceSetup: Failed");
		exit(-1);
	}

	this->lua->setGlobal(std::string("P"), this);

	this->lua->processFile(file.c_str());

	this->processed = true;

	std::list < Package * >::iterator iter = this->dependsStart();
	std::list < Package * >::iterator end = this->dependsEnd();

	for(; iter != end; iter++) {
		if(!(*iter)->process()) {
			throw CustomException("dependency failure");
			return false;
		}
	}

	this->visiting = false;
	return true;
}

bool Package::extract_staging(const char *dir, std::list < std::string > *done)
{
	{
		std::list < std::string >::iterator dIt = done->begin();
		std::list < std::string >::iterator dEnd = done->end();

		for(; dIt != dEnd; dIt++) {
			if((*dIt).compare(this->name) == 0)
				return true;
		}
	}

	std::list < Package * >::iterator dIt = this->dependsStart();
	std::list < Package * >::iterator dEnds = this->dependsEnd();

	for(; dIt != dEnds; dIt++) {
		if(!(*dIt)->extract_staging(dir, done))
			return false;
	}

	const char *pwd = WORLD->getWorkingDir()->c_str();

	if(this->bd != NULL) {
		std::unique_ptr < PackageCmd > pc(new PackageCmd(dir, "pax"));
		pc->addArg("-rf");
		pc->addArgFmt("%s/output/%s/staging/%s.tar.bz2", pwd,
			      this->getNS()->getName().c_str(), this->name.c_str());
		pc->addArg("-p");
		pc->addArg("p");

		if(!pc->Run(this)) {
			log(this, "Failed to extract staging_dir");
			return false;
		}
	}

	done->push_back(this->name);

	return true;
}

bool Package::extract_install(const char *dir, std::list < std::string > *done)
{
	{
		std::list < std::string >::iterator dIt = done->begin();
		std::list < std::string >::iterator dEnd = done->end();

		for(; dIt != dEnd; dIt++) {
			if((*dIt).compare(this->name) == 0)
				return true;
		}
	}

	std::list < Package * >::iterator dIt = this->dependsStart();
	std::list < Package * >::iterator dEnds = this->dependsEnd();

	if(!this->intercept) {
		for(; dIt != dEnds; dIt++) {
			if(!(*dIt)->extract_install(dir, done))
				return false;
		}
	}

	const char *pwd = WORLD->getWorkingDir()->c_str();

	if(this->bd != NULL) {
		if(!this->installFiles.empty()) {
			std::list < std::string >::iterator it = this->installFiles.begin();
			std::list < std::string >::iterator end = this->installFiles.end();
			for(; it != end; it++) {
				std::unique_ptr < PackageCmd >
				    pc(new PackageCmd(dir, "cp"));
				pc->addArgFmt("%s/output/%s/install/%s", pwd,
					      this->getNS()->getName().c_str(),
					      (*it).c_str());
				pc->addArg(*it);

				if(!pc->Run(this)) {
					log(this, "Failed to copy %s (for install)\n",
					    (*it).c_str());
					return false;
				}
			}
		} else {
			std::unique_ptr < PackageCmd > pc(new PackageCmd(dir, "pax"));
			pc->addArg("-rf");
			pc->addArgFmt("%s/output/%s/install/%s.tar.bz2", pwd,
				      this->getNS()->getName().c_str(), this->name.c_str());
			pc->addArg("-p");
			pc->addArg("p");
			if(!pc->Run(this)) {
				log(this, "Failed to extract install_dir\n");
				return false;
			}
		}
	}

	done->push_back(this->name);

	return true;
}

bool Package::canBuild()
{
	if(this->isBuilt()) {
		return true;
	}

	std::list < Package * >::iterator dIt = this->dependsStart();
	std::list < Package * >::iterator dEnds = this->dependsEnd();

	if(dIt != dEnds) {
		for(; dIt != dEnds; dIt++) {
			if(!(*dIt)->isBuilt())
				return false;
		}
	}
	return true;
}

bool Package::isBuilt()
{
	bool ret = false;
	pthread_mutex_lock(&this->lock);
	ret = this->built;
	pthread_mutex_unlock(&this->lock);
	return ret;
}

bool Package::isBuilding()
{
	bool ret = false;
	pthread_mutex_lock(&this->lock);
	ret = this->building;
	pthread_mutex_unlock(&this->lock);
	return ret;
}

void Package::setBuilding()
{
	pthread_mutex_lock(&this->lock);
	this->building = true;
	pthread_mutex_unlock(&this->lock);
}

static bool ff_file(Package * P, const char *hash, const char *rfile, const char *path,
		    const char *fname, const char *fext)
{
	bool ret = false;
	char *url = NULL;
	asprintf(&url, "%s/%s/%s/%s/%s", WORLD->fetchFrom().c_str(),
		 P->getNS()->getName().c_str(), P->getName().c_str(), hash, rfile);
	char *cmd = NULL;
	asprintf(&cmd, "wget -q %s -O %s/%s%s", url, path, fname, fext);
	int res = system(cmd);
	if(res != 0) {
		log(P, "Failed to get %s", rfile);
		ret = true;
	}
	free(cmd);
	free(url);
	return ret;
}

void Package::updateBuildInfoHash()
{
	// populate the build.info hash
	char *build_info_file = NULL;
	asprintf(&build_info_file, "%s/.build.info.new", this->bd->getPath());
	char *hash = hash_file(build_info_file);
	this->buildinfo_hash = std::string(hash);
	log(this, "Hash: %s", hash);
	free(hash);
	free(build_info_file);
}

BuildUnit *Package::buildInfo()
{
	char *Info_file = NULL;
	BuildUnit *res;
	if(this->isHashingOutput()) {
		asprintf(&Info_file, "%s/.output.info", this->bd->getShortPath());
		res = new OutputInfoFileUnit(Info_file);
	} else {
		asprintf(&Info_file, "%s/.build.info", this->bd->getShortPath());
		if(this->buildinfo_hash.compare("") == 0) {
			log(this, "Package %s hash is empty\n", this->bd->getShortPath());
			return NULL;
		}
		res = new BuildInfoFileUnit(Info_file, this->buildinfo_hash);
	}
	free(Info_file);
	return res;
}

void Package::prepareBuildInfo()
{
	// Add the extraction info file
	this->build_description->add(this->Extract->extractionInfo(this, this->bd));

	// Add each of our dependencies build info files
	std::list < Package * >::iterator dIt = this->dependsStart();
	std::list < Package * >::iterator dEnds = this->dependsEnd();

	for(; dIt != dEnds; dIt++) {
		BuildUnit *bi = (*dIt)->buildInfo();
		if(!bi) {
			log(this, "bi is NULL :(");
			exit(-1);
		}
		this->build_description->add(bi);
	}

	// Create the new build info file
	char *buildInfoFname = NULL;
	asprintf(&buildInfoFname, "%s/.build.info.new", this->bd->getPath());
	std::ofstream buildInfo(buildInfoFname);
	this->build_description->print(buildInfo);
	buildInfo.close();
	free(buildInfoFname);
	this->updateBuildInfoHash();
}

void Package::updateBuildInfo(bool updateOutputHash)
{
	// mv the build info file into the regular place
	char *oldfname = NULL;
	char *newfname = NULL;
	asprintf(&oldfname, "%s/.build.info.new", this->bd->getPath());
	asprintf(&newfname, "%s/.build.info", this->bd->getPath());
	rename(oldfname, newfname);
	free(oldfname);
	free(newfname);

	if(updateOutputHash && this->isHashingOutput()) {
		// Hash the entire new path
		char *cmd = NULL;
		asprintf(&cmd,
			 "cd %s; find -type f -exec sha256sum {} \\; | sort -k 2 > %s/.output.info",
			 this->bd->getNewPath(), this->bd->getPath());
		system(cmd);
		free(cmd);
	}
}

bool Package::fetchFrom()
{
	bool ret = false;
	char *staging_dir = strdup(this->getNS()->getStagingDir().c_str());
	char *install_dir = strdup(this->getNS()->getInstallDir().c_str());
	const char *files[4][4] = {
		{"usable", staging_dir, this->name.c_str(), ".tar.bz2.ff"},
		{"staging.tar.bz2", staging_dir, this->name.c_str(), ".tar.bz2"},
		{"install.tar.bz2", install_dir, this->name.c_str(), ".tar.bz2"},
		{"output.info", this->bd->getPath(), ".output", ".info"}
	};
	const char *ffrom = WORLD->fetchFrom().c_str();
	log(this, "FF URL: %s/%s/%s/%s", ffrom, this->getNS()->getName().c_str(),
	    this->name.c_str(), this->buildinfo_hash.c_str());

	int count = 3;
	if(this->isHashingOutput()) {
		count = 4;
	}

	for(int i = 0; !ret && i < count; i++) {
		ret =
		    ff_file(this, this->buildinfo_hash.c_str(), files[i][0], files[i][1],
			    files[i][2], files[i][3]);
	}

	if(ret) {
		log(this, "Could not optimize away building");
	} else {
		log(this, "Build cache used");

		this->updateBuildInfo(false);
	}
	free(staging_dir);
	free(install_dir);
	return ret;
}

bool Package::shouldBuild()
{
	// we need to rebuild if the code is updated
	if(this->codeUpdated)
		return true;
	// we need to rebuild if installFiles is not empty
	if(!this->installFiles.empty())
		return true;

	const char *pwd = WORLD->getWorkingDir()->c_str();
	// lets make sure the install file (still) exists
	bool ret = false;
	char *fname = NULL;
	asprintf(&fname, "%s/output/%s/install/%s.tar.bz2", pwd,
		 this->getNS()->getName().c_str(), this->name.c_str());
	FILE *f = fopen(fname, "r");
	if(f == NULL) {
		ret = true;
	} else {
		fclose(f);
	}
	free(fname);
	fname = NULL;
	// Now lets check that the staging file (still) exists
	asprintf(&fname, "%s/output/%s/staging/%s.tar.bz2", pwd,
		 this->getNS()->getName().c_str(), this->name.c_str());
	f = fopen(fname, "r");
	if(f == NULL) {
		ret = true;
	} else {
		fclose(f);
	}
	free(fname);
	fname = NULL;

	char *cmd = NULL;
	asprintf(&cmd, "cmp -s %s/.build.info.new %s/.build.info", this->bd->getPath(),
		 this->bd->getPath());
	int res = system(cmd);
	free(cmd);
	cmd = NULL;

	// if there are changes,
	if(res != 0 || (ret && !this->codeUpdated)) {
		// see if we can grab new staging/install files
		if(this->canFetchFrom() && WORLD->canFetchFrom()) {
			ret = this->fetchFrom();
		} else {
			// otherwise, make sure we get (re)built
			ret = true;
		}
	}

	return ret;
}

bool Package::prepareBuildDirs()
{
	char *staging_dir = NULL;
	asprintf(&staging_dir, "output/%s/%s/staging", this->getNS()->getName().c_str(),
		 this->name.c_str());
	log(this, "Generating staging directory ...");

	{			// Clean out the (new) staging/install directories
		char *cmd = NULL;
		const char *pwd = WORLD->getWorkingDir()->c_str();
		asprintf(&cmd, "rm -fr %s/output/%s/%s/new/install/*", pwd,
			 this->getNS()->getName().c_str(), this->name.c_str());
		system(cmd);
		free(cmd);
		cmd = NULL;
		asprintf(&cmd, "rm -fr %s/output/%s/%s/new/staging/*", pwd,
			 this->getNS()->getName().c_str(), this->name.c_str());
		system(cmd);
		free(cmd);
		cmd = NULL;
		asprintf(&cmd, "rm -fr %s/output/%s/%s/staging/*", pwd,
			 this->getNS()->getName().c_str(), this->name.c_str());
		system(cmd);
		free(cmd);
	}

	std::list < std::string > *done = new std::list < std::string > ();
	std::list < Package * >::iterator dIt = this->dependsStart();
	std::list < Package * >::iterator dEnds = this->dependsEnd();
	for(; dIt != dEnds; dIt++) {
		if(!(*dIt)->extract_staging(staging_dir, done))
			return false;
	}
	log(this, "Done (%d)", done->size());
	delete done;
	free(staging_dir);
	staging_dir = NULL;
	return true;
}

bool Package::extractInstallDepends()
{
	if(this->depsExtraction == NULL) {
		return true;
	}
	// Extract installed files to a given location
	log(this, "Removing old install files ...");
	{
		std::unique_ptr < PackageCmd >
		    pc(new PackageCmd(WORLD->getWorkingDir()->c_str(), "rm"));
		pc->addArg("-fr");
		pc->addArg(this->depsExtraction);
		if(!pc->Run(this)) {
			log(this,
			    "Failed to remove %s (pre-install)", this->depsExtraction);
			return false;
		}
	}

	// Create the directory
	{
		int res = mkdir(this->depsExtraction, 0700);
		if((res < 0) && (errno != EEXIST)) {
			error(this->depsExtraction);
			return false;
		}
	}

	log(this, "Extracting installed files from dependencies ...");
	std::list < std::string > *done = new std::list < std::string > ();
	std::list < Package * >::iterator dIt = this->dependsStart();
	std::list < Package * >::iterator dEnds = this->dependsEnd();
	for(; dIt != dEnds; dIt++) {
		if(!(*dIt)->extract_install(this->depsExtraction, done))
			return false;
	}
	delete done;
	log(this, "Dependency install files extracted");
	return true;
}

bool Package::packageNewStaging()
{
	std::unique_ptr < PackageCmd > pc(new PackageCmd(this->bd->getNewStaging(), "pax"));
	pc->addArg("-x");
	pc->addArg("cpio");
	pc->addArg("-wf");
	pc->addArgFmt("%s/output/%s/staging/%s.tar.bz2", WORLD->getWorkingDir()->c_str(),
		      this->getNS()->getName().c_str(), this->name.c_str());
	pc->addArg(".");

	if(!pc->Run(this)) {
		log(this, "Failed to compress staging directory");
		return false;
	}
	return true;
}

bool Package::packageNewInstall()
{
	const char *pwd = WORLD->getWorkingDir()->c_str();
	if(!this->installFiles.empty()) {
		std::list < std::string >::iterator it = this->installFiles.begin();
		std::list < std::string >::iterator end = this->installFiles.end();
		for(; it != end; it++) {
			std::cout << "Copying " << *it << " to install folder\n";
			std::unique_ptr < PackageCmd >
			    pc(new PackageCmd(this->bd->getNewInstall(), "cp"));
			pc->addArg(*it);
			pc->addArgFmt("%s/output/%s/install/%s", pwd,
				      this->getNS()->getName().c_str(), (*it).c_str());

			if(!pc->Run(this)) {
				log(this, "Failed to copy install file (%s) ",
				    (*it).c_str());
				return false;
			}
		}
	} else {
		std::unique_ptr < PackageCmd >
		    pc(new PackageCmd(this->bd->getNewInstall(), "pax"));
		pc->addArg("-x");
		pc->addArg("cpio");
		pc->addArg("-wf");
		pc->addArgFmt("%s/output/%s/install/%s.tar.bz2", pwd,
			      this->getNS()->getName().c_str(), this->name.c_str());
		pc->addArg(".");

		if(!pc->Run(this)) {
			log(this, "Failed to compress install directory");
			return false;
		}
	}
	return true;
}

bool Package::build()
{
	struct timespec start, end;

	// Already build, pretend to successfully build
	if(this->isBuilt()) {
		return true;
	}

	std::list < Package * >::iterator dIt = this->dependsStart();
	std::list < Package * >::iterator dEnds = this->dependsEnd();

	/* Check our dependencies are already built, or build them */
	for(; dIt != dEnds; dIt++) {
		if(!(*dIt)->build())
			return false;
	}

	// Create the new extraction.info file
	this->Extract->prepareNewExtractInfo(this, this->bd);

	// Create the new build.info file
	this->prepareBuildInfo();

	// Check if building is required
	bool sb = this->shouldBuild();

	if((WORLD->forcedMode() && !WORLD->isForced(this->name)) || (!sb)) {
		pthread_mutex_lock(&this->lock);
		log(this, "Not required");
		// Just pretend we are built
		this->built = true;
		pthread_mutex_unlock(&this->lock);
		WORLD->packageFinished(this);
		return true;
	}

	clock_gettime(CLOCK_REALTIME, &start);

	if(this->Extract->extractionRequired(this, this->bd)) {
		log(this, "Extracting ...");
		if(!this->Extract->extract(this, this->bd)) {
			return false;
		}
	}

	log(this, "Building ...");
	// Clean new/{staging,install}, Extract the dependency staging directories
	if(!this->prepareBuildDirs()) {
		return false;
	}

	if(!this->extractInstallDepends()) {
		return false;
	}

	std::list < PackageCmd * >::iterator cIt = this->commands.begin();
	std::list < PackageCmd * >::iterator cEnd = this->commands.end();

	log(this, "Running Commands");
	for(; cIt != cEnd; cIt++) {
		if(!(*cIt)->Run(this))
			return false;
	}
	log(this, "Done Commands");

	log(this, "BUILT");

	if(!this->packageNewStaging()) {
		return false;
	}

	if(!this->packageNewInstall()) {
		return false;
	}

	this->updateBuildInfo();

	clock_gettime(CLOCK_REALTIME, &end);

	this->run_secs = (end.tv_sec - start.tv_sec);

	log(this, "Built in %d seconds", this->run_secs);
	pthread_mutex_lock(&this->lock);
	this->building = false;
	this->built = true;
	this->was_built = true;
	pthread_mutex_unlock(&this->lock);
	WORLD->packageFinished(this);

	return true;
}
