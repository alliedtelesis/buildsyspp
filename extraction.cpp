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

GitDirExtractionUnit::GitDirExtractionUnit(const char *git_dir, const char *to_dir, bool link)
{
	this->uri = std::string(git_dir);
	char *Hash = git_hash(git_dir);
	this->hash = std::string(Hash);
	free(Hash);
	this->toDir = std::string(to_dir);
	this->linked = link;
}

bool GitDirExtractionUnit::isDirty()
{
	char *pwd = getcwd(NULL,0);
	char *cmd = NULL;
	asprintf(&cmd, "git diff --quiet HEAD");
	chdir(this->uri.c_str());
	int res = system(cmd);
	free(cmd);
	chdir(pwd);
	return (res != 0);
}

std::string GitDirExtractionUnit::dirtyHash()
{
	char *phash = git_diff_hash(this->uri.c_str());
	std::string ret(phash);
	free(phash);
	return ret;
}

bool GitDirExtractionUnit::extract(Package *P, BuildDir *bd)
{
	char **argv = (char **)calloc(5, sizeof(char *));
	int argp = 0;
	if(this->linked)
	{
		argv[argp++] = strdup("ln");
		argv[argp++] = strdup("-sfT");
	} else {
		argv[argp++] = strdup("cp");
		argv[argp++] = strdup("-dpRuf");
	}
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
		// An error occured, try remove the file, then relink/copy
		char **rmargv = (char **)calloc(4, sizeof(char *));
		rmargv[0] = strdup("rm");
		rmargv[1] = strdup("-fr");
		rmargv[2] = strdup(this->toDir.c_str());
		log(P->getName().c_str(), (char *)"%s %s %s\n", rmargv[0], rmargv[1], rmargv[2]);
		if(run(P->getName().c_str(), (char *)"/bin/rm", rmargv , bd->getPath(), NULL) != 0)
			throw CustomException("Failed to link  or copy, could not remove target first");
		if(run(P->getName().c_str(), argv[0], argv, bd->getPath(), NULL) != 0)
			throw CustomException("Failed to link or copy, even after removing target first");
		free(rmargv[0]);
		free(rmargv[1]);
		free(rmargv[2]);
		free(rmargv);
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


