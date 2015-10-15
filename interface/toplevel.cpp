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
#include <sys/stat.h>

static int li_name(lua_State * L)
{
	if(lua_gettop(L) < 0 || lua_gettop(L) > 1) {
		throw CustomException("name() takes 1 or no argument(s)");
	}
	int args = lua_gettop(L);

	lua_getglobal(L, "P");
	Package *P = (Package *) lua_topointer(L, -1);

	if(args == 0) {
		std::string value = P->getNS()->getName();
		lua_pushstring(L, value.c_str());
		return 1;
	}
	if(lua_type(L, 1) != LUA_TSTRING)
		throw CustomException("Argument to name() must be a string");
	const char *value = lua_tostring(L, 1);

	P->setNS(WORLD->findNameSpace(value));

	return 0;
}

static int li_feature(lua_State * L)
{
	if(lua_gettop(L) < 1 || lua_gettop(L) > 3) {
		throw CustomException("feature() takes 1 to 3 arguments");
	}
	if(lua_gettop(L) == 1) {
		lua_getglobal(L, "P");
		Package *P = (Package *) lua_topointer(L, -1);

		if(lua_type(L, 1) != LUA_TSTRING)
			throw CustomException("Argument to feature() must be a string");
		const char *key = lua_tostring(L, 1);
		try {
			std::string value = WORLD->getFeature(std::string(key));
			lua_pushstring(L, value.c_str());
			P->buildDescription()->add(new
						   FeatureValueUnit(key, value.c_str()));
		}
		catch(NoKeyException & E) {
			lua_pushnil(L);
			P->buildDescription()->add(new FeatureNilUnit(key));
		}
		return 1;
	}
	if(lua_type(L, 1) != LUA_TSTRING)
		throw CustomException("First argument to feature() must be a string");
	if(lua_type(L, 2) != LUA_TSTRING)
		throw CustomException("Second argument to feature() must be a string");
	if(lua_gettop(L) == 3 && lua_type(L, 3) != LUA_TBOOLEAN)
		throw
		    CustomException
		    ("Third argument to feature() must be boolean, if present");
	const char *key = lua_tostring(L, 1);
	const char *value = lua_tostring(L, 2);

	if(lua_gettop(L) == 3) {
		WORLD->setFeature(std::string(key), std::string(value),
				  lua_toboolean(L, -3));
	}

	WORLD->setFeature(std::string(key), std::string(value));
	return 0;
}


int li_builddir(lua_State * L)
{
	int args = lua_gettop(L);
	if(args > 1) {
		throw CustomException("builddir() takes 1 or no arguments");
	}

	lua_getglobal(L, "P");
	Package *P = (Package *) lua_topointer(L, -1);

	// create a table, since this is actually an object
	CREATE_TABLE(L, P->builddir());
	P->builddir()->lua_table(L);

	if((args == 1 && lua_toboolean(L, 1)) ||
	   (WORLD->areCleaning() &&
	    !(WORLD->forcedMode() && !WORLD->isForced(P->getName())))) {
		P->builddir()->clean();
	}

	return 1;
}

int li_intercept(lua_State * L)
{
	lua_getglobal(L, "P");
	Package *P = (Package *) lua_topointer(L, -1);

	P->setIntercept();

	return 0;
}

static void depend(Package * P, NameSpace * ns, const char *name)
{
	Package *p = NULL;
	// create the Package
	if(ns) {
		p = ns->findPackage(std::string(name));
	} else {
		p = P->getNS()->findPackage(std::string(name));
	}
	if(p == NULL) {
		throw CustomException("Failed to create or find Package");
	}

	P->depend(p);
}

