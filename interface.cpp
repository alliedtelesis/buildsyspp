#include <buildsys.h>

#define CHECK_ARGUMENT(F,N,T)		if(!lua_is##T(L, N)) throw CustomException("" #F "() requires a " #T " as argument " #N );

#define CHECK_ARGUMENT_TYPE(F,N,T,V) CHECK_ARGUMENT(F,N,table) \
	lua_pushstring(L, #T); \
	lua_gettable(L, N); \
	if(!lua_isstring(L, -1) || strcmp(lua_tostring(L, -1), "yes") != 0) throw CustomException("" #F "() requires an object of type " #T " as argument " #N); \
	lua_pop(L, 1); \
	lua_pushstring(L, "data"); \
	lua_gettable(L, N); \
	if(!lua_islightuserdata(L, -1)) throw CustomException("" #F "() requires data of argument " #N " to contain something of type " #T ""); \
	T * V = (T *)lua_topointer(L, -1); \
	lua_pop(L, 1);

int li_bd_fetch(lua_State *L)
{
	/* the first argument is the table, and is implicit */
	int argc = lua_gettop(L);
	if(argc < 1)
	{
		throw CustomException("fetch() requires at least 2 arguments");
	}
	if(!lua_istable(L, 1)) throw CustomException("fetch() must be called using : not .");
	if(!lua_isstring(L, 2)) throw CustomException("fetch() expects a string as the first argument");
	if(!lua_isstring(L, 3)) throw CustomException("fetch() expects a string as the second argument");

	lua_getglobal(L, "P");
	Package *P = (Package *)lua_topointer(L, -1);

	CHECK_ARGUMENT_TYPE("fetch",1,BuildDir,d);
	
	char *location = strdup(lua_tostring(L, 2));
	const char *method = lua_tostring(L, 3);
	
	char **argv = NULL;
	
	if(WORLD->forcedMode() && !WORLD->isForced(P->getName()))
	{
		return 0;
	}
	
	if(strcmp(method, "dl") == 0)
	{
		int res = mkdir("dl", 0700);
		if((res < 0) && (errno != EEXIST))
		{
			throw CustomException("Error: Creating download directory");
		}
		bool decompress = false;
		if(argc > 3)
		{
			decompress = lua_toboolean(L, 4);
		}
		bool get = true;
		char *fname = strrchr(location, '/');
		if(fname != NULL)
		{
			fname++;
			get = false;
			char *t = fname;
			if(decompress)
			{
				fname = strdup(fname);
				char *ext = strrchr(fname, '.');
				if(ext != NULL) ext[0] = '\0';
			}
			char *pwd = getcwd(NULL, 0);
			char *fpath = NULL;
			asprintf(&fpath, "%s/dl/%s", pwd, fname);
			free(pwd);
			FILE *f = fopen(fpath, "r");
			if(f == NULL)
			{
				get = true;
			} else {
				fclose(f);
			}
			free(fpath);
			if(decompress) free(fname);
			fname = t;
		}
		if(get)
		{
			argv = (char **)calloc(4, sizeof(char *));
			argv[0] = strdup("wget");
			argv[1] = strdup(location);
			if(run(P->getName().c_str(), (char *)"wget", argv , "dl", NULL) != 0)
				throw CustomException("Failed to fetch file");
			if(decompress)
			{
				// We want to run a command on this file
				char *cmd = NULL;
				char *ext = strrchr(fname, '.');
				if(ext == NULL)
				{
					log(P->getName().c_str(), (char *)"Could not guess decompression based on extension: %s\n", fname);
				}
				
				if(strcmp(ext, ".bz2")==0)
				{
					asprintf(&cmd, "bunzip2 -d dl/%s", fname);
				} else
				if(strcmp(ext, ".gz")==0)
				{
					asprintf(&cmd, "gunzip -d dl/%s", fname);
				}
				system(cmd);
				free(cmd);
			}
		}
	} else if(strcmp(method, "git") == 0) {
		argv = (char **)calloc(4, sizeof(char *));
		argv[0] = strdup("git");
		argv[1] = strdup("clone");
		argv[2] = strdup(location);
		if(run(P->getName().c_str(), (char *)"git", argv , d->getPath(), NULL) != 0)
			throw CustomException("Failed to git clone");
		fprintf(stderr, "Git clone, considering code updated\n");
		P->setCodeUpdated();
	} else if(strcmp(method, "linkgit") == 0) {
		argv = (char **)calloc(5, sizeof(char *));
		argv[0] = strdup("ln");
		argv[1] = strdup("-sf");
		if(location[0] == '.')
		{
			char *pwd = getcwd(NULL, 0);
			asprintf(&argv[2], "%s/%s", pwd, location);
			free(pwd);
		} else {
			argv[2] = strdup(location);
		}
		argv[3] = strdup(".");
		if(run(P->getName().c_str(), (char *)"ln", argv , d->getPath(), NULL) != 0)
		{
			throw CustomException("Failed to symbolically link to git directory");
		}
		GitDirExtractionUnit *gdeu = new GitDirExtractionUnit(argv[2]);
		P->extraction()->add(gdeu);
	} else if(strcmp(method, "link") == 0) {
		argv = (char **)calloc(5, sizeof(char *));
		argv[0] = strdup("ln");
		argv[1] = strdup("-sf");
		if(location[0] == '.')
		{
			char *pwd = getcwd(NULL, 0);
			asprintf(&argv[2], "%s/%s", pwd, location);
			free(pwd);
		} else {
			argv[2] = strdup(location);
		}
		argv[3] = strdup(".");
		if(run(P->getName().c_str(), (char *)"ln", argv , d->getPath(), NULL) != 0)
		{
			// An error occured, try remove the file, then relink
			char **rmargv = (char **)calloc(4, sizeof(char *));
			rmargv[0] = strdup("rm");
			rmargv[1] = strdup("-fr");
			char *l = strdup(argv[2]);
			char *l2 = strrchr(l,'/');
			if(l2[1] == '\0')
			{
				l2[0] = '\0';
				l2 = strrchr(l,'/');
			}
			l2++;
			rmargv[2] = strdup(l2);
			log(P->getName().c_str(), (char *)"%s %s %s\n", rmargv[0], rmargv[1], rmargv[2]);
			if(run(P->getName().c_str(), (char *)"/bin/rm", rmargv , d->getPath(), NULL) != 0)
				throw CustomException("Failed to ln (symbolically), could not remove target first");
			if(run(P->getName().c_str(), (char *)"ln", argv, d->getPath(), NULL) != 0)
				throw CustomException("Failed to ln (symbolically), even after removing target first");
			free(rmargv[0]);
			free(rmargv[1]);
			free(rmargv[2]);
			free(rmargv);
			free(l);
		}
		fprintf(stderr, "Linked data in, considering updated\n");
		P->setCodeUpdated();
	} else if(strcmp(method, "copyfile") == 0) {
		char *file_path = NULL;
		if(location[0] == '/')
		{
			file_path = strdup(location);
		} else {
			char *pwd = getcwd(NULL, 0);
			if(location[0] == '.')
			{
				asprintf(&file_path, "%s/%s", pwd, location);
			} else {
				asprintf(&file_path, "%s/package/%s/%s", pwd, P->getName().c_str(), location);
			}
			free(pwd);
		}
		P->extraction()->add(new FileCopyExtractionUnit(file_path));
		free(file_path);
	} else if(strcmp(method, "copygit") == 0) {
		char *src_path = NULL;
		if(location[0] == '/')
		{
			src_path = strdup(location);
		} else {
			char *pwd = getcwd(NULL, 0);
			if(location[0] == '.')
			{
				asprintf(&src_path, "%s/%s", pwd, location);
			} else {
				asprintf(&src_path, "%s/package/%s/%s", pwd, P->getName().c_str(), location);
			}
			free(pwd);
		}
		GitDirExtractionUnit *gdeu = new GitDirExtractionUnit(src_path);
		P->extraction()->add(gdeu);
		argv = (char **)calloc(5, sizeof(char *));
		argv[0] = strdup("cp");
		argv[1] = strdup("-dpRuf");
		argv[2] = src_path;
		argv[3] = strdup(".");
		if(run(P->getName().c_str(), (char *)"cp", argv , d->getPath(), NULL) != 0)
			throw CustomException("Failed to copy (recursively)");		
	} else if(strcmp(method, "sm") == 0) {
		argv = (char **)calloc(5, sizeof(char *));
		argv[0] = strdup("ln");
		argv[1] = strdup("-sfT");
		if(location[0] == '.')
		{
			char *pwd = getcwd(NULL, 0);
			asprintf(&argv[2], "%s/%s", pwd, location);
			free(pwd);
		} else {
			argv[2] = strdup(location);
		}
		argv[3] = strdup(d->getWorkSrc());
		if(run(P->getName().c_str(), (char *)"ln", argv , d->getPath(), NULL) != 0)
		{
			// An error occured, try remove the file, then relink
			char **rmargv = (char **)calloc(4, sizeof(char *));
			rmargv[0] = strdup("rm");
			rmargv[1] = strdup("-fr");
			char *l = strdup(argv[2]);
			char *l2 = strrchr(l,'/');
			if(l2[1] == '\0')
			{
				l2[0] = '\0';
				l2 = strrchr(l,'/');
			}
			l2++;
			rmargv[2] = strdup(l2);
			log(P->getName().c_str(), (char *)"%s %s %s\n", rmargv[0], rmargv[1], rmargv[2]);
			if(run(P->getName().c_str(), (char *)"/bin/rm", rmargv , d->getPath(), NULL) != 0)
				throw CustomException("Failed to ln (symbolically), could not remove target first");
			if(run(P->getName().c_str(), (char *)"ln", argv, d->getPath(), NULL) != 0)
				throw CustomException("Failed to ln (symbolically), even after removing target first");
			free(rmargv[0]);
			free(rmargv[1]);
			free(rmargv[2]);
			free(rmargv);
			free(l);
		}
		if (mkdir(d->getWorkBuild(), 0777) && errno != EEXIST)
		{
			// An error occured, try remove the direcotry, then recreate
			char **rmargv = (char **)calloc(4, sizeof(char *));
			rmargv[0] = strdup("rm");
			rmargv[1] = strdup("-fr");
			rmargv[2] = strdup(d->getWorkBuild());
			rmargv[3] = NULL;
			log(P->getName().c_str(), (char *)"%s %s %s\n", rmargv[0], rmargv[1], rmargv[2]);
			if(run(P->getName().c_str(), (char *)"/bin/rm", rmargv , d->getPath(), NULL) != 0)
				throw CustomException("Failed to mkdir, could not remove target first");
			if (mkdir(d->getWorkBuild(), 0777))
				throw CustomException("Failed to mkdir, even after removing target first");
			free(rmargv[0]);
			free(rmargv[1]);
			free(rmargv[2]);
			free(rmargv);
		}
		GitDirExtractionUnit *gdeu = new GitDirExtractionUnit(argv[2]);
		P->extraction()->add(gdeu);
	} else if(strcmp(method, "copy") == 0) {
		argv = (char **)calloc(5, sizeof(char *));
		argv[0] = strdup("cp");
		argv[1] = strdup("-dpRuf");
		if(location[0] == '/')
		{
			argv[2] = strdup(location);
		}
		else
		{
			char *pwd = getcwd(NULL, 0);
			if(location[0] == '.')
			{
				asprintf(&argv[2], "%s/%s", pwd, location);
			} else {
				asprintf(&argv[2], "%s/package/%s/%s", pwd, P->getName().c_str(), location);
			}
			free(pwd);
		}
		argv[3] = strdup(".");
		if(run(P->getName().c_str(), (char *)"cp", argv , d->getPath(), NULL) != 0)
			throw CustomException("Failed to copy (recursively)");
		fprintf(stderr, "Copied data in, considering code updated\n");
		P->setCodeUpdated();
	} else if(strcmp(method, "deps") == 0) {
		char *path = NULL;
		asprintf(&path, "%s/%s", d->getPath(), location);
		// record this directory (need to complete this operation later)
		lua_getglobal(L, "P");
		Package *P = (Package *)lua_topointer(L, -1);
		P->setDepsExtract(path);
		fprintf(stderr, "Will add installed files, considering code updated\n");
		P->setCodeUpdated();
	} else {
		throw CustomException("Unsupported fetch method");
	}
	
	free(location);
	
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
	
	return 0;
}

int li_bd_extract(lua_State *L)
{
	if(lua_gettop(L) != 2) throw CustomException("extract() requires exactly 1 argument");
	if(!lua_istable(L, 1)) throw CustomException("extract() must be called using : not .");
	if(!lua_isstring(L, 2)) throw CustomException("extract() expects a string as the only argument");

	lua_pushstring(L, "data");
	lua_gettable(L, 1);
	if(!lua_islightuserdata(L, -1)) throw CustomException("extract() must be called using : not .");
	BuildDir *d = (BuildDir *)lua_topointer(L, -1);
	lua_pop(L, 1);

	lua_getglobal(L, "P");
	Package *P = (Package *)lua_topointer(L, -1);

	if(WORLD->forcedMode() && !WORLD->isForced(P->getName()))
	{
		return 0;
	}
	
	const char *fName = lua_tostring(L, 2);
	char *realFName = NULL;
	if(!strncmp(fName, "dl/", 3))
	{
		char *pwd = getcwd(NULL, 0);
		asprintf(&realFName, "%s/%s", pwd, fName);
		free(pwd);
	} else {
		asprintf(&realFName, "%s/%s", d->getPath(), fName);
	}
	
	TarExtractionUnit *teu = new TarExtractionUnit(realFName);
	P->extraction()->add(teu);

	free(realFName);
	return 0;
}

int li_bd_cmd(lua_State *L)
{
	int argc = lua_gettop(L);
	if(argc < 4) throw CustomException("cmd() requires at least 3 arguments");
	if(argc > 5) throw CustomException("cmd() requires at most 4 arguments");
	if(!lua_istable(L, 1)) throw CustomException("cmd() must be called using : not .");
	if(!lua_isstring(L, 2)) throw CustomException("cmd() expects a string as the first argument");
	if(!lua_isstring(L, 3)) throw CustomException("cmd() expects a string as the second argument");
	if(!lua_istable(L, 4)) throw CustomException("cmd() expects a table of strings as the third argument");
	if(argc == 5 && !lua_istable(L, 5)) throw CustomException("cmd() expects a table of strings as the fourth argument, if present");

	CHECK_ARGUMENT_TYPE("cmd",1,BuildDir,d);

	lua_getglobal(L, "P");
	Package *P = (Package *)lua_topointer(L, -1);

	char *path = NULL;
	const char *dir = lua_tostring(L, 2);
	if (dir[0] == '/')
		asprintf(&path, "%s", dir);
	else
		asprintf(&path, "%s/%s", d->getPath(), dir);
	const char *app = lua_tostring(L, 3);
	
	PackageCmd *pc = new PackageCmd(path, app);
	
	pc->addArg(app);

	lua_pushnil(L);  /* first key */
	while (lua_next(L, 4) != 0) {
		/* uses 'key' (at index -2) and 'value' (at index -1) */
		if(lua_type(L, -1) != LUA_TSTRING) throw CustomException("cmd() requires a table of strings as the third argument\n");
		pc->addArg(lua_tostring(L, -1));
		/* removes 'value'; keeps 'key' for next iteration */
		lua_pop(L, 1);
	}

	if(argc == 5)
	{
		lua_pushnil(L);  /* first key */
		while (lua_next(L, 5) != 0) {
			/* uses 'key' (at index -2) and 'value' (at index -1) */
			if(lua_type(L, -1) != LUA_TSTRING) throw CustomException("cmd() requires a table of strings as the fourth argument\n");
			pc->addEnv(lua_tostring(L, -1));
			/* removes 'value'; keeps 'key' for next iteration */
			lua_pop(L, 1);
		}
	}

	P->addCommand(pc);

	free(path);

	return 0;
}

int li_bd_shell(lua_State *L)
{
	/* bd:shell(dir, command [, env]) */
	
	int argc = lua_gettop(L);
	if(argc < 3) throw CustomException("shell() requires at least 2 arguments");
	if(argc > 4) throw CustomException("shell() requires at most 3 arguments");
	if(!lua_istable(L, 1)) throw CustomException("shell() must be called using : not .");
	if(!lua_isstring(L, 2)) throw CustomException("shell() expects a string as the first argument");
	if(!lua_isstring(L, 3)) throw CustomException("shell() expects a string as the second argument");
	if(argc >= 4 && !lua_istable(L, 4)) throw CustomException("shell() expects a table of strings as the third argument, if present");

	CHECK_ARGUMENT_TYPE("shell",1,BuildDir,d);

	lua_getglobal(L, "P");
	Package *P = (Package *)lua_topointer(L, -1);

	std::string path = lua_tostring(L, 2);
	if (path[0] != '/')
	{
		std::string dir = d->getPath();
		path = dir + "/" + path;
	}

	std::string cmd = lua_tostring(L, 3);
	
	PackageCmd *pc = new PackageCmd(path, "bash");

	pc->addArg("bash");
	pc->addArg("-c");
	pc->addArg(cmd);

	if(argc == 4)
	{
		lua_pushnil(L);  /* first key */
		while (lua_next(L, 4) != 0) {
			/* uses 'key' (at index -2) and 'value' (at index -1) */
			if(lua_type(L, -1) != LUA_TSTRING) throw CustomException("shell() requires a table of strings as the third argument\n");
			pc->addEnv(lua_tostring(L, -1));
			/* removes 'value'; keeps 'key' for next iteration */
			lua_pop(L, 1);
		}
	}

	P->addCommand(pc);

	return 0;
}

int li_bd_autoreconf(lua_State *L)
{
	/* bd:autoreconf() */

	int argc = lua_gettop(L);
	if(argc != 1) throw CustomException("autoreconf() requires zero arguments");

	CHECK_ARGUMENT_TYPE("autoreconf",1,BuildDir,d);

	lua_getglobal(L, "P");
	Package *P = (Package *)lua_topointer(L, -1);

	PackageCmd *pc = new PackageCmd(d->getWorkSrc(), "autoreconf");

	if (WORLD->areSkipConfigure())
		pc->skipCommand();
	
	std::string incdir = d->getStaging();
	incdir += "/usr/local/aclocal";

	pc->addArg("autoreconf");
	pc->addArg("-i");
	pc->addArg("-B");
	pc->addArg(incdir);

	P->addCommand(pc);

	return 0;
}

int li_bd_configure(lua_State *L)
{
	/* bd:configure(args [, env [, dir]]) */

	int argc = lua_gettop(L);
	if(argc < 2) throw CustomException("configure() requires at least 1 arguments");
	if(argc > 4) throw CustomException("configure() requires at most 3 arguments");
	if(!lua_istable(L, 1)) throw CustomException("configure() must be called using : not .");
	if(!lua_istable(L, 2)) throw CustomException("configure() expects a table of string as the first argument");
	if(argc >= 3 && !lua_istable(L, 3)) throw CustomException("configure() expects a table of strings as the second argument, if present");
	if(argc >= 4 && !lua_isstring(L, 4)) throw CustomException("configure() expects a string as the third argument, if present");

	CHECK_ARGUMENT_TYPE("configure",1,BuildDir,d);

	lua_getglobal(L, "P");
	Package *P = (Package *)lua_topointer(L, -1);

	std::string path = d->getWorkBuild();
	if (argc >= 4)
	{
		std::string dir = lua_tostring(L, 4);
		if (dir[0] == '/')
			path = dir;
		else
			path += "/" + dir;
	}

	std::string app = d->getWorkSrc();
	app += "/configure";

	PackageCmd *pc = new PackageCmd(path, app);
	
	if (WORLD->areSkipConfigure())
		pc->skipCommand();
	
	pc->addArg(app);

	lua_pushnil(L);  /* first key */
	while (lua_next(L, 2) != 0) {
		/* uses 'key' (at index -2) and 'value' (at index -1) */
		if(lua_type(L, -1) != LUA_TSTRING) throw CustomException("configure() requires a table of strings as the first argument\n");
		pc->addArg(lua_tostring(L, -1));
		/* removes 'value'; keeps 'key' for next iteration */
		lua_pop(L, 1);
	}

	if(argc >= 3)
	{
		lua_pushnil(L);  /* first key */
		while (lua_next(L, 3) != 0) {
			/* uses 'key' (at index -2) and 'value' (at index -1) */
			if(lua_type(L, -1) != LUA_TSTRING) throw CustomException("configure() requires a table of strings as the second argument\n");
			pc->addEnv(lua_tostring(L, -1));
			/* removes 'value'; keeps 'key' for next iteration */
			lua_pop(L, 1);
		}
	}

	P->addCommand(pc);

	return 0;
}

int li_bd_make(lua_State *L)
{
	/* bd:make(args [, env [,dir]]) */

	int argc = lua_gettop(L);
	if(argc < 2) throw CustomException("make() requires at least 1 arguments");
	if(argc > 4) throw CustomException("make() requires at most 3 arguments");
	if(!lua_istable(L, 1)) throw CustomException("make() must be called using : not .");
	if(!lua_istable(L, 2)) throw CustomException("make() expects a table of string as the first argument");
	if(argc >= 3 && !lua_istable(L, 3)) throw CustomException("make() expects a table of strings as the second argument, if present");
	if(argc >= 4 && !lua_isstring(L, 4)) throw CustomException("make() expects a string as the third argument, if present");

	CHECK_ARGUMENT_TYPE("make",1,BuildDir,d);

	lua_getglobal(L, "P");
	Package *P = (Package *)lua_topointer(L, -1);

	std::string path = d->getWorkBuild();
	if (argc >= 4)
	{
		std::string dir = lua_tostring(L, 4);
		if (dir[0] == '/')
			path = dir;
		else
			path += "/" + dir;
	}
	
	PackageCmd *pc = new PackageCmd(path, "make");
	
	pc->addArg("make");
	try {
		std::string value = "-j" + WORLD->getFeature("job-limit");
		pc->addArg(value);
	}
	catch(NoKeyException &E) {
	}
	try {
		std::string value = "-l" + WORLD->getFeature("load-limit");
		pc->addArg(value);
	}
	catch(NoKeyException &E) {
	}

	lua_pushnil(L);  /* first key */
	while (lua_next(L, 2) != 0) {
		/* uses 'key' (at index -2) and 'value' (at index -1) */
		if(lua_type(L, -1) != LUA_TSTRING) throw CustomException("make() requires a table of strings as the first argument\n");
		pc->addArg(lua_tostring(L, -1));
		/* removes 'value'; keeps 'key' for next iteration */
		lua_pop(L, 1);
	}

	if(argc >= 3)
	{
		lua_pushnil(L);  /* first key */
		while (lua_next(L, 3) != 0) {
			/* uses 'key' (at index -2) and 'value' (at index -1) */
			if(lua_type(L, -1) != LUA_TSTRING) throw CustomException("make() requires a table of strings as the second argument\n");
			pc->addEnv(lua_tostring(L, -1));
			/* removes 'value'; keeps 'key' for next iteration */
			lua_pop(L, 1);
		}
	}

	P->addCommand(pc);

	return 0;
}


int li_bd_patch(lua_State *L)
{
	if(lua_gettop(L) != 4) throw CustomException("patch() requires exactly 3 arguments");
	if(!lua_istable(L, 1)) throw CustomException("patch() must be called using : not .");
	if(!lua_isstring(L, 2)) throw CustomException("patch() expects a string as the first argument");
	if(!lua_isnumber(L, 3)) throw CustomException("patch() expects a number as the second argument");
	if(!lua_istable(L, 4)) throw CustomException("patch() expects a table of strings as the third argument");
	
	lua_getglobal(L, "P");
	Package *P = (Package *)lua_topointer(L, -1);
	
	if(WORLD->forcedMode() && !WORLD->isForced(P->getName()))
	{
		return true;
	}

	CHECK_ARGUMENT_TYPE("patch",1,BuildDir,d);

	char *patch_path = NULL;
	asprintf(&patch_path, "%s/%s", d->getPath(), lua_tostring(L, 2));

	int patch_depth = lua_tonumber(L, 3);


	char *pwd = getcwd(NULL, 0);
		
	lua_pushnil(L);  /* first key */
	while (lua_next(L, 4) != 0) {
		/* uses 'key' (at index -2) and 'value' (at index -1) */
		{
			char *uri = NULL;
			if(lua_type(L, -1) != LUA_TSTRING) throw CustomException("patch() requires a table of strings as the third argument\n");
			if(!strncmp(lua_tostring(L, -1), "dl/", 3))
			{
				asprintf(&uri, "%s/%s", pwd,  lua_tostring(L, -1));
			} else {
				asprintf(&uri, "%s/package/%s/%s", pwd, P->getName().c_str(), lua_tostring(L, -1));
			}
			PatchExtractionUnit *peu = new PatchExtractionUnit(patch_depth, patch_path, uri);
			P->extraction()->add(peu);

			free(uri);
		}
		/* removes 'value'; keeps 'key' for next iteration */
		lua_pop(L, 1);
	}
	
	free(pwd);
	free(patch_path);

	return 0;
}

int li_bd_installfile(lua_State *L)
{
	if(lua_gettop(L) != 2) throw CustomException("installfile() requires exactly 1 argument");
	if(!lua_istable(L, 1)) throw CustomException("installfile() must be called using : not .");
	if(!lua_isstring(L, 2)) throw CustomException("installfile() expects a string as the only argument");

	lua_getglobal(L, "P");
	Package *P = (Package *)lua_topointer(L, -1);

	P->setInstallFile(strdup(lua_tostring(L, 2)));

	return 0;
}

static int li_name(lua_State *L)
{
	if(lua_gettop(L) < 0 || lua_gettop(L) > 1)
	{
		throw CustomException("name() takes 1 or no argument(s)");
	}
	if(lua_gettop(L) == 0)
	{
		std::string value = WORLD->getName();
		lua_pushstring(L, value.c_str());
		return 1;
	}
	if(lua_type(L, 1) != LUA_TSTRING) throw CustomException("Argument to name() must be a string");
	const char *value = lua_tostring(L, 1);
	
	WORLD->setName(std::string(value));
	return 0;
}

static int li_feature(lua_State *L)
{
	if(lua_gettop(L) < 1 || lua_gettop(L) > 3)
	{
		throw CustomException("feature() takes 1 to 3 arguments");
	}
	if(lua_gettop(L) == 1)
	{
		lua_getglobal(L, "P");
		Package *P = (Package *)lua_topointer(L, -1);
		
		if(lua_type(L, 1) != LUA_TSTRING) throw CustomException("Argument to feature() must be a string");
		const char *key = lua_tostring(L, 1);
		try {
			std::string value = WORLD->getFeature(std::string(key));
			lua_pushstring(L, value.c_str());
			P->buildDescription()->add(new FeatureValueUnit(key,value.c_str()));
		}
		catch(NoKeyException &E)
		{
			lua_pushnil(L);
			P->buildDescription()->add(new FeatureNilUnit(key));
		}
		return 1;
	}
	if(lua_type(L, 1) != LUA_TSTRING) throw CustomException("First argument to feature() must be a string");
	if(lua_type(L, 2) != LUA_TSTRING) throw CustomException("Second argument to feature() must be a string");
	if(lua_gettop(L) == 3 && lua_type(L, 3) != LUA_TBOOLEAN) throw CustomException("Third argument to feature() must be boolean, if present");
	const char *key = lua_tostring(L, 1);
	const char *value = lua_tostring(L, 2);
	
	if(lua_gettop(L) == 3)
	{
		WORLD->setFeature(std::string(key), std::string(value), lua_toboolean(L, -3));
	}

	WORLD->setFeature(std::string(key), std::string(value));
	return 0;
}

int li_builddir(lua_State *L)
{
	int args = lua_gettop(L);
	if(args > 1)
	{
		throw CustomException("builddir() takes 1 or no arguments");
	}

	lua_getglobal(L, "P");
	Package *P = (Package *)lua_topointer(L, -1);
	
	// create a table, since this is actually an object
	CREATE_TABLE(L,P->builddir());
	P->builddir()->lua_table(L);

	if((args == 1 && lua_toboolean(L, 1)) || 
		(WORLD->areCleaning() && 
		!(WORLD->forcedMode() && !WORLD->isForced(P->getName()))))
	{
		P->builddir()->clean();
	}

	return 1;
}

int li_intercept(lua_State *L)
{
	lua_getglobal(L, "P");
	Package *P = (Package *)lua_topointer(L, -1);
	
	P->setIntercept();
	
	return 0;
}

int li_depend(lua_State *L)
{
	if(lua_gettop(L) != 1)
	{
		throw CustomException("depend() takes exactly 1 argument");
	}
	if(lua_type(L, 1) != LUA_TSTRING) throw CustomException("Argument to depend() must be a string");
	
	// check that the dependency exists
	char *luaFile = NULL;
	if(asprintf(&luaFile, "package/%s/%s.lua", lua_tostring(L, 1), lua_tostring(L, 1)) <= 0)
	{
		throw CustomException("Error with asprintf");
	}
	
	FILE *f = fopen(luaFile, "r");
	if(f == NULL)
	{
		log(lua_tostring(L, 1), "Opening %s: %s", luaFile, strerror(errno));
		throw CustomException("Failed opening Package");
	}
	fclose(f);

	// create the Package
	Package *p = WORLD->findPackage(std::string(lua_tostring(L, 1)),std::string(luaFile));
	if(p == NULL)
	{
		free(luaFile);
		throw CustomException("Failed to create or find Package");
	}

	// record the dependency
	lua_getglobal(L, "P");
	Package *P = (Package *)lua_topointer(L, -1);

	P->depend(p);
	
	return 0;
}

bool buildsys::interfaceSetup(Lua *lua)
{
	lua->registerFunc("builddir", li_builddir);
	lua->registerFunc("depend",   li_depend);
	lua->registerFunc("feature",  li_feature);
	lua->registerFunc("intercept",li_intercept);
	lua->registerFunc("name",     li_name);

	return true;
}
