/******************************************************************************
 Copyright 2013 Allied Telesis Labs Ltd. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright
       notice, this list of conditions and the following disclaimer.

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

static char *absolute_path(BuildDir * d, const char *dir, bool allowDL = false)
{
	char *path = NULL;
	if(dir[0] == '/' || (allowDL && !strncmp(dir, "dl/", 3)))
		asprintf(&path, "%s", dir);
	else
		asprintf(&path, "%s/%s", d->getPath(), dir);
	return path;
}

static char *relative_path(BuildDir * d, const char *dir, bool allowDL = false)
{
	char *path = NULL;
	if(dir[0] == '/' || (allowDL && !strncmp(dir, "dl/", 3)))
		asprintf(&path, "%s", dir);
	else
		asprintf(&path, "%s/%s", d->getShortPath(), dir);
	return path;
}

static void add_env(Package * P, PackageCmd * pc)
{
	char *pn_env = NULL;
	asprintf(&pn_env, "BS_PACKAGE_NAME=%s", P->getName().c_str());
	pc->addEnv(pn_env);
	free(pn_env);
}

int li_bd_fetch(lua_State * L)
{
	/* the first argument is the table, and is implicit */
	int argc = lua_gettop(L);
	if(argc < 1) {
		throw CustomException("fetch() requires at least 2 arguments");
	}
	if(!lua_istable(L, 1))
		throw CustomException("fetch() must be called using : not .");
	if(!lua_isstring(L, 2))
		throw CustomException("fetch() expects a string as the first argument");
	if(!lua_isstring(L, 3))
		throw CustomException("fetch() expects a string as the second argument");
	if(argc > 3) {
		if(!lua_isboolean(L, 4) && !lua_isstring(L, 4))
			throw
			    CustomException
			    ("fetch() expects either a string or boolean as the third argument");
	} else if(argc > 4) {
		if(!lua_isstring(L, 5) || (lua_isstring(L, 4)))
			throw
			    CustomException
			    ("fetch() expects either a string as the third or fourth argument, but not both");
	}

	/* Don't fetch anything when in parse-only mode */
	if(WORLD->areParseOnly()) {
		return 0;
	}

	lua_getglobal(L, "P");
	Package *P = (Package *) lua_topointer(L, -1);

	CHECK_ARGUMENT_TYPE("fetch", 1, BuildDir, d);

	char *location = strdup(lua_tostring(L, 2));
	const char *method = lua_tostring(L, 3);

	if(WORLD->forcedMode() && !WORLD->isForced(P->getName())) {
		return 0;
	}

	FetchUnit *f = NULL;

	if(strcmp(method, "dl") == 0) {
		bool decompress = false;
		char *filename = strdup("");
		if(argc > 3) {
			if(lua_isboolean(L, 4)) {
				decompress = lua_toboolean(L, 4);
			} else {
				filename = strdup(lua_tostring(L, 4));
			}
		} else if(argc > 4) {
			filename = strdup(lua_tostring(L, 5));
		}
		f = new DownloadFetch(std::string(location), decompress,
				      std::string(filename));
		free(filename);
	} else if(strcmp(method, "git") == 0) {
		const char *branch = lua_tostring(L, 4);
		const char *local = lua_tostring(L, 5);
		if(local == NULL) {
			char *l2 = strrchr(location, '/');
			if(l2[1] == '\0') {
				l2[0] = '\0';
				l2 = strrchr(location, '/');
			}
			l2++;
			char *dotgit = strstr(l2, ".git");
			if(dotgit) {
				dotgit[0] = '\0';
			}
			local = l2;
		}
		if(branch == NULL) {
			// Default to master
			branch = "origin/master";
		}
		GitExtractionUnit *geu = new GitExtractionUnit(location, local, branch);
		f = geu;
		P->extraction()->add(geu);
	} else if(strcmp(method, "linkgit") == 0) {
		char *l = P->relative_fetch_path(location);
		char *l2 = strrchr(l, '/');
		if(l2[1] == '\0') {
			l2[0] = '\0';
			l2 = strrchr(l, '/');
		}
		l2++;
		LinkGitDirExtractionUnit *lgdeu =
		    new LinkGitDirExtractionUnit(location, l2);
		P->extraction()->add(lgdeu);
		free(l);
	} else if(strcmp(method, "link") == 0) {
		f = new LinkFetch(std::string(location));
	} else if(strcmp(method, "copyfile") == 0) {
		char *file_path = P->relative_fetch_path(location);
		P->extraction()->add(new FileCopyExtractionUnit(file_path));
		free(file_path);
	} else if(strcmp(method, "copygit") == 0) {
		char *src_path = NULL;
		src_path = P->relative_fetch_path(location);
		CopyGitDirExtractionUnit *cgdeu =
		    new CopyGitDirExtractionUnit(src_path, ".");
		P->extraction()->add(cgdeu);
	} else if(strcmp(method, "sm") == 0) {
		if(mkdir(d->getWorkBuild(), 0777) && errno != EEXIST) {
			// An error occured, try remove the file, then relink
			PackageCmd *rmpc = new PackageCmd(d->getPath(), "rm");
			rmpc->addArg("-fr");
			rmpc->addArg(d->getWorkBuild());
			if(!rmpc->Run(P))
				throw
				    CustomException
				    ("Failed to ln (symbolically), could not remove target first");
			if(mkdir(d->getWorkBuild(), 0777))
				throw
				    CustomException
				    ("Failed to mkdir, even after removing target first");
			delete rmpc;
		}
		char *l = strdup(d->getWorkSrc());
		char *l2 = strrchr(l, '/');
		if(l2[1] == '\0') {
			l2[0] = '\0';
			l2 = strrchr(l, '/');
		}
		l2++;
		GitDirExtractionUnit *lgdeu = new LinkGitDirExtractionUnit(location, l2);
		P->extraction()->add(lgdeu);
		free(l);
	} else if(strcmp(method, "copy") == 0) {
		f = new CopyFetch(std::string(location));
	} else if(strcmp(method, "deps") == 0) {
		char *path = absolute_path(d, location);
		// record this directory (need to complete this operation later)
		lua_getglobal(L, "P");
		Package *P = (Package *) lua_topointer(L, -1);
		P->setDepsExtract(path);
		fprintf(stderr, "Will add installed files, considering code updated\n");
		P->setCodeUpdated();
	} else {
		throw CustomException("Unsupported fetch method");
	}

	if(f) {
		P->fetch()->add(f);
		if(f->force_updated()) {
			P->setCodeUpdated();
		}
	}

	free(location);

	return 0;
}

