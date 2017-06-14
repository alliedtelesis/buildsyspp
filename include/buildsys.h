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

#include <iostream>
#include <map>
#include <list>
#include <memory>

extern "C" {
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
};

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <boost/config.hpp>
#include <boost/utility.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/visitors.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_utility.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/topological_sort.hpp>

using namespace boost;

typedef boost::adjacency_list < boost::vecS, boost::vecS, boost::directedS > Graph;
typedef boost::graph_traits < Graph >::vertex_descriptor Vertex;
typedef boost::graph_traits < Graph >::edge_descriptor Edge;

#define error(M) \
	do {\
		log("BuildSys","%s:%s():%i: %s", __FILE__, __FUNCTION__ , __LINE__, M); \
	} while(0)

#define LUA_SET_TABLE_TYPE(L,T) \
	lua_pushstring(L, #T); \
	lua_pushstring(L, "yes"); \
	lua_settable(L, -3);

#define LUA_ADD_TABLE_FUNC(L,N,F) \
	lua_pushstring(L, N); \
	lua_pushcfunction(L, F); \
	lua_settable(L, -3);

#define CREATE_TABLE(L,D) \
	lua_newtable(L); \
	lua_pushstring(L, "data"); \
	lua_pushlightuserdata(L, D); \
	lua_settable(L, -3);

#define SET_TABLE_TYPE(L,T) \
	lua_pushstring(L, #T); \
	lua_pushstring(L, "yes"); \
	lua_settable(L, -3);

#define LUA_ADD_TABLE_TABLE(L,N,D) \
	lua_pushstring(L, N); \
	CREATE_TABLE(D) \
	D->lua_table(L); \
	lua_settable(L, -3);

#define LUA_ADD_TABLE_STRING(L,N,S) \
	lua_pushstring(L, N); \
	lua_pushstring(L, S); \
	lua_settable(L, -3);

extern "C" {
	int li_bd_cmd(lua_State * L);
	int li_bd_extract(lua_State * L);
	int li_bd_fetch(lua_State * L);
	int li_bd_installfile(lua_State * L);
	int li_bd_patch(lua_State * L);
	int li_bd_restore(lua_State * L);
};

namespace buildsys {
	typedef std::map < std::string, std::string > key_value;
	typedef std::list < std::string > string_list;

	class Package;

	/*! The catchable exception */
	class Exception {
	public:
		//! Return the error message for this exception
		virtual std::string error_msg() = 0;
	};

	/*! An exception with a custom error message */
	class CustomException:public Exception {
	private:
		std::string errmsg;
	public:
		/** Construct an exception with a specific error message
		 *  \param err The erorr message
		 */
		CustomException(std::string err):errmsg(err) {
		};
		virtual std::string error_msg() {
			return errmsg;
		}
	};

	/*! A Lua fault */
	class LuaException:public Exception {
	public:
		virtual std::string error_msg() {
			return "Lua Error";
		}
	};

	/*! A Memory fault */
	class MemoryException:public Exception {
	public:
		virtual std::string error_msg() {
			return "Memory Error";
		}
	};

	/*! No Such Key fault */
	class NoKeyException:public Exception {
	public:
		virtual std::string error_msg() {
			return "Key does not exist";
		}
	};

	/*! Error using/creating directory */
	class DirException:public Exception {
	private:
		std::string errmsg;
	public:
		/** Create a directory exception
		 *  \param location The directory path
		 *  \param err The error message
		 */
		DirException(const char *location, const char *err) {
			char *em = NULL;
			asprintf(&em, "Error with directory '%s': %s", location, err);
			errmsg = std::string(em);
			free(em);
		} virtual std::string error_msg() {
			return errmsg;
		}
	};

	/*! File not found */
	class FileNotFoundException:public Exception {
	private:
		std::string errmsg;
	public:
		/** Create a file not found exception
		 *  \param file the file that was not found
		 *  \param where The location where the error occurred
		 */
		FileNotFoundException(const char *file, const char *where) {
			char *em = NULL;
			asprintf(&em, "%s: File not found '%s'", where, file);
			errmsg = std::string(em);
			free(em);
		} virtual std::string error_msg() {
			return errmsg;
		}
	};

	/*! A C++ wrapper for a lua state (instance) */
	class Lua {
	private:
		lua_State * state;
	public:
		/** Create a lua state
		 * Creates a new lua state, and opens loads our libraries into it
		 */
		Lua() {
			state = luaL_newstate();
			if(state == NULL)
				throw LuaException();
			luaL_openlibs(state);
		};

		~Lua() {
			lua_close(state);
			this->state = NULL;
		}
		/** Load and exexcute a lua file in this instance
		 *  \param filename The name of the lua file to load and run
		 */
		void processFile(const char *filename) {

			int res = luaL_dofile(state, filename);

			if(res != 0) {
				throw CustomException(lua_tostring(state, -1));
			}
		}
		/** Register a function in this lua instance
		 *  \param name The name of the function
		 *  \param fn The function to call
		 */
		void registerFunc(std::string name, lua_CFunction fn) {
			lua_register(state, name.c_str(), fn);
		}
		/** Set a global variable to given data
		 *  \param name the name to store this data as
		 *  \param data the data
		 */
		void setGlobal(std::string name, void *data) {
			lua_pushlightuserdata(state, data);
			lua_setglobal(state, name.c_str());
		}
	};

	//! A directory to perform actions on
	class Dir {
	private:
	public:
		//! Create a directory
		Dir() {
		};
		virtual ~ Dir() {
		};
		//! Set this as a Dir for lua
		static void lua_table_r(lua_State * L) {
		LUA_SET_TABLE_TYPE(L, Dir)}
		//! Register as a lua table for return from a function
		virtual void lua_table(lua_State * L) {
			lua_table_r(L);
		};
	};

	/** A directory for building a package in
	 *  Each package has one build directory which is used to run all the commands in
	 */
	class BuildDir:public Dir {
	private:
		typedef Dir super;
		std::string path;
		std::string rpath;
		std::string staging;
		std::string new_path;
		std::string new_staging;
		std::string new_install;
		std::string work_build;
		std::string work_src;
	public:
		/** Create a build directory
		 *  \param P The package this directory is for
		 */
		BuildDir(Package * P);
		~BuildDir() {
		};
		//! Return the full path to this directory
		const char *getPath() {
			return this->path.c_str();
		};
		//! Get the short version (relative only) of the working directory
		const char *getShortPath() {
			return this->rpath.c_str();
		};
		//! Return the full path to the staging directory
		const char *getStaging() {
			return this->staging.c_str();
		};
		//! Return the full path to the new directory
		const char *getNewPath() {
			return this->new_path.c_str();
		};
		//! Return the full path to the new staging directory
		const char *getNewStaging() {
			return this->new_staging.c_str();
		};
		//! Return the full path to the new install directory
		const char *getNewInstall() {
			return this->new_install.c_str();
		};
		const char *getWorkBuild() {
			return this->work_build.c_str();
		};
		const char *getWorkSrc() {
			return this->work_src.c_str();
		};

		//! Remove all the contents of this directory
		void clean();

		static void lua_table_r(lua_State * L) {
			LUA_SET_TABLE_TYPE(L, BuildDir);
			LUA_ADD_TABLE_FUNC(L, "cmd", li_bd_cmd);
			LUA_ADD_TABLE_FUNC(L, "extract", li_bd_extract);
			LUA_ADD_TABLE_FUNC(L, "fetch", li_bd_fetch);
			LUA_ADD_TABLE_FUNC(L, "installfile", li_bd_installfile);
			LUA_ADD_TABLE_FUNC(L, "patch", li_bd_patch);
			LUA_ADD_TABLE_FUNC(L, "restore", li_bd_restore);
			super::lua_table_r(L);
		}
		virtual void lua_table(lua_State * L) {
			lua_table_r(L);
			LUA_ADD_TABLE_STRING(L, "new_staging", new_staging.c_str());
			LUA_ADD_TABLE_STRING(L, "new_install", new_install.c_str());
			LUA_ADD_TABLE_STRING(L, "path", path.c_str());
			LUA_ADD_TABLE_STRING(L, "staging", staging.c_str());
			LUA_ADD_TABLE_STRING(L, "work_build", work_build.c_str());
			LUA_ADD_TABLE_STRING(L, "work_src", work_src.c_str());
		};
	};

	/** A command to run as part of a packages build step
	 *  Stores the enviroment and arguements for a given command
	 *  Used to hold commands betwen the config loading step and package build steps.
	 */
	class PackageCmd {
	private:
		char *path;
		char *app;
		char **args;
		size_t arg_count;
		char **envp;
		size_t envp_count;
		bool skip;
	public:
		/** Create a Package Command
		 *  \param path The path to run this command in
		 *  \param app The program to invoke
		 */
		PackageCmd(const char *path, const char *app):path(strdup(path)),
		    app(strdup(app)), args(NULL), arg_count(0), envp(NULL), envp_count(0),
		    skip(false) {
			this->addArg(app);
		};
		PackageCmd(std::string const &path,
			   const char *app):path(strdup(path.c_str())), app(strdup(app)),
		    args(NULL), arg_count(0), envp(NULL), envp_count(0), skip(false) {
			this->addArg(app);
		};
		PackageCmd(std::string const &path,
			   std::string const &app):path(strdup(path.c_str())),
		    app(strdup(app.c_str())), args(NULL), arg_count(0), envp(NULL),
		    envp_count(0), skip(false) {
			this->addArg(app);
		};

		~PackageCmd();

		//! Mark a command to allow skiping its execution
		void skipCommand(void) {
			this->skip = true;
		}

		/** Add an argument to this command
		 *  \param arg The argument to append to this command
		 */
		void addArg(const char *arg) {
			this->arg_count++;
			this->args =
			    (char **) realloc(this->args,
					      sizeof(char *) * (this->arg_count + 1));
			this->args[this->arg_count - 1] = strdup(arg);
			this->args[this->arg_count] = NULL;
		}
		void addArg(std::string const &arg) {
			addArg(arg.c_str());
		}
		void addArgFmt(const char *fmt, ...);

		/** Add an enviroment variable to this command
		 *  \param env The enviroment variable to append to this command
		 */
		void addEnv(const char *env) {
			this->envp_count++;
			this->envp =
			    (char **) realloc(this->envp,
					      sizeof(char *) * (this->envp_count + 1));
			this->envp[this->envp_count - 1] = strdup(env);
			this->envp[this->envp_count] = NULL;
		}
		void addEnv(std::string const &env) {
			addEnv(env.c_str());
		}

		/** Run this command
		 *  \param P The package to use in the command logging
		 */
		bool Run(Package * P);

		//! Print the command line
		void printCmd(const char *package);
	};


	/* A fetch unit
	 * Describes a way to retrieve a file/directory
	 */
	class FetchUnit {
	protected:
		std::string fetch_uri;	//!< URI of this unit
	public:
		FetchUnit(std::string uri):fetch_uri(uri) {
		};
		virtual bool fetch(Package * P, BuildDir * d) = 0;
		virtual bool force_updated() {
			return false;
		};
	};

	/* A downloaded file
	 */
	class DownloadFetch:public FetchUnit {
	protected:
		bool decompress;
		std::string filename;
	public:
		DownloadFetch(std::string uri, bool decompress,
			      std::string filename):FetchUnit(uri), decompress(decompress),
		    filename(filename) {
		};
		virtual bool fetch(Package * P, BuildDir * d);
	};

	/* A linked file/directory
	 */
	class LinkFetch:public FetchUnit {
	public:
		LinkFetch(std::string uri):FetchUnit(uri) {
		};
		virtual bool fetch(Package * P, BuildDir * d);
		virtual bool force_updated() {
			return true;
		};
	};

	/* A copied file/directory
	 */
	class CopyFetch:public FetchUnit {
	public:
		CopyFetch(std::string uri):FetchUnit(uri) {
		};
		virtual bool fetch(Package * P, BuildDir * d);
		virtual bool force_updated() {
			return true;
		};
	};



	/** An extraction unit
	 *  Describes a single step required to re-extract a package
	 */
	class ExtractionUnit {
	protected:
		std::string uri;	//!< URI of this unit
		std::string * hash;	//!< Hash of this unit
	public:
		ExtractionUnit():uri(std::string()), hash(NULL) {
		};
		virtual bool print(std::ostream & out) = 0;
		virtual std::string type() = 0;
		virtual bool extract(Package * P, BuildDir * b) = 0;
		virtual std::string URI() {
			return this->uri;
		};
		virtual std::string HASH() {
			return *this->hash;
		};
	};

	/** A build unit
	 *  Describes a single piece required to re-build a package
	 */
	class BuildUnit {
	public:
		BuildUnit() {
		};
		virtual bool print(std::ostream & out) = 0;
		virtual std::string type() = 0;
	};

	//! A compressed file extraction unit
	class CompressedFileExtractionUnit:public ExtractionUnit {
	public:
		CompressedFileExtractionUnit(const char *fName);
		virtual std::string HASH();
		virtual bool print(std::ostream & out) {
			out << this->type() << " " << this->
			    uri << " " << this->HASH() << std::endl;
			return true;
		}
	};

	//! A tar extraction unit
	class TarExtractionUnit:public CompressedFileExtractionUnit {
	public:
		//! Create an extraction unit for a tar file
		TarExtractionUnit(const char *fName):CompressedFileExtractionUnit(fName) {
		};
		virtual std::string type() {
			return std::string("TarFile");
		}
		virtual bool extract(Package * P, BuildDir * b);
	};

	class ZipExtractionUnit:public CompressedFileExtractionUnit {
	public:
		//! Create an extraction unit for a tar file
		ZipExtractionUnit(const char *fName):CompressedFileExtractionUnit(fName) {
		};
		virtual std::string type() {
			return std::string("ZipFile");
		}
		virtual bool extract(Package * P, BuildDir * b);
	};

	//! A patch file as part of the extraction step
	class PatchExtractionUnit:public ExtractionUnit {
	private:
		int level;
		char *patch_path;
	public:
		PatchExtractionUnit(int level, char *patch_path, char *patch);
		virtual bool print(std::ostream & out) {
			out << this->type() << " " << this->
			    level << " " << this->patch_path << " " << this->
			    uri << " " << this->HASH() << std::endl;
			return true;
		}
		virtual std::string type() {
			return std::string("PatchFile");
		}
		virtual bool extract(Package * P, BuildDir * b);
	};

	//! A file copy as part of the extraction step
	class FileCopyExtractionUnit:public ExtractionUnit {
	private:
	public:
		FileCopyExtractionUnit(const char *file);
		virtual bool print(std::ostream & out) {
			out << this->type() << " " << this->
			    uri << " " << this->HASH() << std::endl;
			return true;
		}
		virtual std::string type() {
			return std::string("FileCopy");
		}
		virtual bool extract(Package * P, BuildDir * b);
	};

	//! A git directory as part of the extraction step
	class GitDirExtractionUnit:public ExtractionUnit {
	protected:
		std::string toDir;
	public:
		GitDirExtractionUnit(const char *git_dir, const char *toDir);
		virtual bool print(std::ostream & out) {
			out << this->type() << " " << this->
			    modeName() << " " << this->uri << " " << this->
			    toDir << " " << this->HASH() << " " << (this->isDirty()? this->
								    dirtyHash() : "")
			    << std::endl;
			return true;
		}
		virtual std::string type() {
			return std::string("GitDir");
		}
		virtual bool extract(Package * P, BuildDir * b) = 0;
		virtual bool isDirty();
		virtual std::string dirtyHash();
		virtual std::string localPath() {
			return this->uri;
		}
		virtual std::string modeName() = 0;
	};

	//! A local linked-in git dir as part of an extraction step
	class LinkGitDirExtractionUnit:public GitDirExtractionUnit {
	public:
		LinkGitDirExtractionUnit(const char *git_dir,
					 const char *toDir):GitDirExtractionUnit(git_dir,
										 toDir) {
		};
		virtual bool extract(Package * P, BuildDir * bd);
		virtual std::string modeName() {
			return "link";
		};
	};

	//! A local copied-in git dir as part of an extraction step
	class CopyGitDirExtractionUnit:public GitDirExtractionUnit {
	public:
		CopyGitDirExtractionUnit(const char *git_dir,
					 const char *toDir):GitDirExtractionUnit(git_dir,
										 toDir) {
		};
		virtual bool extract(Package * P, BuildDir * bd);
		virtual std::string modeName() {
			return "copy";
		};
	};

	//! A remote git dir as part of an extraction step
	class GitExtractionUnit:public GitDirExtractionUnit, public FetchUnit {
	private:
		std::string refspec;
		std::string local;
	public:
		GitExtractionUnit(const char *remote, const char *local,
				  std::string refspec):GitDirExtractionUnit(remote, local),
		    FetchUnit(remote), refspec(refspec) {
		};
		virtual bool fetch(Package * P, BuildDir * d);
		virtual bool extract(Package * P, BuildDir * bd);
		virtual std::string modeName() {
			return "fetch";
		};
		virtual std::string localPath() {
			return this->local;
		}
	};

	//! A feature/value as part of the build step
	class FeatureValueUnit:public BuildUnit {
	private:
		std::string feature;
		std::string value;
	public:
		FeatureValueUnit(const char *feature,
				 const char *value):feature(std::string(feature)),
		    value(std::string(value)) {
		};
		virtual bool print(std::ostream & out);
		virtual std::string type() {
			return std::string("FeatureValue");
		}
	};

	//! A feature that is nil as part of the build step
	class FeatureNilUnit:public BuildUnit {
	private:
		std::string feature;
	public:
		FeatureNilUnit(const char *feature):feature(std::string(feature)) {
		};
		virtual bool print(std::ostream & out) {
			out << this->type() << " " << this->feature << std::endl;
			return true;
		}
		virtual std::string type() {
			return std::string("FeatureNil");
		}
	};

	//! A lua package file as part of the build step
	class PackageFileUnit:public BuildUnit {
	private:
		std::string uri;	//!< URI of this package file
		std::string hash;	//!< Hash of this package file
	public:
		PackageFileUnit(const char *file);
		virtual bool print(std::ostream & out) {
			out << this->type() << " " << this->
			    uri << " " << this->hash << std::endl;
			return true;
		}
		virtual std::string type() {
			return std::string("PackageFile");
		}
	};

	//! A lua require file as part of the build step
	class RequireFileUnit:public BuildUnit {
	private:
		std::string uri;	//!< URI of this package file
		std::string hash;	//!< Hash of this package file
	public:
		RequireFileUnit(const char *file);
		virtual bool print(std::ostream & out) {
			out << this->type() << " " << this->
			    uri << " " << this->hash << std::endl;
			return true;
		}
		virtual std::string type() {
			return std::string("RequireFile");
		}
	};

	//! An extraction info file as part of the build step
	class ExtractionInfoFileUnit:public BuildUnit {
	private:
		std::string uri;	//!< URI of this extraction info file
		std::string hash;	//!< Hash of this extraction info file
	public:
		ExtractionInfoFileUnit(const char *file);
		virtual bool print(std::ostream & out) {
			out << this->type() << " " << this->
			    uri << " " << this->hash << std::endl;
			return true;
		}
		virtual std::string type() {
			return std::string("ExtractionInfoFile");
		}
	};

	//! A build info file as part of the build step
	class BuildInfoFileUnit:public BuildUnit {
	private:
		std::string uri;	//!< URI of this build info file
		std::string hash;	//!< Hash of this build info file
	public:
		BuildInfoFileUnit(const char *file, std::string hash);
		virtual bool print(std::ostream & out) {
			out << this->type() << " " << this->
			    uri << " " << this->hash << std::endl;
			return true;
		}
		virtual std::string type() {
			return std::string("BuildInfoFile");
		}
	};

	//! An output (hash) info file as part of the build step
	class OutputInfoFileUnit:public BuildUnit {
	private:
		std::string uri;	//!< URI of this output info file
		std::string hash;	//!< Hash of this output info file
	public:
		OutputInfoFileUnit(const char *file);
		virtual bool print(std::ostream & out) {
			out << this->type() << " " << this->
			    uri << " " << this->hash << std::endl;
			return true;
		}
		virtual std::string type() {
			return std::string("OutputInfoFile");
		}
	};


	/** A fetch description
	 *  Describes all the fetching required for a package
	 *  Used for checking if a package needs anything downloaded
	 */
	class Fetch {
	private:
		FetchUnit ** FUs;
		size_t FU_count;
	public:
		Fetch():FUs(NULL), FU_count(0) {
		};
		bool add(FetchUnit * fu);
		bool fetch(Package * P, BuildDir * d) {
			for(size_t i = 0; i < this->FU_count; i++) {
				if(!FUs[i]->fetch(P, d))
					return false;
			}
			return true;
		};
	};

	/** An extraction description
	 *  Describes all the steps and files required to re-extract a package
	 *  Used for checking a package needs extracting again
	 */
	class Extraction {
	private:
		ExtractionUnit ** EUs;
		size_t EU_count;
		bool extracted;
	public:
		Extraction():EUs(NULL), EU_count(0), extracted(false) {
		};
		bool add(ExtractionUnit * eu);
		bool print(std::ostream & out) {
			for(size_t i = 0; i < this->EU_count; i++) {
				if(!EUs[i]->print(out))
					return false;
			}
			return true;
		}
		bool extract(Package * P, BuildDir * bd);
		void prepareNewExtractInfo(Package * P, BuildDir * bd);
		bool extractionRequired(Package * P, BuildDir * bd);
		ExtractionInfoFileUnit *extractionInfo(Package * P, BuildDir * bd);
	};

	/** A build description
	 *  Describes relevant information to determine if a package needs rebuilding
	 */
	class BuildDescription {
	private:
		BuildUnit ** BUs;
		size_t BU_count;
	public:
		BuildDescription():BUs(NULL), BU_count(0) {
		};
		bool add(BuildUnit * bu);
		bool print(std::ostream & out) {
			for(size_t i = 0; i < this->BU_count; i++) {
				if(!BUs[i]->print(out))
					return false;
			}
			return true;
		}
	};

	//! A namespace
	class NameSpace {
	private:
		std::string name;
		std::list < Package * >packages;
	public:
		NameSpace(std::string name):name(name) {
		};
		std::string getName() {
			return name;
		};
		std::list < Package * >::iterator packagesStart();
		std::list < Package * >::iterator packagesEnd();
		//! Find or create a package with the given name
		Package *findPackage(std::string name);
		void addPackage(Package * p);
		void removePackage(Package * p);
		std::string getStagingDir();
		std::string getInstallDir();
	};

	//! A package to build
	class Package {
	private:
		std::list < Package * >depends;
		std::list < PackageCmd * >commands;
		std::string name;
		std::string file;
		std::string overlay;
		std::string buildinfo_hash;
		NameSpace *ns;
		BuildDir *bd;
		Fetch *f;
		Extraction *Extract;
		BuildDescription *build_description;
		Lua *lua;
		bool intercept;
		char *depsExtraction;
		string_list installFiles;
		bool visiting;
		bool processed;
		bool built;
		bool building;
		bool codeUpdated;
		bool was_built;
		bool no_fetch_from;
		bool hash_output;
		pthread_mutex_t lock;
		time_t run_secs;
		//! Update the buildinfo hash file
		void updateBuildInfoHash();
	protected:
		//! Extract the new staging directory this package created in the given path
		bool extract_staging(const char *dir, std::list < std::string > *done);
		//! Extract the new install directory this package created in the given path
		bool extract_install(const char *dir, std::list < std::string > *done);
		//! prepare the (new) build.info file
		void prepareBuildInfo();
		//! update the build.info file
		void updateBuildInfo(bool hashOutput = true);
		//! Attempt to fetchFrom
		bool fetchFrom();
		//! Return a new build unit (the build info) for this package
		BuildUnit *buildInfo();
		//! Prepare the (new) staging/install directories for the building of this package
		bool prepareBuildDirs();
		//! Extract all dependencies install dirs for this package (if bd:fetch(,'deps') was used)
		bool extractInstallDepends();
		//! Package up the new/staging directory
		bool packageNewStaging();
		//! Package up the new/install directory (or installFile, if set)
		bool packageNewInstall();
	public:
		/** Create a package
		 *  Constucts a package with a given name, based on a specific file
		 *  \param name The name of this package
		 *  \param file The lua file describing this package
		 */
		Package(NameSpace * ns, std::string name, std::string file,
			std::string overlay):name(name), file(file), overlay(overlay),
		    buildinfo_hash(""), ns(ns), bd(new BuildDir(this)), f(new Fetch()),
		    Extract(new Extraction()), build_description(new BuildDescription()),
		    lua(new Lua()), intercept(false), depsExtraction(NULL), visiting(false),
		    processed(false), built(false), building(false),
		    codeUpdated(false), was_built(false), no_fetch_from(false),
		    hash_output(false), run_secs(0) {
			pthread_mutex_init(&this->lock, NULL);
		};
		//! Set the namespace this package is in
		void setNS(NameSpace * ns) {
			if(this->ns)
				this->ns->removePackage(this);
			this->ns = ns;
			if(this->ns)
				this->ns->addPackage(this);
			this->resetBD();
		};
		//! Returns the namespace this package is in
		NameSpace *getNS() {
			return this->ns;
		};
		//! Returns the build directory being used by this package
		BuildDir *builddir();
		//! Get the absolute fetch path for this package
		char *absolute_fetch_path(const char *location);
		//! Get the relative fetch path for this package
		char *relative_fetch_path(const char *location);
		//! Recreate the build directory
		void resetBD();
		//! Returns the extraction
		Extraction *extraction() {
			return this->Extract;
		};
		//! Returns the fetch
		Fetch *fetch() {
			return this->f;
		};
		//! Returns the builddescription
		BuildDescription *buildDescription() {
			return this->build_description;
		};
		/** Convert this package to the intercepting type
		 *  Intercepting packages stop the extract install method from recursing past them.
		 */
		void setIntercept() {
			this->intercept = true;
		};
		//! Return the name of this package
		std::string getName() {
			return this->name;
		};
		//! Return the overlay this package is from
		std::string getOverlay() {
			return this->overlay;
		};
		/** Depend on another package
		 *  \param p The package to depend on
		 */
		void depend(Package * p) {
			this->depends.push_back(p);
		};
		/** Set the location to extract install directories to
		 *  During the build, all files that all dependencies have installed
		 *  will be extracted to the given path
		 *  \param de relative path to extract dependencies to
		 */
		void setDepsExtract(char *de) {
			this->depsExtraction = de;
		};
		//! Add a command to run during the build stage
			/** \param pc The comamnd to run
			  */
		void addCommand(PackageCmd * pc) {
			this->commands.push_back(pc);
		};
		/** Set the file to install
		 *  Setting this overrides to standard zipping up of the entire new install directory
		 *  and just installs this specific file
		 *  \param i the file to install
		 */
		void setInstallFile(char *i) {
			this->installFiles.push_back(std::string(i));
		};
		//! Parse and load the lua file for this package
		bool process();
		//! Sets the code updated flag
		void setCodeUpdated() {
			this->codeUpdated = true;
		};
		//! Is the code updated flag set
		bool isCodeUpdated() {
			return this->codeUpdated;
		};
		/** Is this package ready for building yet ?
		 *  \return true iff all all dependencies are built
		 */
		bool canBuild();
		/** Is this package already built ?
		 *  \return true if this package has already been built during this invocation of buildsys
		 */
		bool isBuilt();
		/** Was this package actually built ?
		 *  \return true iff this package's build commands were actually run
		 */
		bool wasBuilt() {
			return this->was_built;
		}
		//! Disable fetch-from for this package
		void disableFetchFrom() {
			this->no_fetch_from = true;
		};
		//! Can we fetch-from ?
		bool canFetchFrom() {
			return !this->no_fetch_from;
		};
		//! Hash the output for this package
		void setHashOutput() {
			this->hash_output = true;
		};
		//! Is this package hashing its' output ?
		bool isHashingOutput() {
			return this->hash_output;
		};
		/** Should this package be rebuilt ?
		 *  This returns true when any of the following occur:
		 *  - The output staging or install tarballs are removed
		 *  - The package file changes
		 *  - Any of the feature values that are used by the package change
		 */
		bool shouldBuild();
		//! Build this package
		bool build();
		//! Has building of this package already started ?
		bool isBuilding();
		//! Tell this package it is now building
		void setBuilding();

		//! Get the start iterator for the dependencies list
		std::list < Package * >::iterator dependsStart();
		//! Get the end iterator for the dependencies list
		std::list < Package * >::iterator dependsEnd();

		/** Print the label for use on the graph
		 * Prints the package name, number of commands to run, and time spent building
		 */
		void printLabel(std::ostream & out);

		//! Return the lua instance being used
		Lua *getLua() {
			return this->lua;
		};
	};

	//! A graph of dependencies between packages
	class Internal_Graph {
	private:
		typedef std::map < Package *, Vertex > NodeVertexMap;
		typedef std::map < Vertex, Package * >VertexNodeMap;
		typedef std::vector < Vertex > container;
		Graph g;
		NodeVertexMap *Nodes;
		VertexNodeMap *NodeMap;
		container *c;
	public:
		//! Create an Internal_Graph
		Internal_Graph();
		~Internal_Graph();
		//! Output the graph to dependencies.dot
		void output();
		//! Perform a topological sort
		void topological();
		//! Find a package with no dependencies
		Package *topoNext();
		//! Remove a package from this graph
		void deleteNode(Package * p);
	};

	//! The world, everything that everything needs to access
	class World {
	private:
		std::string bsapp;
		key_value *features;
		string_list *forcedDeps;
		std::list < NameSpace * >*namespaces;
		Package *p;
		string_list *overlays;
		Internal_Graph *graph;
		Internal_Graph *topo_graph;
		std::string fetch_from;
		std::string tarball_cache;
		std::string * pwd;
		string_list *ignoredFeatures;
		bool failed;
		bool cleaning;
		bool extractOnly;
		bool parseOnly;
		bool keepGoing;
		pthread_mutex_t cond_lock;
		pthread_cond_t cond;
		int threads_running;
		bool outputPrefix;
	public:
		World(char *bsapp):bsapp(std::string(bsapp)), features(new key_value()),
		    forcedDeps(new string_list()),
		    namespaces(new std::list < NameSpace * >()),
		    overlays(new string_list()), graph(NULL),
		    ignoredFeatures(new string_list()), failed(false), cleaning(false),
		    extractOnly(false), parseOnly(false), keepGoing(false),
		    threads_running(0), outputPrefix(true) {
			overlays->push_back(std::string("."));
			char *pwd = getcwd(NULL, 0);
			this->pwd = new std::string(pwd);
			free(pwd);
			pthread_mutex_init(&this->cond_lock, NULL);
			pthread_cond_init(&this->cond, NULL);
		};

		~World();

		//! Return the name we were invoked as
		std::string getAppName() {
			return this->bsapp;
		};

		/** Are we operating in 'forced' mode
		 *  If more than 1 parameter was passed on the command line,
		 *  we are operating in forced mode.
		 *  This means that we ignore any detection of what needs building,
		 *  and build only a specific set of packages (all the arguments, except the first one)
		 */
		bool forcedMode() {
			return !forcedDeps->empty();
		};
		/** Add a package to 'forced' mode
		 *  This will automatically turn on forced mode
		 */
		void forceBuild(std::string name) {
			forcedDeps->push_back(name);
		};
		//! Check if a specific package is being forced
		bool isForced(std::string name);
		/** Are we operating in 'cleaning' mode
		 *  If --clean is parsed as a parameter, we run in cleaning mode
		 *  This will make any package that would have been built
		 *  clean out its working directory instead
		 *  (Ignoring dependency scanning, but obeying forced mode)
		 */
		bool areCleaning() {
			return this->cleaning;
		}
		//! Set cleaning mode
		void setCleaning() {
			this->cleaning = true;
		}
		/** Are we operating in 'extract only' mode
		 *  If --extract-only is parsed as a parameter, we run in 'extract-only' mode
		 *  This will make buildsys stop after extracting all sources (obeying all other package filtering rules)
		 */
		bool areExtractOnly() {
			return this->extractOnly;
		}
		//! Set extract only mode
		void setExtractOnly() {
			this->extractOnly = true;
		}

		/** Are we operating in 'parse only' mode
		 *  If --parse-only is parsed as a parameter, we run in 'parse-only' mode
		 *  This will make buildsys stop after parsing all packages (package filtering rules have no impact)
		 */
		bool areParseOnly() {
			return this->parseOnly;
		}
		//! Set parase only mode
		void setParseOnly() {
			this->parseOnly = true;
		}

		/** Are we operating in 'keep going' mode
		 *  If --keep-going is parsed as a parameter, we run in 'keep-going' mode
		 *  This will make buildsys finish any packages it is currently building before exiting if there is a fault
		 */
		bool areKeepGoing() {
			return this->keepGoing;
		}
		//! Set keep only mode
		void setKeepGoing() {
			this->keepGoing = true;
		}

		/** Are we expected to output the package name as a prefix
		 *  If --no-output-prefix is parsed as a parameter, we don't prefix package output.
		 *  This will make it so that menuconfig doesn't look horrible.
		 *  At the expense of making it much harder to debug when the build breaks.
		 */
		bool areOutputPrefix() {
			return this->outputPrefix;
		}
		//! clear output prefix mode
		void clearOutputPrefix() {
			this->outputPrefix = false;
		}
		/** Set a feature to a specific value
		 *  Note that the default is to not set already-set features to new values
		 *  pass override=true to ignore this safety
		 *  lua: feature('magic-support','yes',true)
		 */
		bool setFeature(std::string key, std::string value, bool override = false);
		/** Set a feature using a key=value string
		 *  This is used to handle the command line input of feature/value settings
		 */
		bool setFeature(char *kv);
		/** Get the value of a specific feature
		 *  lua: feature('magic-support')
		 */
		std::string getFeature(std::string key);
		/** Print all feature/values to the console
		 */
		void printFeatureValues();

		//! Ignore a feature for build.info
		void ignoreFeature(std::string feature) {
			this->ignoredFeatures->push_back(feature);
		}
		//! Is a feature ignored
		bool isIgnoredFeature(std::string feature);
		//! Is the ignore list empty ?
		bool noIgnoredFeatures() {
			return this->ignoredFeatures->empty();
		}

		//! Start the processing and building steps with the given meta package
		bool basePackage(char *filename);

		//! Get the start iterator for the namespace list
		std::list < NameSpace * >::iterator nameSpacesStart() {
			return this->namespaces->begin();
		};
		//! Get the end iterator for the namespace list
		std::list < NameSpace * >::iterator nameSpacesEnd() {
			return this->namespaces->end();
		};
		//! Find (or create) a namespace
		NameSpace *findNameSpace(std::string name);

		//! Tell everything that we have failed
		void setFailed() {
			this->failed = true;
		};
		//! Test if we have failed
		bool isFailed() {
			return this->failed;
		}
		//! Declare a package built
		bool packageFinished(Package * p);

		//! Allow the fetch from location to be set
		void setFetchFrom(std::string from) {
			this->fetch_from = from;
		}
		//! Test if the fetch from location is set
		bool canFetchFrom() {
			return (this->fetch_from != "");
		}
		//! Test if the fetch from location is set
		std::string fetchFrom() {
			return this->fetch_from;
		}

		//! Set the location of the local tarball cache
		void setTarballCache(std::string cache) {
			this->tarball_cache = cache;
		}
		//! Test if the tarball cache location is set
		bool haveTarballCache() {
			return (this->tarball_cache != "");
		}
		//! Get the the tarball Cache location
		std::string tarballCache() {
			return this->tarball_cache;
		}

		//! Get 'pwd'
		std::string * getWorkingDir() {
			return this->pwd;
		}

		//! Add an overlay to search for packages
		void addOverlayPath(std::string path) {
			this->overlays->push_back(path);
		};
		//! Get the start iterator for the overlay list
		string_list::iterator overlaysStart();
		//! Get the end iterator for the over list
		string_list::iterator overlaysEnd();
		//! A thread has started
		void threadStarted() {
			pthread_mutex_lock(&this->cond_lock);
			this->threads_running++;
			pthread_mutex_unlock(&this->cond_lock);
		}
		//! A thread has finished
		void threadEnded() {
			pthread_mutex_lock(&this->cond_lock);
			this->threads_running--;
			pthread_cond_broadcast(&this->cond);
			pthread_mutex_unlock(&this->cond_lock);
		};
		//! output the dependency graph
		bool output_graph() {
			if(this->graph != NULL) {
				graph->output();
			};
			return true;
		};
		//! populate the arguments list with out forced build list
		bool populateForcedList(PackageCmd * pc);
	};
	extern World *WORLD;

	bool interfaceSetup(Lua * lua);
	void log(const char *package, const char *fmt, ...);
	void log(Package * P, const char *fmt, ...);
	void program_output(Package * P, const char *mesg);
	int run(Package * P, char *program, char *argv[], const char *path,
		char *newenvp[]);

	void hash_setup(void);
	char *hash_file(const char *fname);
	void hash_shutdown(void);
};

using namespace buildsys;
