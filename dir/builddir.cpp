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
	{
		const char *tmp = strrchr(pname, '/');
		if(tmp != NULL)
		{
			char *subpart = strdup(pname);
			subpart[tmp - pname] = '\0';
			asprintf(&path, "mkdir -p output/%s/staging/%s", gname, subpart);
			system(path);
			free(path);
			path = NULL;
			asprintf(&path, "mkdir -p output/%s/install/%s", gname, subpart);
			system(path);
			free(path);
			path = NULL;
			free(subpart);
		}
	}
	path = NULL;
	if(asprintf(&path, "mkdir -p output/%s/%s", gname, pname) == -1) {
		fprintf(stderr, "Failed creating output/global-name/package-name path: %s\n", strerror(errno));
	}
	res = system(path);
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
	std::string work_path = "";
	if(strrchr(name.c_str(),'/') != NULL)
	{
		const char *cname = name.c_str();
		const char *tmp = strrchr(cname,'/');
		if(*(tmp+1) != '\0') {
			work_path = this->path + "/" + std::string((tmp+1));
		}
	}
	if(work_path == "")
	{
		work_path = this->path + "/" + name;
	}
	this->work_src = work_path;
	this->work_build = work_path + "-build";
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
