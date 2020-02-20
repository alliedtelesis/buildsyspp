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

#include "include/buildsys.h"
#include "interface/luainterface.h"

static std::string absolute_path(BuildDir *d, const std::string &dir, bool allowDL = false)
{
	std::string path;
	if(boost::algorithm::starts_with(dir, "/") ||
	   (allowDL && boost::algorithm::starts_with(dir, "dl/"))) {
		path = dir;
	} else {
		path = d->getPath() + "/" + dir;
	}
	return path;
}

static std::string relative_path(const BuildDir *d, const std::string &dir,
                                 bool allowDL = false)
{
	std::string path;
	if(boost::algorithm::starts_with(dir, "/") ||
	   (allowDL && boost::algorithm::starts_with(dir, "dl/"))) {
		path = dir;
	} else {
		path = d->getShortPath() + "/" + dir;
	}
	return path;
}

static void add_env(Package *P, PackageCmd *pc)
{
	std::string pn_env = string_format("BS_PACKAGE_NAME=%s", P->getName().c_str());
	pc->addEnv(pn_env);
}

int li_bd_fetch(lua_State *L)
{
	/* the first argument is the table, and is implicit */
	int argc = lua_gettop(L);
	if(argc < 1) {
		throw CustomException("fetch() requires 1 argument");
	}
	if(!lua_istable(L, 1)) {
		throw CustomException("fetch() must be called using : not .");
	}
	if(!lua_istable(L, 2)) {
		throw CustomException("fetch() expects a table as the first argument");
	}

	std::string uri;
	std::string to;
	std::string method;
	std::string filename;
	bool decompress = false;
	std::string branch;
	std::string reponame;
	bool listedonly = false;
	std::string copyto;

	Package *P = li_get_package();

	CHECK_ARGUMENT_TYPE("fetch_table", 1, BuildDir, d);

	/* Get the parameters from the table */
	lua_pushvalue(L, 2);
	lua_pushnil(L);

	while(lua_next(L, -2) != 0) {
		lua_pushvalue(L, -2);
		if((lua_isstring(L, -1) != 0) && (lua_isstring(L, -2) != 0)) {
			std::string key(lua_tostring(L, -1));
			std::string value(lua_tostring(L, -2));
			if(key == "uri") {
				uri = value;
			} else if(key == "method") {
				method = value;
			} else if(key == "filename") {
				filename = value;
			} else if(key == "decompress") {
				decompress = (value == "true");
			} else if(key == "branch") {
				branch = value;
			} else if(key == "reponame") {
				reponame = value;
			} else if(key == "to") {
				to = value;
			} else if(key == "listedonly") {
				listedonly = (value == "true");
			} else if(key == "copyto") {
				copyto = value;
			} else {
				log(P, "Unknown key %s (%s)", key.c_str(), value.c_str());
			}
		}
		lua_pop(L, 2);
	}
	lua_pop(L, 1);

	/* Create fetch object here */
	FetchUnit *f = nullptr;
	if(method == "dl") {
		if(uri.empty()) {
			throw CustomException("fetch method = dl requires uri to be set");
		}

		f = new DownloadFetch(uri, decompress, filename, P);
		if(!copyto.empty()) {
			P->extraction()->add(new FetchedFileCopyExtractionUnit(f, copyto));
		}
	} else if(method == "git") {
		if(uri.empty()) {
			throw CustomException("fetch method = git requires uri to be set");
		}

		if(reponame.empty()) {
			auto last_slash = uri.rfind('/');
			if(last_slash == std::string::npos) {
				throw CustomException("fetch method = git failure parsing uri");
			}

			if(last_slash == uri.length() - 1) {
				auto second_last_slash = uri.rfind('/', last_slash - 1);
				reponame = uri.substr(second_last_slash + 1, last_slash);
			} else {
				reponame = uri.substr(last_slash + 1, std::string::npos);
			}

			if(boost::algorithm::ends_with(reponame, ".git")) {
				reponame.resize(reponame.length() - 4);
			}
		}
		if(branch.empty()) {
			// Default to master
			branch = std::string("origin/master");
		}
		GitExtractionUnit *geu = new GitExtractionUnit(uri, reponame, branch, P);
		P->extraction()->add(geu);
	} else if(method == "linkgit") {
		if(uri.empty()) {
			throw CustomException("fetch method = linkgit requires uri to be set");
		}
		std::string l = P->relative_fetch_path(uri);
		char *l_copy = strdup(l.c_str());
		char *l2 = strrchr(l_copy, '/');
		if(l2[1] == '\0') {
			l2[0] = '\0';
			l2 = strrchr(l_copy, '/');
		}
		l2++;
		LinkGitDirExtractionUnit *lgdeu = new LinkGitDirExtractionUnit(uri, l2);
		P->extraction()->add(lgdeu);
		free(l_copy);
	} else if(method == "link") {
		if(uri.empty()) {
			throw CustomException("fetch method = link requires uri to be set");
		}
		f = new LinkFetch(uri, P);
	} else if(method == "copyfile") {
		if(uri.empty()) {
			throw CustomException("fetch method = copyfile requires uri to be set");
		}
		std::string file_path = P->relative_fetch_path(uri);
		P->extraction()->add(new FileCopyExtractionUnit(file_path, uri));
	} else if(method == "copygit") {
		if(uri.empty()) {
			throw CustomException("fetch method = copygit requires uri to be set");
		}
		std::string src_path = P->relative_fetch_path(uri);
		CopyGitDirExtractionUnit *cgdeu = new CopyGitDirExtractionUnit(src_path, ".");
		P->extraction()->add(cgdeu);
	} else if(method == "copy") {
		if(uri.empty()) {
			throw CustomException("fetch method = copy requires uri to be set");
		}
		f = new CopyFetch(uri, P);
	} else if(method == "deps") {
		std::string path = absolute_path(d, to);
		// record this directory (need to complete this operation later)
		P->setDepsExtract(path, listedonly);
		log(P, "Will add installed files, considering code updated");
		P->setCodeUpdated();
	} else {
		throw CustomException("Unsupported fetch method");
	}

	/* Add the fetch unit to the package */
	if(f != nullptr) {
		P->fetch()->add(f);
		if(f->force_updated()) {
			P->setCodeUpdated();
		}

		/* Return the fetch object here */
		CREATE_TABLE(L, f);
		f->lua_table(L);
	}

	return 1;
}

