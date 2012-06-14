#include <buildsys.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

BuildDir::BuildDir(std::string name)
{
	const char *gname = WORLD->getName().c_str();
	const char *pname = name.c_str();
	char *pwd = getcwd(NULL, 0);

	int res = mkdir("output", 0700);
	if((res < 0) && (errno != EEXIST))
	{
		throw DirException((char *)"output", strerror(errno));
	}
	
	char *path = NULL;
	if(asprintf(&path, "output/%s", gname) == -1) {
		fprintf(stderr, "output/global-name path: %s\n", strerror(errno));
	}
	res = mkdir(path, 0700);
	if((res < 0) && (errno != EEXIST))
	{
		throw DirException(path, strerror(errno));
	}
	free(path);
	path = NULL;
	if(asprintf(&path, "output/%s/staging", gname) == -1) {
		fprintf(stderr, "Failed creating output/global-name/staging path: %s\n", strerror(errno));
	}
	res = mkdir(path, 0700);
	if((res < 0) && (errno != EEXIST))
	{
		throw DirException(path, strerror(errno));
	}
	free(path);
	path = NULL;
	if(asprintf(&path, "output/%s/install", gname) == -1) {
		fprintf(stderr, "Failed creating output/global-name/install path: %s\n", strerror(errno));
	}
	res = mkdir(path, 0700);
	if((res < 0) && (errno != EEXIST))
	{
		throw DirException(path, strerror(errno));
	}
	free(path);
	path = NULL;
	if(asprintf(&path, "output/%s/%s", gname, pname) == -1) {
		fprintf(stderr, "Failed creating output/global-name/package-name path: %s\n", strerror(errno));
	}
	res = mkdir(path, 0700);
	if((res < 0) && (errno != EEXIST))
	{
		throw DirException(path, strerror(errno));
	}
	free(path);
	path = NULL;
	if(asprintf(&path, "%s/output/%s/%s/work", pwd, gname, pname) == -1) {
		fprintf(stderr, "Failed creating output/global-name/package-name/work path: %s\n", strerror(errno));
	}
	res = mkdir(path, 0700);
	if((res < 0) && (errno != EEXIST))
	{
		throw DirException(path, strerror(errno));
	}
	this->path = std::string(path);
	path = NULL;
	if(asprintf(&path, "output/%s/%s/work", gname, pname) == -1) {
		fprintf(stderr, "Failed creating output/global-name/package-name/work path: %s\n", strerror(errno));
	}
	this->rpath = std::string(path);
	this->work_src = this->path + "/" + name;
	this->work_build = this->path + "/" + name + "-build";
	path = NULL;
	if(asprintf(&path, "%s/output/%s/%s/new", pwd, gname, pname) == -1) {
		fprintf(stderr, "Failed creating output/global-name/package-name/new path: %s\n", strerror(errno));
	}
	res = mkdir(path, 0700);
	if((res < 0) && (errno != EEXIST))
	{
		throw DirException(path, strerror(errno));
	}
	free(path);
	path = NULL;
	if(asprintf(&path, "%s/output/%s/%s/staging", pwd, gname, pname) == -1) {
		fprintf(stderr, "Failed creating output/global-name/package-name/staging path: %s\n", strerror(errno));
	}
	res = mkdir(path, 0700);
	if(res < 0)
	{
		if(errno != EEXIST)
		{
			throw DirException(path, strerror(errno));
			
		}
	}
	this->staging = std::string(path);
	free(path);
	path = NULL;
	if(asprintf(&path, "%s/output/%s/%s/new/staging", pwd, gname, pname) == -1) {
		fprintf(stderr, "Failed creating output/global-name/package-name/new/staging path: %s\n", strerror(errno));
	}
	res = mkdir(path, 0700);
	if(res < 0)
	{
		if(errno != EEXIST)
		{
			throw DirException(path, strerror(errno));
			
		}
	}
	this->new_staging = std::string(path);
	free(path);
	path = NULL;
	if(asprintf(&path, "%s/output/%s/%s/new/install", pwd, gname, pname) == -1) {
		fprintf(stderr, "Failed creating output/global-name/package-name/new/install path: %s\n", strerror(errno));
	}
	res = mkdir(path, 0700);
	if(res < 0)
	{
		if(errno != EEXIST)
		{
			throw DirException(path, strerror(errno));
			
		}
	}
	this->new_install = std::string(path);	
	free(path);
	free(pwd);

}

void BuildDir::clean()
{
	char *cmd = NULL;
	asprintf(&cmd, "rm -fr %s", this->path.c_str());
	system(cmd);
	free(cmd);	
	int res = mkdir(this->path.c_str(),0700);
	if(res < 0)
	{
		// We should complain here
	}
}
