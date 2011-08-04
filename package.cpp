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
		if(WORLD->forcedMode() && !WORLD->isForced(this->name))
		{
			this->bd = new BuildDir(this->name, false);
		} else {
			this->bd = new BuildDir(this->name, true);
		}
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


bool Package::build()
{
	struct timespec start, end;
	
	if(this->built == true)
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
		}
	}

	if(WORLD->forcedMode() && !WORLD->isForced(this->name))
	{
		return true;
	}
	
	clock_gettime(CLOCK_REALTIME, &start);
	
	if(this->bd != NULL)
	{
		log(this->name.c_str(), (char *)"Building ...");
		// Extract the dependency staging directories
		char *staging_dir = NULL;
		asprintf(&staging_dir, "output/%s/%s/staging", WORLD->getName().c_str(), this->name.c_str());
		log(this->name.c_str(), (char *)"Generating staging directory ...");
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
				char *pwd = getcwd(NULL, 0);
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
				free(pwd);
			}
			
			// Create the directory			
			{
				int res = mkdir(this->depsExtraction, 0700);
				if((res < 0) && (errno != EEXIST))
				{
					throw new DirException(this->depsExtraction, strerror(errno));
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
	
	char *pwd = getcwd(NULL, 0);
	
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
	
	free(pwd);

	clock_gettime(CLOCK_REALTIME, &end);
	
	this->run_secs = (end.tv_sec - start.tv_sec);
	
	log(this->name.c_str(), (char *)"Built in %d seconds",  this->run_secs);
	
	this->built = true;

	return true;
}