int li_depend(lua_State * L)
{
	if(!(lua_gettop(L) == 1 || lua_gettop(L) == 2)) {
		throw CustomException("depend() takes 1 argument or 2 arguments");
	}

	NameSpace *ns = NULL;
	if(lua_gettop(L) == 2) {
		if(lua_type(L, 2) != LUA_TSTRING) {
			throw
			    CustomException
			    ("depend() takes a string as the second argument");
		}
		ns = WORLD->findNameSpace(std::string(lua_tostring(L, 2)));
	}
	// get the current package
	lua_getglobal(L, "P");
	Package *P = (Package *) lua_topointer(L, -1);
	if(lua_type(L, 1) == LUA_TSTRING) {
		depend(P, ns, lua_tostring(L, 1));
	} else if(lua_istable(L, 1)) {
		lua_pushnil(L);	/* first key */
		while(lua_next(L, 1) != 0) {
			/* uses 'key' (at index -2) and 'value' (at index -1) */
			if(lua_type(L, -1) != LUA_TSTRING)
				throw
				    CustomException
				    ("depend() requires a table of strings as the only argument\n");
			depend(P, ns, lua_tostring(L, -1));
			/* removes 'value'; keeps 'key' for next iteration */
			lua_pop(L, 1);
		}
	} else {
		throw CustomException("depend() takes a string or a table of strings");
	}

	return 0;
}

int li_buildlocally(lua_State * L)
{
	if(lua_gettop(L) != 0) {
		throw CustomException("buildlocally() takes no arguments");
	}

	lua_getglobal(L, "P");
	Package *P = (Package *) lua_topointer(L, -1);

	// Do not try and download the final result for this package
	// probably because it breaks something else that builds later
	P->disableFetchFrom();

	return 0;
}


int li_hashoutput(lua_State * L)
{
	if(lua_gettop(L) != 0) {
		throw CustomException("buildlocally() takes no arguments");
	}

	lua_getglobal(L, "P");
	Package *P = (Package *) lua_topointer(L, -1);

	// Instead of depender using build.info hash
	// create an output.info and get them to hash that
	// useful for kernel-headers and other packages
	// that produce data that changes less often
	// than the sources
	P->setHashOutput();

	return 0;
}

int li_require(lua_State * L)
{
	bool success = false;

	if(lua_gettop(L) != 1) {
		throw CustomException("require() takes 1 argument");
	}

	if(!lua_isstring(L, 1)) {
		throw CustomException("Argument to require() must be a string");
	}

	lua_getglobal(L, "P");
	Package *P = (Package *) lua_topointer(L, -1);

	char *fName = NULL;
	asprintf(&fName, "%s.lua", lua_tostring(L, 1));
	char *relativeFName = P->relative_fetch_path(fName);

	// Check whether the relative file path exists. If it does
	// not exist then we use the original file path
	struct stat buf;
	if(stat(relativeFName, &buf) == 0) {
		P->getLua()->processFile(relativeFName);
		P->buildDescription()->add(new RequireFileUnit(relativeFName));
		success = true;
	} else {
		// Look through all the overlays for this file
		string_list::iterator iter = WORLD->overlaysStart();
		string_list::iterator iterEnd = WORLD->overlaysEnd();
		for(; iter != iterEnd; iter++) {
			char *overlayFName = NULL;
			if(asprintf(&overlayFName, "%s/%s", iter->c_str(), fName) <= 0) {
				throw CustomException("Error with asprintf");
			}
			if(stat(overlayFName, &buf) == 0) {
				P->getLua()->processFile(overlayFName);
				P->buildDescription()->add(new
							   RequireFileUnit(overlayFName));
				success = true;
			}
			free(overlayFName);
			if(success) {
				break;
			}
		}
	}

	if(!success) {
		throw FileNotFoundException("require", fName);
	}

	free(fName);
	free(relativeFName);
	return 0;
}

bool buildsys::interfaceSetup(Lua * lua)
{
	lua->registerFunc("builddir", li_builddir);
	lua->registerFunc("depend", li_depend);
	lua->registerFunc("feature", li_feature);
	lua->registerFunc("intercept", li_intercept);
	lua->registerFunc("name", li_name);
	lua->registerFunc("buildlocally", li_buildlocally);
	lua->registerFunc("hashoutput", li_hashoutput);
	lua->registerFunc("require", li_require);

	return true;
}