int li_bd_restore(lua_State *L)
{
	/* the first argument is the table, and is implicit */
	int argc = lua_gettop(L);
	if(argc < 1) {
		throw CustomException("restore() requires at least 2 arguments");
	}
	if(!lua_istable(L, 1)) {
		throw CustomException("restore() must be called using : not .");
	}
	if(lua_isstring(L, 2) == 0) {
		throw CustomException("restore() expects a string as the first argument");
	}
	if(lua_isstring(L, 3) == 0) {
		throw CustomException("restore() expects a string as the second argument");
	}

	Package *P = li_get_package();

	CHECK_ARGUMENT_TYPE("restore", 1, BuildDir, d);

	std::string location(lua_tostring(L, 2));
	std::string method(lua_tostring(L, 3));

	if(P->getWorld()->forcedMode() && !P->getWorld()->isForced(P->getName())) {
		return 0;
	}

	if(method == "copyfile") {
		PackageCmd pc(d->getPath(), "cp");

		pc.addArg("-pRLuf");

		auto position = location.rfind('/');
		if(position != std::string::npos) {
			pc.addArg(location.substr(position + 1));
		} else {
			pc.addArg(location);
		}

		pc.addArg(P->absolute_fetch_path(location));

		P->addCommand(std::move(pc));
	} else {
		throw CustomException("Unsupported restore method");
	}

	return 0;
}

int li_bd_extract_table(lua_State *L)
{
	if(lua_gettop(L) != 2) {
		throw CustomException("extract() requires exactly 1 argument");
	}
	if(!lua_istable(L, 1)) {
		throw CustomException("extract() must be called using : not .");
	}

	Package *P = li_get_package();

	CHECK_ARGUMENT_TYPE("extract", 2, FetchUnit, f);

	ExtractionUnit *eu = nullptr;
	if(strstr(f->relative_path().c_str(), ".zip") != nullptr) {
		eu = new ZipExtractionUnit(f);
	} else {
		// The catch all for tar compressed files
		eu = new TarExtractionUnit(f);
	}
	P->extraction()->add(eu);

	return 0;
}

int li_bd_extract(lua_State *L)
{
	if(lua_gettop(L) != 2) {
		throw CustomException("extract() requires exactly 1 argument");
	}
	if(!lua_istable(L, 1)) {
		throw CustomException("extract() must be called using : not .");
	}
	if(lua_istable(L, 2)) {
		return li_bd_extract_table(L);
	}
	if(lua_isstring(L, 2) == 0) {
		throw CustomException("extract() expects a string as the only argument");
	}

	lua_pushstring(L, "data");
	lua_gettable(L, 1);
	if(!lua_islightuserdata(L, -1)) {
		throw CustomException("extract() must be called using : not .");
	}
	const auto *d = reinterpret_cast<const BuildDir *>(lua_topointer(L, -1));
	lua_pop(L, 1);

	Package *P = li_get_package();

	log(P, "Using deprecated extract API");

	if(P->getWorld()->forcedMode() && !P->getWorld()->isForced(P->getName())) {
		return 0;
	}

	std::string fName(lua_tostring(L, 2));
	std::string realFName = relative_path(d, fName, true);

	ExtractionUnit *eu = nullptr;
	if(fName.find(".zip") != std::string::npos) {
		eu = new ZipExtractionUnit(realFName);
	} else {
		// The catch all for tar compressed files
		eu = new TarExtractionUnit(realFName);
	}
	P->extraction()->add(eu);

	return 0;
}