int li_bd_restore(lua_State * L)
{
	/* the first argument is the table, and is implicit */
	int argc = lua_gettop(L);
	if(argc < 1) {
		throw CustomException("restore() requires at least 2 arguments");
	}
	if(!lua_istable(L, 1))
		throw CustomException("restore() must be called using : not .");
	if(!lua_isstring(L, 2))
		throw CustomException("restore() expects a string as the first argument");
	if(!lua_isstring(L, 3))
		throw CustomException("restore() expects a string as the second argument");

	lua_getglobal(L, "P");
	Package *P = (Package *) lua_topointer(L, -1);

	CHECK_ARGUMENT_TYPE("restore", 1, BuildDir, d);

	char *location = strdup(lua_tostring(L, 2));
	const char *method = lua_tostring(L, 3);

	if(WORLD->forcedMode() && !WORLD->isForced(P->getName())) {
		return 0;
	}

	if(strcmp(method, "copyfile") == 0) {
		PackageCmd *pc = new PackageCmd(d->getPath(), "cp");

		pc->addArg("-pRLuf");

		char const *fn = strrchr(location, '/');
		pc->addArg(fn != NULL ? fn + 1 : location);

		char *tmp = P->absolute_fetch_path(location);
		pc->addArg(tmp);
		free(tmp);

		P->addCommand(pc);
	} else {
		throw CustomException("Unsupported restore method");
	}

	free(location);

	return 0;
}

int li_bd_extract(lua_State * L)
{
	if(lua_gettop(L) != 2)
		throw CustomException("extract() requires exactly 1 argument");
	if(!lua_istable(L, 1))
		throw CustomException("extract() must be called using : not .");
	if(!lua_isstring(L, 2))
		throw CustomException("extract() expects a string as the only argument");

	lua_pushstring(L, "data");
	lua_gettable(L, 1);
	if(!lua_islightuserdata(L, -1))
		throw CustomException("extract() must be called using : not .");
	BuildDir *d = (BuildDir *) lua_topointer(L, -1);
	lua_pop(L, 1);

	lua_getglobal(L, "P");
	Package *P = (Package *) lua_topointer(L, -1);

	if(WORLD->forcedMode() && !WORLD->isForced(P->getName())) {
		return 0;
	}

	const char *fName = lua_tostring(L, 2);
	char *realFName = relative_path(d, fName, true);

	ExtractionUnit *eu = NULL;
	if(strstr(fName, ".zip") != NULL) {
		eu = new ZipExtractionUnit(realFName);
	} else {
		// The catch all for tar compressed files
		eu = new TarExtractionUnit(realFName);
	}
	P->extraction()->add(eu);

	free(realFName);
	return 0;
}

