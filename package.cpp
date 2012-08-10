#include <buildsys.h>

std::list<Package *>::iterator Package::dependsStart()
{
	return this->depends.begin();
}
std::list<Package *>::iterator Package::dependsEnd()
{
	return this->depends.end();
}

BuildDir *Package::builddir()
{
	if(this->bd == NULL)
	{
		this->bd = new BuildDir(this->name);
	}
	
	return this->bd;
}

void Package::printLabel(std::ostream& out)
{
	out << "[label=\"";
	
	out << this->getName() << "\\n";
	out << "Cmds:" << this->commands.size() << "\\n";
	out << "Time: " << this->run_secs << "s";
	
	out << "\"]";
	
}

bool Package::process()
{
	if(this->visiting == true)
	{
		log(this->name.c_str(), (char *)"dependency loop!!!");
		return false;
	}
	if(this->processed == true) return true;
	
	this->visiting = true;

	log(this->name.c_str(), (char *)"Processing (%s)", this->file.c_str());

	this->build_description->add(new PackageFileUnit(this->file.c_str()));

	WORLD->getLua()->setGlobal(std::string("P"), this);

	WORLD->getLua()->processFile(file.c_str());

	this->processed = true;

	std::list<Package *>::iterator iter = this->depends.begin();
	std::list<Package *>::iterator end = this->depends.end();

	for(; iter != end; iter++)
	{
		if(!(*iter)->process())
		{
			log(this->name.c_str(),(char *)"dependency failed");
			return false;
		}
	}

	this->visiting = false;
	return true;
}

bool Package::extract()
{
	if(this->extracted)
	{
		return true;
	}

	if(this->bd)
	{
		// Dont bother extracting if we are running in forced mode, and this package isn't forced
		if(!(WORLD->forcedMode() && !WORLD->isForced(this->getName())))
		{
			// Create the new extraction info file
			char *exinfoFname = NULL;
			asprintf(&exinfoFname, "%s/.extraction.info.new", this->bd->getPath());
			std::ofstream exInfo(exinfoFname);
			this->Extract->print(exInfo);
			free(exinfoFname);
		
			char *cmd = NULL;
			asprintf(&cmd, "cmp -s %s/.extraction.info.new %s/.extraction.info", this->bd->getPath(), this->bd->getPath());
			int res = system(cmd);
			free(cmd);
			cmd = NULL;
		
			// if there are changes,
			if(res != 0 || this->codeUpdated)
			{	// Extract our source code
				log(this->name.c_str(), (char *)"Extracting sources and patching");
				this->Extract->extract(this, this->bd);
				//this->setCodeUpdated();
			}
			// mv the file into the regular place
			asprintf(&cmd, "mv %s/.extraction.info.new %s/.extraction.info", this->bd->getPath(), this->bd->getPath());
			system(cmd);
			free(cmd);
		}
	}

	std::list<Package *>::iterator iter = this->depends.begin();
	std::list<Package *>::iterator end = this->depends.end();

	for(; iter != end; iter++)
	{
		if(!(*iter)->extract())
		{
			log(this->name.c_str(),(char *)"dependency extract failed");
			return false;
		}
	}
	
	this->extracted = true;
	return true;
}

bool Package::extract_staging(const char *dir, std::list<std::string> *done)
{
	{
		std::list<std::string>::iterator dIt = done->begin();
		std::list<std::string>::iterator dEnd = done->end();

		for(; dIt != dEnd; dIt++)
		{
			if((*dIt).compare(this->name)==0) return true;
		}
	}

	std::list<Package *>::iterator dIt = this->depends.begin();
	std::list<Package *>::iterator dEnds = this->depends.end();
	
	for(; dIt != dEnds; dIt++)
	{
		if(!(*dIt)->extract_staging(dir, done))
			return false;
	}
	
	char *pwd = getcwd(NULL, 0);

	if(this->bd != NULL)
	{
		char **args = (char **)calloc(6, sizeof(char *));
		args[0] = strdup("pax");
		args[1] = strdup("-rf");
		asprintf(&args[2], "%s/output/%s/staging/%s.tar.bz2", pwd, WORLD->getName().c_str(), this->name.c_str());
		args[3] = strdup("-p");
		args[4] = strdup("p");

		if(run(this->name.c_str(), (char *)"pax", args, dir, NULL) != 0)
		{
			log(this->name.c_str(), (char *)"Failed to extract staging_dir");
			return false;
		}
	
		if(args != NULL)
		{
			int i = 0;
			while(args[i] != NULL)
			{
				free(args[i]);
				i++;
			}
			free(args);
		}
	}
	free(pwd);

	done->push_back(this->name);

	return true;
}