int li_bd_cmd(lua_State *L)
{
	int argc = lua_gettop(L);
	if(argc < 4) {
		throw CustomException("cmd() requires at least 3 arguments");
	}
	if(argc > 5) {
		throw CustomException("cmd() requires at most 4 arguments");
	}
	if(!lua_istable(L, 1)) {
		throw CustomException("cmd() must be called using : not .");
	}
	if(lua_isstring(L, 2) == 0) {
		throw CustomException("cmd() expects a string as the first argument");
	}
	if(lua_isstring(L, 3) == 0) {
		throw CustomException("cmd() expects a string as the second argument");
	}
	if(!lua_istable(L, 4)) {
		throw CustomException("cmd() expects a table of strings as the third argument");
	}
	if(argc == 5 && !lua_istable(L, 5)) {
		throw CustomException(
		    "cmd() expects a table of strings as the fourth argument, if present");
	}

	CHECK_ARGUMENT_TYPE("cmd", 1, BuildDir, d);

	Package *P = li_get_package();

	std::string dir = relative_path(d, lua_tostring(L, 2));
	std::string app(lua_tostring(L, 3));

	PackageCmd pc(dir, app);

	lua_pushnil(L); /* first key */
	while(lua_next(L, 4) != 0) {
		/* uses 'key' (at index -2) and 'value' (at index -1) */
		if(lua_type(L, -1) != LUA_TSTRING) {
			throw CustomException(
			    "cmd() requires a table of strings as the third argument\n");
		}
		pc.addArg(lua_tostring(L, -1));
		/* removes 'value'; keeps 'key' for next iteration */
		lua_pop(L, 1);
	}

	if(argc == 5) {
		lua_pushnil(L); /* first key */
		while(lua_next(L, 5) != 0) {
			/* uses 'key' (at index -2) and 'value' (at index -1) */
			if(lua_type(L, -1) != LUA_TSTRING) {
				throw CustomException(
				    "cmd() requires a table of strings as the fourth argument\n");
			}
			pc.addEnv(std::string(lua_tostring(L, -1)));
			/* removes 'value'; keeps 'key' for next iteration */
			lua_pop(L, 1);
		}
	}

	add_env(P, &pc);
	P->addCommand(std::move(pc));

	return 0;
}

int li_bd_patch(lua_State *L)
{
	if(lua_gettop(L) != 4) {
		throw CustomException("patch() requires exactly 3 arguments");
	}
	if(!lua_istable(L, 1)) {
		throw CustomException("patch() must be called using : not .");
	}
	if(lua_isstring(L, 2) == 0) {
		throw CustomException("patch() expects a string as the first argument");
	}
	if(lua_isnumber(L, 3) == 0) {
		throw CustomException("patch() expects a number as the second argument");
	}
	if(!lua_istable(L, 4)) {
		throw CustomException("patch() expects a table of strings as the third argument");
	}

	Package *P = li_get_package();

	if(P->getWorld()->forcedMode() && !P->getWorld()->isForced(P->getName())) {
		return 1;
	}

	CHECK_ARGUMENT_TYPE("patch", 1, BuildDir, d);

	std::string patch_path = relative_path(d, lua_tostring(L, 2), true);

	auto patch_depth = static_cast<int>(lua_tonumber(L, 3));

	lua_pushnil(L); /* first key */
	while(lua_next(L, 4) != 0) {
		/* uses 'key' (at index -2) and 'value' (at index -1) */
		{
			if(lua_type(L, -1) != LUA_TSTRING) {
				throw CustomException(
				    "patch() requires a table of strings as the third argument\n");
			}
			std::string uri = P->relative_fetch_path(lua_tostring(L, -1));
			PatchExtractionUnit *peu =
			    new PatchExtractionUnit(patch_depth, patch_path, uri, lua_tostring(L, -1));
			P->extraction()->add(peu);
		}
		/* removes 'value'; keeps 'key' for next iteration */
		lua_pop(L, 1);
	}

	return 0;
}

int li_bd_installfile(lua_State *L)
{
	if(lua_gettop(L) != 2) {
		throw CustomException("installfile() requires exactly 1 argument");
	}
	if(!lua_istable(L, 1)) {
		throw CustomException("installfile() must be called using : not .");
	}
	if(lua_isstring(L, 2) == 0) {
		throw CustomException("installfile() expects a string as the only argument");
	}

	Package *P = li_get_package();
	P->setInstallFile(std::string(lua_tostring(L, 2)));

	return 0;
}