int li_bd_cmd(lua_State * L)
{
	int argc = lua_gettop(L);
	if(argc < 4)
		throw CustomException("cmd() requires at least 3 arguments");
	if(argc > 5)
		throw CustomException("cmd() requires at most 4 arguments");
	if(!lua_istable(L, 1))
		throw CustomException("cmd() must be called using : not .");
	if(!lua_isstring(L, 2))
		throw CustomException("cmd() expects a string as the first argument");
	if(!lua_isstring(L, 3))
		throw CustomException("cmd() expects a string as the second argument");
	if(!lua_istable(L, 4))
		throw
		    CustomException
		    ("cmd() expects a table of strings as the third argument");
	if(argc == 5 && !lua_istable(L, 5))
		throw
		    CustomException
		    ("cmd() expects a table of strings as the fourth argument, if present");

	CHECK_ARGUMENT_TYPE("cmd", 1, BuildDir, d);

	lua_getglobal(L, "P");
	Package *P = (Package *) lua_topointer(L, -1);

	char *dir = relative_path(d, lua_tostring(L, 2));
	const char *app = lua_tostring(L, 3);

	PackageCmd *pc = new PackageCmd(dir, app);

	lua_pushnil(L);		/* first key */
	while(lua_next(L, 4) != 0) {
		/* uses 'key' (at index -2) and 'value' (at index -1) */
		if(lua_type(L, -1) != LUA_TSTRING)
			throw
			    CustomException
			    ("cmd() requires a table of strings as the third argument\n");
		pc->addArg(lua_tostring(L, -1));
		/* removes 'value'; keeps 'key' for next iteration */
		lua_pop(L, 1);
	}

	if(argc == 5) {
		lua_pushnil(L);	/* first key */
		while(lua_next(L, 5) != 0) {
			/* uses 'key' (at index -2) and 'value' (at index -1) */
			if(lua_type(L, -1) != LUA_TSTRING)
				throw
				    CustomException
				    ("cmd() requires a table of strings as the fourth argument\n");
			pc->addEnv(lua_tostring(L, -1));
			/* removes 'value'; keeps 'key' for next iteration */
			lua_pop(L, 1);
		}
	}

	add_env(P, pc);
	P->addCommand(pc);

	free(dir);

	return 0;
}

int li_bd_patch(lua_State * L)
{
	if(lua_gettop(L) != 4)
		throw CustomException("patch() requires exactly 3 arguments");
	if(!lua_istable(L, 1))
		throw CustomException("patch() must be called using : not .");
	if(!lua_isstring(L, 2))
		throw CustomException("patch() expects a string as the first argument");
	if(!lua_isnumber(L, 3))
		throw CustomException("patch() expects a number as the second argument");
	if(!lua_istable(L, 4))
		throw
		    CustomException
		    ("patch() expects a table of strings as the third argument");

	lua_getglobal(L, "P");
	Package *P = (Package *) lua_topointer(L, -1);

	if(WORLD->forcedMode() && !WORLD->isForced(P->getName())) {
		return true;
	}

	CHECK_ARGUMENT_TYPE("patch", 1, BuildDir, d);

	char *patch_path = relative_path(d, lua_tostring(L, 2), true);

	int patch_depth = lua_tonumber(L, 3);


	lua_pushnil(L);		/* first key */
	while(lua_next(L, 4) != 0) {
		/* uses 'key' (at index -2) and 'value' (at index -1) */
		{
			char *uri = NULL;
			if(lua_type(L, -1) != LUA_TSTRING)
				throw
				    CustomException
				    ("patch() requires a table of strings as the third argument\n");
			uri = P->relative_fetch_path(lua_tostring(L, -1));
			PatchExtractionUnit *peu =
			    new PatchExtractionUnit(patch_depth, patch_path, uri);
			P->extraction()->add(peu);
			free(uri);
		}
		/* removes 'value'; keeps 'key' for next iteration */
		lua_pop(L, 1);
	}

	free(patch_path);

	return 0;
}

int li_bd_installfile(lua_State * L)
{
	if(lua_gettop(L) != 2)
		throw CustomException("installfile() requires exactly 1 argument");
	if(!lua_istable(L, 1))
		throw CustomException("installfile() must be called using : not .");
	if(!lua_isstring(L, 2))
		throw
		    CustomException("installfile() expects a string as the only argument");

	lua_getglobal(L, "P");
	Package *P = (Package *) lua_topointer(L, -1);

	P->setInstallFile(strdup(lua_tostring(L, 2)));

	return 0;
}