bool Package::extract_install(const char *dir, std::list<std::string> *done)
{
	{
		std::list<std::string>::iterator dIt = done->begin();
		std::list<std::string>::iterator dEnd = done->end();

		for(; dIt != dEnd; dIt++)
		{
			if((*dIt).compare(this->name)==0) return true;
		}
	}

	std::list<Package *>::iterator dIt = this->depends.begin();
	std::list<Package *>::iterator dEnds = this->depends.end();
	
	if(!this->intercept)
	{
		for(; dIt != dEnds; dIt++)
		{
			if(!(*dIt)->extract_install(dir, done))
				return false;
		}
	}
	
	char *pwd = getcwd(NULL, 0);

	if(this->bd != NULL)
	{
		char **args = (char **)calloc(6, sizeof(char *));
		if(this->installFile)
		{
			args[0] = strdup("cp");
			asprintf(&args[1], "%s/output/%s/install/%s", pwd, WORLD->getName().c_str(), this->installFile);
			args[2] = strdup(this->installFile);

			if(run(this->name.c_str(), (char *)"cp", args, dir, NULL) != 0)
			{
				log(this->name.c_str(), "Failed to copy %s (for install)\n", this->installFile);
				return false;
			}
		} else {
			args[0] = strdup("pax");
			args[1] = strdup("-rf");
			asprintf(&args[2], "%s/output/%s/install/%s.tar.bz2", pwd, WORLD->getName().c_str(), this->name.c_str());
			args[3] = strdup("-p");
			args[4] = strdup("p");
			if(run(this->name.c_str(), (char *)"pax", args, dir, NULL) != 0)
			{
				log(this->name.c_str(), "Failed to extract install_dir\n");
				return false;
			}
		}
	
		if(args != NULL)
		{
			int i = 0;
			while(args[i] != NULL)
			{
				free(args[i]);
				i++;
			}
			free(args);
		}
	}
	free(pwd);

	done->push_back(this->name);

	return true;
}

bool Package::canBuild()
{
	if(this->isBuilt())
	{
		return true;
	}
	
	std::list<Package *>::iterator dIt = this->depends.begin();
	std::list<Package *>::iterator dEnds = this->depends.end();
	
	if(dIt != dEnds)
	{
		for(; dIt != dEnds; dIt++)
		{
			if(!(*dIt)->isBuilt())
				return false;
		}
	}
	return true;
}

bool Package::isBuilt()
{
	bool ret = false;
#ifdef UNDERSCORE
	// lock
	us_mutex_lock(this->lock);
#endif
	ret = this->built;
#ifdef UNDERSCORE
	// unlock
	us_mutex_unlock(this->lock);
#endif
	return ret;
}

bool Package::isBuilding()
{
	bool ret = false;
#ifdef UNDERSCORE
	// lock
	us_mutex_lock(this->lock);
#endif
	ret = this->building;
#ifdef UNDERSCORE
	// unlock
	us_mutex_unlock(this->lock);
#endif
	return ret;
}

void Package::setBuilding()
{
#ifdef UNDERSCORE
	// lock
	us_mutex_lock(this->lock);
#endif
	this->building = true;
#ifdef UNDERSCORE
	// unlock
	us_mutex_unlock(this->lock);
#endif
}

