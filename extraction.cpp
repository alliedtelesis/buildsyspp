#include <buildsys.h>

static char * hash_file(const char *fname)
{
	char *cmd = NULL;
	asprintf(&cmd, "sha256sum %s", fname);
	FILE *f = popen(cmd,"r");
	free(cmd);
	char *Hash = (char *)calloc(65, sizeof(char));
	fread(Hash, sizeof(char), 64, f);
	fclose(f);
	return Hash;	
}

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
	return Commit;
}

static char * git_diff_hash(const char *gdir)
{
	char *pwd = getcwd(NULL,0);
	char *cmd = NULL;
	asprintf(&cmd, "git diff HEAD | git patch-id");
	chdir(gdir);
	FILE *f = popen(cmd, "r");
	free(cmd);
	char *Commit = (char *)calloc(41, sizeof(char));
	fread(Commit, sizeof(char), 40, f);
	chdir(pwd);
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
	
	int res = mkdir("dl", 0700);
	if((res < 0) && (errno != EEXIST))
	{
		throw CustomException("Error: Creating download directory");
	}
	argv = (char **)calloc(4, sizeof(char *));
	argv[0] = strdup("tar");
	argv[1] = strdup("xf");
	argv[2] = strdup(this->uri.c_str());
	
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
	char **argv = (char **)calloc(7, sizeof(char *));
	argv[0] = strdup("patch");
	asprintf(&argv[1], "-p%i", this->level);
	argv[2] = strdup("-stN");
	argv[3] = strdup("-i");
	
	argv[4] = strdup(this->uri.c_str());
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
	char **argv = NULL;
	
	argv = (char **)calloc(5, sizeof(char *));
	argv[0] = strdup("cp");
	argv[1] = strdup("-a");
	argv[2] = strdup(this->uri.c_str());
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

GitDirExtractionUnit::GitDirExtractionUnit(const char *git_dir)
{
	this->uri = std::string(git_dir);
	char *Hash = git_hash(git_dir);
	this->hash = std::string(Hash);
	free(Hash);
}

bool GitDirExtractionUnit::isDirty()
{
	char *pwd = getcwd(NULL,0);
	char *cmd = NULL;
	asprintf(&cmd, "git diff-index --quiet HEAD");
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


