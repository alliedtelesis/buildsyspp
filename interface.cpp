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
			if(run((char *)"wget", argv , "dl", NULL) != 0)
				throw CustomException("Failed to fetch file");
			if(decompress)
			{
				// We want to run a command on this file
				char *cmd = NULL;
				char *ext = strrchr(fname, '.');
				if(ext == NULL)
				{
					fprintf(stderr, "Could not guess decompression based on extension: %s\n", fname);
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
		if(run((char *)"git", argv , d->getPath(), NULL) != 0)
			throw CustomException("Failed to git clone");
	} else if(strcmp(method, "link") == 0) {
		argv = (char **)calloc(5, sizeof(char *));
		argv[0] = strdup("ln");
		argv[1] = strdup("-s");
		if(location[0] == '.')
		{
			char *pwd = getcwd(NULL, 0);
			asprintf(&argv[2], "%s/%s", pwd, location);
			free(pwd);
		} else {
			argv[2] = strdup(location);
		}
		argv[3] = strdup(".");
		if(run((char *)"ln", argv , d->getPath(), NULL) != 0)
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
			fprintf(stderr, "%s %s %s\n", rmargv[0], rmargv[1], rmargv[2]);
			if(run((char *)"/bin/rm", rmargv , d->getPath(), NULL) != 0)
				throw CustomException("Failed to ln (symbolically), could not remove target first");
			if(run((char *)"ln", argv, d->getPath(), NULL) != 0)
				throw CustomException("Failed to ln (symbolically), even after removing target first");
			free(rmargv[0]);
			free(rmargv[1]);
			free(rmargv[2]);
			free(rmargv);
			free(l);
		}
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
		if(run((char *)"cp", argv , d->getPath(), NULL) != 0)
			throw CustomException("Failed to copy (recursively)");
	} else if(strcmp(method, "deps") == 0) {
		char *path = NULL;
		asprintf(&path, "%s/%s", d->getPath(), location);
		// record this directory (need to complete this operation later)
		lua_getglobal(L, "P");
		Package *P = (Package *)lua_topointer(L, -1);
		P->setDepsExtract(path);
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
	
	const char *fName = lua_tostring(L, 2);

	char **argv = NULL;
	
	int res = mkdir("dl", 0700);
	if((res < 0) && (errno != EEXIST))
	{
		throw CustomException("Error: Creating download directory");
	}
	argv = (char **)calloc(4, sizeof(char *));
	argv[0] = strdup("tar");
	argv[1] = strdup("xf");
	if(!strncmp(fName, "dl/", 3))
	{
		char *pwd = getcwd(NULL, 0);
		asprintf(&argv[2], "%s/%s", pwd, fName);
		free(pwd);
	} else {
		argv[2] = strdup(fName);
	}
	
	if(run((char *)"tar", argv , d->getPath(), NULL) != 0)
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
	asprintf(&path, "%s/%s", d->getPath(), lua_tostring(L, 2));
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

int li_bd_patch(lua_State *L)
{
	if(lua_gettop(L) != 4) throw CustomException("patch() requires exactly 3 arguments");
	if(!lua_istable(L, 1)) throw CustomException("patch() must be called using : not .");
	if(!lua_isstring(L, 2)) throw CustomException("patch() expects a string as the first argument");
	if(!lua_isnumber(L, 3)) throw CustomException("patch() expects a number as the second argument");
	if(!lua_istable(L, 4)) throw CustomException("patch() expects a table of strings as the third argument");
	
	lua_getglobal(L, "P");
	Package *P = (Package *)lua_topointer(L, -1);
	
	CHECK_ARGUMENT_TYPE("fetch",1,BuildDir,d);

	char *patch_path = NULL;
	asprintf(&patch_path, "%s/%s", d->getPath(), lua_tostring(L, 2));

	int patch_depth = lua_tonumber(L, 3);


	char **argv = (char **)calloc(7, sizeof(char *));
	argv[0] = strdup("patch");
	asprintf(&argv[1], "-p%i", patch_depth);
	argv[2] = strdup("-stN");
	argv[3] = strdup("-i");
	
	char *pwd = getcwd(NULL, 0);
		
	lua_pushnil(L);  /* first key */
	while (lua_next(L, 4) != 0) {
		/* uses 'key' (at index -2) and 'value' (at index -1) */
		{
			if(lua_type(L, -1) != LUA_TSTRING) throw CustomException("patch() requires a table of strings as the third argument\n");
			if(!strncmp(lua_tostring(L, -1), "dl/", 3))
			{
				asprintf(&argv[4], "%s/%s", pwd,  lua_tostring(L, -1));
			} else {
				asprintf(&argv[4], "%s/package/%s/%s", pwd, P->getName().c_str(), lua_tostring(L, -1));
			}
			argv[5] = strdup("--dry-run");
			if(run((char *)"patch", argv , patch_path, NULL) != 0)
			{
				fprintf(stderr, "Patch file: %s\n", argv[4]);
				throw CustomException("Will fail to patch");
			}
			free(argv[5]);
			argv[5] = NULL;
			if(run((char *)"patch", argv , patch_path, NULL) != 0)
				throw CustomException("Truely failed to patch");
			free(argv[4]);
			argv[4] = NULL;
		}
		/* removes 'value'; keeps 'key' for next iteration */
		lua_pop(L, 1);
	}
	
	free(pwd);
	
	if(argv != NULL) {
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

int li_bd_install(lua_State *L)
{
	if(lua_gettop(L) != 2) throw CustomException("install() requires exactly 1 argument");
	if(!lua_istable(L, 1)) throw CustomException("install() must be called using : not .");
	if(!lua_isstring(L, 2)) throw CustomException("install() expects a string as the only argument");

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
		if(lua_type(L, 1) != LUA_TSTRING) throw CustomException("Argument to feature() must be a string");
		const char *key = lua_tostring(L, 1);
		try {
			std::string value = WORLD->getFeature(std::string(key));
			lua_pushstring(L, value.c_str());
		}
		catch(NoKeyException &E)
		{
			lua_pushnil(L);
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

	if(args == 1 && lua_toboolean(L, 1))
	{
		// clean out the build directory
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
		fprintf(stderr, "Opening %s: %s\n", luaFile, strerror(errno));
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

void buildsys::interfaceSetup(Lua *lua)
{
	lua->registerFunc("name",     li_name);
	lua->registerFunc("feature",  li_feature);
	lua->registerFunc("depend",   li_depend);
	lua->registerFunc("builddir", li_builddir);
	lua->registerFunc("intercept",li_intercept);
}