bool Package::shouldBuild()
{
	// we dont need to build if we don't have a build directory
	if(this->bd == NULL) return false;
	// Add the extraction info file
	char *extractionInfoFname = NULL;
	asprintf(&extractionInfoFname, "%s/.extraction.info", this->bd->getShortPath());
	this->build_description->add(new ExtractionInfoFileUnit(extractionInfoFname));
	free(extractionInfoFname);
	
	// Add each of our dependencies build info files
	std::list<Package *>::iterator dIt = this->depends.begin();
	std::list<Package *>::iterator dEnds = this->depends.end();
	
	if(dIt != dEnds)
	{
		for(; dIt != dEnds; dIt++)
		{
			if((*dIt)->bd != NULL)
			{
				char *buildInfo_file = NULL;
				asprintf(&buildInfo_file, "%s/.build.info", (*dIt)->bd->getShortPath());
				this->build_description->add(new BuildInfoFileUnit(buildInfo_file));
				free(buildInfo_file);
			}
		}
	}
	
	// Create the new build info file
	char *buildInfoFname = NULL;
	asprintf(&buildInfoFname, "%s/.build.info.new", this->bd->getPath());
	std::ofstream buildInfo(buildInfoFname);
	this->build_description->print(buildInfo);
	free(buildInfoFname);

	// we need to rebuild if the code is updated
	if(this->codeUpdated) return true;
	char *pwd = getcwd(NULL, 0);
	// lets make sure the install file (still) exists
	bool ret = false;
	char *fname = NULL;
	if(this->installFile)
	{
		asprintf(&fname, "%s/output/%s/install/%s", pwd, WORLD->getName().c_str(), this->installFile);
	} else {
		asprintf(&fname, "%s/output/%s/install/%s.tar.bz2", pwd, WORLD->getName().c_str(), this->name.c_str());
	}
	FILE *f = fopen(fname, "r");
	if(f == NULL)
	{
		ret = true;
	} else {
		fclose(f);
	}
	free(fname);
	fname = NULL;
	// Now lets check that the staging file (still) exists
	asprintf(&fname, "%s/output/%s/staging/%s.tar.bz2", pwd, WORLD->getName().c_str(), this->name.c_str());
	f = fopen(fname, "r");
	if(f == NULL)
	{
		ret = true;
	} else {
		fclose(f);
	}
	free(fname);
	fname = NULL;
	
	char *cmd = NULL;
	asprintf(&cmd, "cmp -s %s/.build.info.new %s/.build.info", this->bd->getPath(), this->bd->getPath());
	int res = system(cmd);
	free(cmd);
	cmd = NULL;

	// if there are changes,
	if(res != 0 || (ret && !this->codeUpdated))
	{	
		// see if we can grab new staging/install files
		if(this->canFetchFrom() && WORLD->canFetchFrom())
		{
			ret = false;
			const char *ffrom = WORLD->fetchFrom().c_str();
			char *build_info_file = NULL;
			asprintf(&build_info_file, "%s/.build.info.new", this->bd->getPath());
			char *hash = hash_file(build_info_file);
			char *url = NULL;
			asprintf(&url, "%s/%s/%s/%s/usable", ffrom, WORLD->getName().c_str(), this->name.c_str(), hash);
			// try wget the file
			char *cmd = NULL;
			asprintf(&cmd, "wget -q %s -O output/%s/staging/%s.tar.bz2.ff\n", url, WORLD->getName().c_str(), this->name.c_str());
			res = system(cmd);
			if(res != 0)
			{
				ret = true;
			}
			asprintf(&url, "%s/%s/%s/%s/staging.tar.bz2", ffrom, WORLD->getName().c_str(), this->name.c_str(), hash);
			// try wget the file
			asprintf(&cmd, "wget -q %s -O output/%s/staging/%s.tar.bz2\n", url, WORLD->getName().c_str(), this->name.c_str());
			res = system(cmd);
			if(res != 0)
			{
				ret = true;
			}
			asprintf(&url, "%s/%s/%s/%s/install.tar.bz2", ffrom, WORLD->getName().c_str(), this->name.c_str(), hash);
			// try wget the file
			asprintf(&cmd, "wget -q %s -O output/%s/install/%s.tar.bz2\n", url, WORLD->getName().c_str(), this->name.c_str());
			res = system(cmd);
			if(res != 0)
			{
				ret = true;
			}
			free(cmd);
			free(url);
			free(hash);
			if(ret)
			{
				log(this->name.c_str(), "Could not optimize away building");
			}
			else
			{
				char *cmd = NULL;
				// mv the build info file into the regular place (faking that we have built this package)
				asprintf(&cmd, "mv %s/.build.info.new %s/.build.info", this->bd->getPath(), this->bd->getPath());
				system(cmd);
				free(cmd);
			}
		} else {
			// otherwise, make sure we get (re)built
			ret = true;
		}
	}
	
	free(pwd);
	
	return ret;
}

bool Package::build()
{
	struct timespec start, end;
	
	if(this->isBuilt())
	{
		return true;
	}
	
	std::list<Package *>::iterator dIt = this->depends.begin();
	std::list<Package *>::iterator dEnds = this->depends.end();
	
	if(dIt != dEnds)
	{
		for(; dIt != dEnds; dIt++)
		{
			if(!(*dIt)->build())
				return false;
			if((*dIt)->wasBuilt())
			{
				std::cout << "Dependency: " << (*dIt)->getName() << " was built" << std::endl;
			}
		}
	}

	bool sb = this->shouldBuild();

	if((WORLD->forcedMode() && !WORLD->isForced(this->name)) ||
		(!sb))
	{
#ifdef UNDERSCORE
		// lock
		us_mutex_lock(this->lock);
#endif
		log(this->name.c_str(), "Not required");
		// Just pretend we are built
		this->built = true;
#ifdef UNDERSCORE
		// unlock
		us_mutex_unlock(this->lock);
#endif
		WORLD->packageFinished(this);
		return true;
	}

	clock_gettime(CLOCK_REALTIME, &start);
	
	char *pwd = getcwd(NULL, 0);
	
	if(this->bd != NULL)
	{
		log(this->name.c_str(), (char *)"Building ...");
		// Extract the dependency staging directories
		char *staging_dir = NULL;
		asprintf(&staging_dir, "output/%s/%s/staging", WORLD->getName().c_str(), this->name.c_str());
		log(this->name.c_str(), (char *)"Generating staging directory ...");
		
		{ // Clean out the (new) staging/install directories
			char *cmd = NULL;
			asprintf(&cmd, "rm -fr %s/output/%s/%s/new/install/*", pwd, WORLD->getName().c_str(), this->name.c_str());
			system(cmd);
			free(cmd);
			cmd = NULL;
			asprintf(&cmd, "rm -fr %s/output/%s/%s/new/staging/*", pwd, WORLD->getName().c_str(), this->name.c_str());
			system(cmd);
			free(cmd);
			cmd = NULL;
			asprintf(&cmd, "rm -fr %s/output/%s/%s/staging/*", pwd, WORLD->getName().c_str(), this->name.c_str());
			system(cmd);
			free(cmd);
		}

		std::list<std::string> *done = new std::list<std::string>();
		for(dIt = this->depends.begin(); dIt != dEnds; dIt++)
		{
			if(!(*dIt)->extract_staging(staging_dir, done))
				return false;
		}
		log(this->name.c_str(), (char *)"Done (%d)", done->size());
		delete done;
		free(staging_dir);
		staging_dir = NULL;
	
		
		if(this->depsExtraction != NULL)
		{
			// Extract installed files to a given location
			log(this->name.c_str(), (char *)"Removing old install files ...");
			{
				char **args = (char **)calloc(4, sizeof(char *));
				args[0] = strdup("rm");
				args[1] = strdup("-fr");
				args[2] = strdup(this->depsExtraction);

				if(run(this->name.c_str(), (char *)"rm", args, pwd, NULL) != 0)
				{
					log(this->name.c_str(), (char *)"Failed to remove %s (pre-install)", this->depsExtraction);
					return false;
				}
		
				if(args != NULL)
				{
					int i = 0;
					while(args[i] != NULL)
					{
						free(args[i]);
						i++;
					}
					free(args);
				}
			}
			
			// Create the directory			
			{
				int res = mkdir(this->depsExtraction, 0700);
				if((res < 0) && (errno != EEXIST))
				{
					error(this->depsExtraction);
					return false;
				}
			}
			
			log(this->name.c_str(), (char *)"Extracting installed files from dependencies ...");
			done = new std::list<std::string>();
			for(dIt = this->depends.begin(); dIt != dEnds; dIt++)
			{
				if(!(*dIt)->extract_install(this->depsExtraction, done))
					return false;
			}
			delete done;
			log(this->name.c_str(), (char *)"Dependency install files extracted");
		}
		
		std::list<PackageCmd *>::iterator cIt = this->commands.begin();
		std::list<PackageCmd *>::iterator cEnd = this->commands.end();
		
		log(this->name.c_str(), (char *)"Running Commands");
		for(; cIt != cEnd; cIt++)
		{
			if(!(*cIt)->Run(this->name.c_str()))
				return false;
		}
		log(this->name.c_str(), (char *)"Done Commands");
	}
	
	log(this->name.c_str(), (char *)"BUILT");
	
	if(this->bd != NULL)
	{
		char **args = (char **)calloc(7, sizeof(char *));
		args[0] = strdup("pax");
		args[1] = strdup("-x");
		args[2] = strdup("cpio");
		args[3] = strdup("-wf");
		asprintf(&args[4], "%s/output/%s/staging/%s.tar.bz2", pwd, WORLD->getName().c_str(), this->name.c_str());
		args[5] = strdup(".");

		if(run(this->name.c_str(), (char *)"pax", args, this->bd->getNewStaging(), NULL) != 0)
		{
			log(this->name.c_str(), (char *)"Failed to compress staging directory");
			return false;
		}

		if(args != NULL)
		{
			int i = 0;
			while(args[i] != NULL)
			{
				free(args[i]);
				i++;
			}
			free(args);
		}
	}

	if(this->bd != NULL)
	{
		char **args = (char **)calloc(7, sizeof(char *));
		if(this->installFile != NULL)
		{
			std::cout << "Copying " << std::string(this->installFile) << " to install folder\n";
			args[0] = strdup("cp");
			args[1] = strdup(this->installFile);
			asprintf(&args[2], "%s/output/%s/install/%s", pwd, WORLD->getName().c_str(), this->installFile);
		
			if(run(this->name.c_str(), (char *)"cp", args, this->bd->getNewInstall(), NULL) != 0)
			{
				log(this->name.c_str(), (char *)"Failed to copy install file (%s) ", this->installFile);
				return false;
			}
		} else {
			args[0] = strdup("pax");
			args[1] = strdup("-x");
			args[2] = strdup("cpio");
			args[3] = strdup("-wf");
			asprintf(&args[4], "%s/output/%s/install/%s.tar.bz2", pwd, WORLD->getName().c_str(), this->name.c_str());
			args[5] = strdup(".");

			if(run(this->name.c_str(), (char *)"pax", args, this->bd->getNewInstall(), NULL) != 0)
			{
				log(this->name.c_str(), (char *)"Failed to compress install directory");
				return false;
			}
		}
		if(args != NULL)
		{
			int i = 0;
			while(args[i] != NULL)
			{
				free(args[i]);
				i++;
			}
			free(args);
		}
	}

	if(this->bd != NULL)
	{
		char *cmd = NULL;
		// mv the build info file into the regular place
		asprintf(&cmd, "mv %s/.build.info.new %s/.build.info", this->bd->getPath(), this->bd->getPath());
		system(cmd);
		free(cmd);
	}
	
	free(pwd);

	clock_gettime(CLOCK_REALTIME, &end);
	
	this->run_secs = (end.tv_sec - start.tv_sec);
	
	log(this->name.c_str(), (char *)"Built in %d seconds",  this->run_secs);
	
#ifdef UNDERSCORE
	// lock
	us_mutex_lock(this->lock);
#endif
	this->building = false;
	this->built = true;
	this->was_built = true;
#ifdef UNDERSCORE
	// unlock
	us_mutex_unlock(this->lock);
#endif

	WORLD->packageFinished(this);

	return true;
}
