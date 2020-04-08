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

#ifndef INCLUDE_BUILDSYS_H_
#define INCLUDE_BUILDSYS_H_

#include <atomic>
#include <condition_variable>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <vector>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <boost/algorithm/string.hpp>
#include <boost/config.hpp>
#include <boost/format.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/breadth_first_search.hpp>
#include <boost/graph/connected_components.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <boost/graph/graph_utility.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/topological_sort.hpp>
#include <boost/graph/visitors.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/utility.hpp>

#include "include/filesystem.h"

using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS>;
using Vertex = boost::graph_traits<Graph>::vertex_descriptor;
using Edge = boost::graph_traits<Graph>::edge_descriptor;

#define error(M)                                                                           \
	do {                                                                                   \
		log("BuildSys", boost::format{"%1%:%2%():%3%: %4%"} % __FILE__ % __FUNCTION__ %    \
		                    __LINE__ % (M));                                               \
	} while(0)

#define LUA_SET_TABLE_TYPE(L, T)                                                           \
	lua_pushstring(L, #T);                                                                 \
	lua_pushstring(L, "yes");                                                              \
	lua_settable(L, -3);

#define LUA_ADD_TABLE_FUNC(L, N, F)                                                        \
	lua_pushstring(L, N);                                                                  \
	lua_pushcfunction(L, F);                                                               \
	lua_settable(L, -3);

#define CREATE_TABLE(L, D)                                                                 \
	lua_newtable(L);                                                                       \
	lua_pushstring(L, "data");                                                             \
	lua_pushlightuserdata(L, D);                                                           \
	lua_settable(L, -3);

#define SET_TABLE_TYPE(L, T)                                                               \
	lua_pushstring(L, #T);                                                                 \
	lua_pushstring(L, "yes");                                                              \
	lua_settable(L, -3);

#define LUA_ADD_TABLE_TABLE(L, N, D)                                                       \
	lua_pushstring(L, N);                                                                  \
	CREATE_TABLE(D)                                                                        \
	(D)->lua_table(L);                                                                     \
	lua_settable(L, -3);

#define LUA_ADD_TABLE_STRING(L, N, S)                                                      \
	lua_pushstring(L, N);                                                                  \
	lua_pushstring(L, S);                                                                  \
	lua_settable(L, -3);

extern "C" {
int li_bd_cmd(lua_State *L);
int li_bd_extract(lua_State *L);
int li_bd_fetch(lua_State *L);
int li_bd_installfile(lua_State *L);
int li_bd_patch(lua_State *L);
int li_bd_restore(lua_State *L);

int li_fu_path(lua_State *L);
}

namespace buildsys
{
	using string_list = std::list<std::string>;

	class Lua;
	class Package;
	class World;

	bool interfaceSetup(Lua *lua);
	void log(const std::string &package, const std::string &str);
	void log(const std::string &package, const boost::format &str);
	void log(Package *P, const std::string &str);
	void log(Package *P, const boost::format &str);
	void program_output(Package *P, const std::string &mesg);
	int run(Package *P, const std::string &program, const std::vector<std::string> &argv,
	        const std::string &path, const std::vector<std::string> &newenvp);

	void hash_setup();
	std::string hash_file(const std::string &fname);
	void hash_shutdown();

	/*! The catchable exception */
	class Exception : public std::exception
	{
	public:
		~Exception() override = default;
		//! Return the error message for this exception
		virtual std::string error_msg()
		{
			return "";
		};
	};

	/*! An exception with a custom error message */
	class CustomException : public Exception
	{
	private:
		std::string errmsg;

	public:
		/** Construct an exception with a specific error message
		 *  \param err The erorr message
		 */
		explicit CustomException(std::string err) : errmsg(std::move(err))
		{
		}
		std::string error_msg() override
		{
			return errmsg;
		}
	};

	/*! A Lua fault */
	class LuaException : public Exception
	{
	public:
		std::string error_msg() override
		{
			return "Lua Error";
		}
	};

	/*! A Memory fault */
	class MemoryException : public Exception
	{
	public:
		std::string error_msg() override
		{
			return "Memory Error";
		}
	};

	/*! No Such Key fault */
	class NoKeyException : public Exception
	{
	public:
		std::string error_msg() override
		{
			return "Key does not exist";
		}
	};

	/*! File not found */
	class FileNotFoundException : public Exception
	{
	private:
		std::string errmsg;

	public:
		/** Create a file not found exception
		 *  \param file the file that was not found
		 *  \param where The location where the error occurred
		 */
		FileNotFoundException(const std::string &file, const std::string &where)
		{
			auto msg = boost::format{"%1%: File not found '%2%'"} % where % file;
			this->errmsg = msg.str();
		}
		std::string error_msg() override
		{
			return errmsg;
		}
	};

	/*! A C++ wrapper for a lua state (instance) */
	class Lua
	{
	private:
		lua_State *state;

	public:
		/** Create a lua state
		 * Creates a new lua state, and opens loads our libraries into it
		 */
		Lua()
		{
			state = luaL_newstate();
			if(state == nullptr) {
				throw LuaException();
			}
			luaL_openlibs(state);
		};

		~Lua()
		{
			lua_close(state);
			this->state = nullptr;
		}
		/** Load and exexcute a lua file in this instance
		 *  \param filename The name of the lua file to load and run
		 */
		void processFile(const std::string &filename)
		{
			if(luaL_dofile(state, filename.c_str()) != 0) {
				throw CustomException(lua_tostring(state, -1));
			}
		}
		/** Register a function in this lua instance
		 *  \param name The name of the function
		 *  \param fn The function to call
		 */
		void registerFunc(const std::string &name, lua_CFunction fn)
		{
			lua_register(state, name.c_str(), fn);
		}
		/** Set a global variable to given data
		 *  \param name the name to store this data as
		 *  \param data the data
		 */
		void setGlobal(const std::string &name, void *data)
		{
			lua_pushlightuserdata(state, data);
			lua_setglobal(state, name.c_str());
		}
		//! Get the lua_State
		lua_State *luaState()
		{
			return state;
		}
	};

	/** A directory for building a package in
	 *  Each package has one build directory which is used to run all the commands in
	 */
	class BuildDir
	{
	private:
		std::string path;
		std::string rpath;
		std::string staging;
		std::string new_path;
		std::string new_staging;
		std::string new_install;
		std::string work_build;
		std::string work_src;
		World *WORLD;

	public:
		/** Create a build directory
		 *  \param P The package this directory is for
		 */
		explicit BuildDir(Package *P);
		//! Return the full path to this directory
		const std::string &getPath() const
		{
			return this->path;
		};
		//! Get the short version (relative only) of the working directory
		const std::string &getShortPath() const
		{
			return this->rpath;
		};
		//! Return the full path to the staging directory
		const std::string &getStaging() const
		{
			return this->staging;
		};
		//! Return the full path to the new directory
		const std::string &getNewPath() const
		{
			return this->new_path;
		};
		//! Return the full path to the new staging directory
		const std::string &getNewStaging() const
		{
			return this->new_staging;
		};
		//! Return the full path to the new install directory
		const std::string &getNewInstall() const
		{
			return this->new_install;
		};
		//! Get the world this builddir is part of
		World *getWorld()
		{
			return this->WORLD;
		};
		//! Remove all of the work directory contents
		void clean() const;
		//! Remove all of the staging directory contents
		void cleanStaging() const;

		void lua_table(lua_State *L)
		{
			LUA_SET_TABLE_TYPE(L, BuildDir);
			LUA_ADD_TABLE_FUNC(L, "cmd", li_bd_cmd);
			LUA_ADD_TABLE_FUNC(L, "extract", li_bd_extract);
			LUA_ADD_TABLE_FUNC(L, "fetch", li_bd_fetch);
			LUA_ADD_TABLE_FUNC(L, "installfile", li_bd_installfile);
			LUA_ADD_TABLE_FUNC(L, "patch", li_bd_patch);
			LUA_ADD_TABLE_FUNC(L, "restore", li_bd_restore);
			LUA_ADD_TABLE_STRING(L, "new_staging", new_staging.c_str());
			LUA_ADD_TABLE_STRING(L, "new_install", new_install.c_str());
			LUA_ADD_TABLE_STRING(L, "path", path.c_str());
			LUA_ADD_TABLE_STRING(L, "staging", staging.c_str());
		};
	};

	/** A command to run as part of a packages build step
	 *  Stores the enviroment and arguements for a given command
	 *  Used to hold commands betwen the config loading step and package build steps.
	 */
	class PackageCmd
	{
	private:
		std::string path;
		std::string app;
		std::vector<std::string> args;
		std::vector<std::string> envp;

	public:
		/** Create a Package Command
		 *  \param path The path to run this command in
		 *  \param app The program to invoke
		 */
		PackageCmd(std::string _path, std::string _app)
		    : path(std::move(_path)), app(std::move(_app))
		{
			this->addArg(app);
		};

		/** Add an argument to this command
		 *  \param arg The argument to append to this command
		 */
		void addArg(const std::string &arg)
		{
			this->args.push_back(arg);
		}

		/** Add an enviroment variable to this command
		 *  \param env The enviroment variable to append to this command
		 */
		void addEnv(const std::string &env)
		{
			this->envp.push_back(env);
		}

		/** Run this command
		 *  \param P The package to use in the command logging
		 */
		bool Run(Package *P);

		//! Print the command line
		void printCmd() const;
	};

	/* A Downloaded Object
	 * Used to prevent multiple packages downloading the same file at the same time
	 */
	class DLObject
	{
	private:
		const std::string filename;
		std::string hash;
		mutable std::mutex lock;

	public:
		explicit DLObject(std::string _filename) : filename(std::move(_filename))
		{
		}
		const std::string &fileName() const
		{
			return this->filename;
		}
		const std::string &HASH() const
		{
			return this->hash;
		}
		void setHASH(std::string _hash)
		{
			this->hash = std::move(_hash);
		}
		std::mutex &getLock() const
		{
			return this->lock;
		}
	};

	/* A hashable unit
	 * For fetch and extraction units.
	 */
	class HashableUnit
	{
	public:
		HashableUnit() = default;
		virtual ~HashableUnit() = default;
		virtual std::string HASH() = 0;
	};

	/* A fetch unit
	 * Describes a way to retrieve a file/directory
	 */
	class FetchUnit : public HashableUnit
	{
	protected:
		const std::string fetch_uri; //!< URI of this unit
		Package *P{nullptr};         //!< Which package is this fetch unit for ?
		bool fetched{false};

	public:
		FetchUnit(std::string uri, Package *_P) : fetch_uri(std::move(uri)), P(_P)
		{
		}
		FetchUnit() = default;
		~FetchUnit() override = default;
		virtual bool fetch(BuildDir *d) = 0;
		virtual bool force_updated()
		{
			return false;
		};
		virtual std::string relative_path() = 0;
		static void lua_table_r(lua_State *L)
		{
			LUA_SET_TABLE_TYPE(L, FetchUnit);
			LUA_ADD_TABLE_FUNC(L, "path", li_fu_path);
		}
		virtual void lua_table(lua_State *L)
		{
			lua_table_r(L);
		};
	};

	/* A downloaded file
	 */
	class DownloadFetch : public FetchUnit
	{
	protected:
		const bool decompress;
		const std::string filename;
		std::string hash;
		std::string full_name();
		std::string final_name();

	public:
		DownloadFetch(std::string _uri, bool _decompress, std::string _filename,
		              Package *_P)
		    : FetchUnit(std::move(_uri), _P), decompress(_decompress),
		      filename(std::move(_filename))
		{
		}
		bool fetch(BuildDir *d) override;
		std::string HASH() override;
		std::string relative_path() override
		{
			return "dl/" + this->final_name();
		};
	};

	/* A linked file/directory
	 */
	class LinkFetch : public FetchUnit
	{
	public:
		LinkFetch(std::string uri, Package *_P) : FetchUnit(std::move(uri), _P)
		{
		}
		bool fetch(BuildDir *d) override;
		bool force_updated() override
		{
			return true;
		};
		std::string HASH() override;
		std::string relative_path() override;
	};

	/* A copied file/directory
	 */
	class CopyFetch : public FetchUnit
	{
	public:
		CopyFetch(std::string uri, Package *_P) : FetchUnit(std::move(uri), _P)
		{
		}
		bool fetch(BuildDir *d) override;
		bool force_updated() override
		{
			return true;
		};
		std::string HASH() override;
		std::string relative_path() override;
	};

	/** An extraction unit
	 *  Describes a single step required to re-extract a package
	 */
	class ExtractionUnit : public HashableUnit
	{
	protected:
		std::string uri;  //!< URI of this unit
		std::string hash; //!< Hash of this unit
	public:
		ExtractionUnit() : uri(std::string()), hash(std::string())
		{
		}
		~ExtractionUnit() override = default;
		virtual void print(std::ostream &out) = 0;
		virtual std::string type() = 0;
		virtual bool extract(Package *P) = 0;
		virtual std::string URI()
		{
			return this->uri;
		};
		std::string HASH() override
		{
			return this->hash;
		};
	};

	/** A build unit
	 *  Describes a single piece required to re-build a package
	 */
	class BuildUnit
	{
	public:
		virtual ~BuildUnit() = default;
		virtual void print(std::ostream &out) = 0;
		virtual std::string type() = 0;
	};

	//! A compressed file extraction unit
	class CompressedFileExtractionUnit : public ExtractionUnit
	{
	protected:
		FetchUnit *fetch;

	public:
		explicit CompressedFileExtractionUnit(const std::string &fname);
		explicit CompressedFileExtractionUnit(FetchUnit *f);
		std::string HASH() override;
		void print(std::ostream &out) override
		{
			out << this->type() << " " << this->uri << " " << this->HASH() << std::endl;
		}
	};

	//! A tar extraction unit
	class TarExtractionUnit : public CompressedFileExtractionUnit
	{
	public:
		//! Create an extraction unit for a tar file
		explicit TarExtractionUnit(const std::string &fname)
		    : CompressedFileExtractionUnit(fname)
		{
		}
		//! Create an extraction unit for tar file from a fetch unit
		explicit TarExtractionUnit(FetchUnit *f) : CompressedFileExtractionUnit(f)
		{
		}
		std::string type() override
		{
			return std::string("TarFile");
		}
		bool extract(Package *P) override;
	};

	class ZipExtractionUnit : public CompressedFileExtractionUnit
	{
	public:
		//! Create an extraction unit for a tar file
		explicit ZipExtractionUnit(const std::string &fname)
		    : CompressedFileExtractionUnit(fname)
		{
		}
		explicit ZipExtractionUnit(FetchUnit *f) : CompressedFileExtractionUnit(f)
		{
		}
		std::string type() override
		{
			return std::string("ZipFile");
		}
		bool extract(Package *P) override;
	};

	//! A patch file as part of the extraction step
	class PatchExtractionUnit : public ExtractionUnit
	{
	private:
		int level;
		std::string patch_path;
		std::string fname_short;

	public:
		PatchExtractionUnit(int _level, const std::string &_patch_path,
		                    const std::string &patch_fname,
		                    const std::string &_fname_short);
		~PatchExtractionUnit() override = default;
		void print(std::ostream &out) override
		{
			out << this->type() << " " << this->level << " " << this->patch_path << " "
			    << this->fname_short << " " << this->HASH() << std::endl;
		}
		std::string type() override
		{
			return std::string("PatchFile");
		}
		bool extract(Package *P) override;
	};

	//! A file copy as part of the extraction step
	class FileCopyExtractionUnit : public ExtractionUnit
	{
	private:
		std::string fname_short;

	public:
		FileCopyExtractionUnit(const std::string &fname, const std::string &_fname_short);
		void print(std::ostream &out) override
		{
			out << this->type() << " " << this->fname_short << " " << this->HASH()
			    << std::endl;
		}
		std::string type() override
		{
			return std::string("FileCopy");
		}
		bool extract(Package *P) override;
	};

	//! A file copy of a fetched file as part of the extraction step
	class FetchedFileCopyExtractionUnit : public ExtractionUnit
	{
	private:
		std::string fname_short;
		FetchUnit *fetched;

	public:
		FetchedFileCopyExtractionUnit(FetchUnit *_fetched, const std::string &_fname_short);
		void print(std::ostream &out) override
		{
			out << this->type() << " " << this->fname_short << " " << this->HASH()
			    << std::endl;
		}
		std::string type() override
		{
			return std::string("FetchedFileCopy");
		}
		bool extract(Package *P) override;
		std::string HASH() override;
	};

	//! A git directory as part of the extraction step
	class GitDirExtractionUnit : public ExtractionUnit
	{
	protected:
		std::string toDir;

	public:
		GitDirExtractionUnit(const std::string &git_dir, const std::string &to_dir);
		GitDirExtractionUnit() = default;
		void print(std::ostream &out) override
		{
			out << this->type() << " " << this->modeName() << " " << this->uri << " "
			    << this->toDir << " " << this->HASH() << " "
			    << (this->isDirty() ? this->dirtyHash() : "") << std::endl;
		}
		std::string type() override
		{
			return std::string("GitDir");
		}
		bool extract(Package *P) override = 0;
		virtual bool isDirty();
		virtual std::string dirtyHash();
		virtual std::string localPath()
		{
			return this->uri;
		}
		virtual std::string modeName() = 0;
	};

	//! A local linked-in git dir as part of an extraction step
	class LinkGitDirExtractionUnit : public GitDirExtractionUnit
	{
	public:
		LinkGitDirExtractionUnit(const std::string &git_dir, const std::string &to_dir)
		    : GitDirExtractionUnit(git_dir, to_dir)
		{
		}
		bool extract(Package *P) override;
		std::string modeName() override
		{
			return "link";
		};
	};

	//! A local copied-in git dir as part of an extraction step
	class CopyGitDirExtractionUnit : public GitDirExtractionUnit
	{
	public:
		CopyGitDirExtractionUnit(const std::string &git_dir, const std::string &to_dir)
		    : GitDirExtractionUnit(git_dir, to_dir)
		{
		}
		bool extract(Package *P) override;
		std::string modeName() override
		{
			return "copy";
		};
	};

	//! A remote git dir as part of an extraction step
	class GitExtractionUnit : public GitDirExtractionUnit, public FetchUnit
	{
	private:
		std::string refspec;
		std::string local;
		bool updateOrigin();

	public:
		GitExtractionUnit(const std::string &remote, const std::string &_local,
		                  std::string _refspec, Package *_P);
		bool fetch(BuildDir *d) override;
		bool extract(Package *_P) override;
		std::string modeName() override
		{
			return "fetch";
		};
		std::string localPath() override
		{
			return this->local;
		};
		std::string HASH() override;
		std::string relative_path() override
		{
			return this->localPath();
		};
	};

	//! A feature/value as part of the build step
	class FeatureValueUnit : public BuildUnit
	{
	private:
		const std::string feature;
		const std::string value;
		World *WORLD;

	public:
		FeatureValueUnit(World *_WORLD, std::string _feature, std::string _value)
		    : feature(std::move(_feature)), value(std::move(_value)), WORLD(_WORLD)
		{
		}
		void print(std::ostream &out) override;
		std::string type() override
		{
			return std::string("FeatureValue");
		}
	};

	//! A feature that is nil as part of the build step
	class FeatureNilUnit : public BuildUnit
	{
	private:
		const std::string feature;

	public:
		explicit FeatureNilUnit(std::string _feature) : feature(std::move(_feature))
		{
		}
		void print(std::ostream &out) override
		{
			out << this->type() << " " << this->feature << std::endl;
		}
		std::string type() override
		{
			return std::string("FeatureNil");
		}
	};

	//! A lua package file as part of the build step
	class PackageFileUnit : public BuildUnit
	{
	private:
		std::string uri;  //!< URI of this package file
		std::string hash; //!< Hash of this package file
	public:
		PackageFileUnit(const std::string &fname, const std::string &_fname_short);
		void print(std::ostream &out) override
		{
			out << this->type() << " " << this->uri << " " << this->hash << std::endl;
		}
		std::string type() override
		{
			return std::string("PackageFile");
		}
	};

	//! A lua require file as part of the build step
	class RequireFileUnit : public BuildUnit
	{
	private:
		std::string uri;  //!< URI of this package file
		std::string hash; //!< Hash of this package file
	public:
		RequireFileUnit(const std::string &fname, const std::string &_fname_short);
		void print(std::ostream &out) override
		{
			out << this->type() << " " << this->uri << " " << this->hash << std::endl;
		}
		std::string type() override
		{
			return std::string("RequireFile");
		}
	};

	//! An extraction info file as part of the build step
	class ExtractionInfoFileUnit : public BuildUnit
	{
	private:
		std::string uri;  //!< URI of this extraction info file
		std::string hash; //!< Hash of this extraction info file
	public:
		explicit ExtractionInfoFileUnit(const std::string &fname);
		void print(std::ostream &out) override
		{
			out << this->type() << " " << this->uri << " " << this->hash << std::endl;
		}
		std::string type() override
		{
			return std::string("ExtractionInfoFile");
		}
	};

	//! A build info file as part of the build step
	class BuildInfoFileUnit : public BuildUnit
	{
	private:
		std::string uri;  //!< URI of this build info file
		std::string hash; //!< Hash of this build info file
	public:
		BuildInfoFileUnit(const std::string &fname, const std::string &_hash);
		void print(std::ostream &out) override
		{
			out << this->type() << " " << this->uri << " " << this->hash << std::endl;
		}
		std::string type() override
		{
			return std::string("BuildInfoFile");
		}
	};

	//! An output (hash) info file as part of the build step
	class OutputInfoFileUnit : public BuildUnit
	{
	private:
		std::string uri;  //!< URI of this output info file
		std::string hash; //!< Hash of this output info file
	public:
		explicit OutputInfoFileUnit(const std::string &fname);
		void print(std::ostream &out) override
		{
			out << this->type() << " " << this->uri << " " << this->hash << std::endl;
		}
		std::string type() override
		{
			return std::string("OutputInfoFile");
		}
	};

	/** A fetch description
	 *  Describes all the fetching required for a package
	 *  Used for checking if a package needs anything downloaded
	 */
	class Fetch
	{
	private:
		std::vector<std::unique_ptr<FetchUnit>> FUs;

	public:
		void add(std::unique_ptr<FetchUnit> fu);
		bool fetch(BuildDir *d)
		{
			for(auto &unit : this->FUs) {
				if(!unit->fetch(d)) {
					return false;
				}
			}
			return true;
		};
	};

	/** An extraction description
	 *  Describes all the steps and files required to re-extract a package
	 *  Used for checking a package needs extracting again
	 */
	class Extraction
	{
	private:
		std::vector<std::unique_ptr<ExtractionUnit>> EUs;
		bool extracted{false};

	public:
		void add(std::unique_ptr<ExtractionUnit> eu);
		void print(std::ostream &out) const
		{
			for(auto &unit : this->EUs) {
				unit->print(out);
			}
		}
		bool extract(Package *P);
		void prepareNewExtractInfo(Package *P, BuildDir *bd);
		bool extractionRequired(Package *P, BuildDir *bd) const;
		std::unique_ptr<ExtractionInfoFileUnit> extractionInfo(BuildDir *bd) const;
	};

	/** A build description
	 *  Describes relevant information to determine if a package needs rebuilding
	 */
	class BuildDescription
	{
	private:
		std::vector<std::unique_ptr<BuildUnit>> BUs;

	public:
		void add(std::unique_ptr<BuildUnit> bu);
		void print(std::ostream &out) const
		{
			for(auto &unit : this->BUs) {
				unit->print(out);
			}
		}
	};

	//! A namespace
	class NameSpace
	{
	private:
		const std::string name;
		std::list<Package *> packages;
		mutable std::mutex lock;
		void _addPackage(Package *p);
		void _removePackage(Package *p);
		World *WORLD;

	public:
		NameSpace(World *_W, std::string _name) : name(std::move(_name)), WORLD(_W)
		{
		}
		~NameSpace();
		const std::string &getName() const
		{
			return name;
		};
		const std::list<Package *> &getPackages() const
		{
			return packages;
		}
		//! Find or create a package with the given name
		Package *findPackage(const std::string &_name);
		void addPackage(Package *p);
		void removePackage(Package *p);
		std::string getStagingDir() const;
		std::string getInstallDir() const;
		//! Get the World this NS belongs to
		World *getWorld();
	};

	//! A dependency on another Package
	class PackageDepend
	{
	private:
		Package *p;
		const bool locally;

	public:
		//! Create a package dependency
		PackageDepend(Package *_p, bool _locally) : p(_p), locally(_locally)
		{
		}
		//! Get the package
		Package *getPackage()
		{
			return this->p;
		};
		//! Get the locally flag
		bool getLocally() const
		{
			return this->locally;
		};
	};

	//! A package to build
	class Package
	{
	private:
		std::list<PackageDepend> depends;
		std::list<PackageCmd> commands;
		std::string name;
		std::string file;
		std::string file_short;
		std::string overlay;
		std::string buildinfo_hash;
		const std::string pwd;
		NameSpace *ns;
		BuildDir bd;
		Fetch f;
		Extraction Extract;
		BuildDescription build_description;
		Lua lua;
		bool intercept{false};
		std::string depsExtraction;
		bool depsExtractionDirectOnly{false};
		string_list installFiles;
		bool visiting{false};
		bool processing_queued{false};
		bool buildInfoPrepared{false};
		std::atomic<bool> built{false};
		std::atomic<bool> building{false};
		std::atomic<bool> was_built{false};
		bool codeUpdated{false};
		bool no_fetch_from{false};
		bool hash_output{false};
		bool suppress_remove_staging{false};
		mutable std::mutex lock;
		time_t run_secs{0};
		std::unique_ptr<std::ofstream> logFile;
		//! Set the buildinfo file hash from the new .build.info.new file
		void updateBuildInfoHash();
		//! Set the buildinfo file hash from the existing .build.info file
		void updateBuildInfoHashExisting();
		bool extract_staging(const std::string &dir);
		bool extract_install(const std::string &dir);
		void getAllDependedPackages(std::unordered_set<Package *> *packages);
		void getDependedPackages(std::unordered_set<Package *> *packages,
		                         bool include_children, bool ignore_intercept);

	protected:
		//! prepare the (new) build.info file
		void prepareBuildInfo();
		//! update the build.info file
		void updateBuildInfo(bool updateOutputHash = true);
		//! Attempt to fetchFrom
		bool fetchFrom();
		//! Return a new build unit (the build info) for this package
		std::unique_ptr<BuildUnit> buildInfo();
		//! Prepare the (new) staging/install directories for the building of this package
		bool prepareBuildDirs();
		//! Extract all dependencies install dirs for this package (if bd:fetch(,'deps') was
		//! used)
		bool extractInstallDepends();
		//! Package up the new/staging directory
		bool packageNewStaging();
		//! Package up the new/install directory (or installFile, if set)
		bool packageNewInstall();
		//! Remove the staging directory (to save space, if not suppressed)
		void cleanStaging() const;
		/** Should this package be rebuilt ?
		 *  This returns true when any of the following occur:
		 *  - The output staging or install tarballs are removed
		 *  - The package file changes
		 *  - Any of the feature values that are used by the package change
		 */
		bool shouldBuild(bool locally);

	public:
		/**
		 * Create a package.
		 *
		 * @param _ns - The namespace the package is in.
		 * @param _name - The name of the package.
		 * @param _file_short - The relative path to the lua file describing this package.
		 * @param _file - The full path to the lua file describing this package.
		 * @param _overlay - The overlay the package comes from.
		 * @param _pwd - The working directory for buildsys++.
		 */
		Package(NameSpace *_ns, std::string _name, std::string _file_short,
		        std::string _file, std::string _overlay, std::string _pwd)
		    : name(std::move(_name)), file(std::move(_file)),
		      file_short(std::move(_file_short)), overlay(std::move(_overlay)),
		      pwd(std::move(_pwd)), ns(_ns), bd(BuildDir(this))
		{
		}
		//! Set the namespace this package is in
		void setNS(NameSpace *_ns)
		{
			if(this->ns != nullptr) {
				this->ns->removePackage(this);
			}
			this->ns = _ns;
			if(this->ns != nullptr) {
				this->ns->addPackage(this);
			}
			this->bd = BuildDir(this);
		};
		//! Returns the namespace this package is in
		NameSpace *getNS()
		{
			return this->ns;
		};
		//! Return the world this package is in
		World *getWorld()
		{
			return this->ns->getWorld();
		};
		//! Returns the build directory being used by this package
		BuildDir *builddir();
		//! Get the absolute fetch path for this package
		std::string absolute_fetch_path(const std::string &location);
		//! Get the relative fetch path for this package
		std::string relative_fetch_path(const std::string &location,
		                                bool also_root = false);
		//! Get the file hash for the given file (if known)
		std::string getFileHash(const std::string &filename);
		//! Returns the extraction
		Extraction *extraction()
		{
			return &this->Extract;
		};
		//! Returns the fetch
		Fetch *fetch()
		{
			return &this->f;
		};
		//! Returns the builddescription
		BuildDescription *buildDescription()
		{
			return &this->build_description;
		};
		/** Convert this package to the intercepting type
		 *  Intercepting packages stop the extract install method from recursing past them.
		 */
		void setIntercept()
		{
			this->intercept = true;
		};
		bool getIntercept() const
		{
			return this->intercept;
		}
		//! Return the name of this package
		const std::string &getName() const
		{
			return this->name;
		};
		//! Return the overlay this package is from
		const std::string &getOverlay() const
		{
			return this->overlay;
		};
		//! Get the log file for this package
		std::ofstream &getLogFile();

		/** Depend on another package
		 *  \param p The package to depend on
		 */
		void depend(Package *P, bool locally)
		{
			this->depends.emplace_back(P, locally);
		};
		/** Set the location to extract install directories to
		 *  During the build, all files that all dependencies have installed
		 *  will be extracted to the given path
		 *  The directonly parameter limits this to only listed dependencies
		 *  i.e. not anything they also depend on
		 *  \param de relative path to extract dependencies to
		 */
		void setDepsExtract(const std::string &de, bool directonly)
		{
			this->depsExtraction = de;
			this->depsExtractionDirectOnly = directonly;
		};
		//! Add a command to run during the build stage
		/** \param pc The comamnd to run
		 */
		void addCommand(PackageCmd &&pc)
		{
			this->commands.push_back(std::move(pc));
		};
		/** Set the file to install
		 *  Setting this overrides to standard zipping up of the entire new install
		 * directory
		 *  and just installs this specific file
		 *  \param i the file to install
		 */
		void setInstallFile(const std::string &i)
		{
			this->installFiles.push_back(i);
		};
		//! Mark this package as queued for processing (returns false if already marked)
		bool setProcessingQueued()
		{
			std::unique_lock<std::mutex> lk(this->lock);
			bool res = !this->processing_queued;
			this->processing_queued = true;
			return res;
		}
		//! Set to prevent the staging directory from being removed at the end of the build
		void setSuppressRemoveStaging()
		{
			this->suppress_remove_staging = true;
		}
		//! Parse and load the lua file for this package
		bool process();
		//! Sets the code updated flag
		void setCodeUpdated()
		{
			this->codeUpdated = true;
		};
		//! Is the code updated flag set
		bool isCodeUpdated() const
		{
			return this->codeUpdated;
		};
		/** Is this package ready for building yet ?
		 *  \return true iff all all dependencies are built
		 */
		bool canBuild();
		/** Is this package already built ?
		 *  \return true if this package has already been built during this invocation of
		 * buildsys
		 */
		bool isBuilt() const
		{
			return this->built;
		}
		//! Disable fetch-from for this package
		void disableFetchFrom()
		{
			this->no_fetch_from = true;
		};
		//! Can we fetch-from ?
		bool canFetchFrom() const
		{
			return !this->no_fetch_from;
		};
		//! Hash the output for this package
		void setHashOutput()
		{
			this->hash_output = true;
		};
		//! Is this package hashing its' output ?
		bool isHashingOutput() const
		{
			return this->hash_output;
		};

		//! Check for any dependency loops
		bool checkForDependencyLoops();

		//! Build this package
		bool build(bool locally = false);
		//! Has building of this package already started ?
		bool isBuilding() const
		{
			return this->building;
		}
		//! Tell this package it is now building
		void setBuilding()
		{
			this->building = true;
		}

		std::list<PackageDepend> &getDepends()
		{
			return this->depends;
		}

		/** Print the label for use on the graph
		 * Prints the package name, number of commands to run, and time spent building
		 */
		void printLabel(std::ostream &out);

		/** Get the value of a specific feature (for this package)
		 *  lua: feature('magic-support')
		 */
		std::string getFeature(const std::string &key);

		//! Return the lua instance being used
		Lua *getLua()
		{
			return &this->lua;
		};

		//! Get the current working directory
		const std::string &getPwd() const
		{
			return this->pwd;
		}
	};

	//! A graph of dependencies between packages
	class Internal_Graph
	{
	private:
		using NodeVertexMap = std::map<Package *, Vertex>;
		using VertexNodeMap = std::map<Vertex, Package *>;
		using container = std::vector<Vertex>;
		Graph g;
		NodeVertexMap Nodes;
		VertexNodeMap NodeMap;
		container c;

	public:
		//! Fill the Internal_Graph
		void fill(World *W);
		//! Output the graph to dependencies.dot
		void output() const;
		//! Perform a topological sort
		void topological();
		//! Find a package with no dependencies
		Package *topoNext();
		//! Remove a package from this graph
		void deleteNode(Package *p);
	};

	class PackageQueue
	{
	private:
		std::list<Package *> queue;
		mutable std::mutex lock;
		mutable std::condition_variable cond;
		int started{0};
		int finished{0};

	public:
		void start()
		{
			std::unique_lock<std::mutex> lk(this->lock);
			this->started++;
		}
		void finish()
		{
			std::unique_lock<std::mutex> lk(this->lock);
			this->finished++;
			this->cond.notify_all();
		}
		void push(Package *p)
		{
			std::unique_lock<std::mutex> lk(this->lock);
			queue.push_back(p);
		}
		Package *pop()
		{
			std::unique_lock<std::mutex> lk(this->lock);
			Package *p = nullptr;
			if(!this->queue.empty()) {
				p = this->queue.front();
				this->queue.pop_front();
			}
			return p;
		}
		bool done() const
		{
			std::unique_lock<std::mutex> lk(this->lock);
			return (this->started == this->finished) && this->queue.empty();
		}
		void wait() const
		{
			std::unique_lock<std::mutex> lk(this->lock);
			if(this->started != this->finished) {
				if(this->queue.empty()) {
					this->cond.wait(lk);
				}
			}
		}
	};

	class FeatureMap
	{
	private:
		std::map<std::string, std::string> features;
		mutable std::mutex lock;

	public:
		/** Set a feature to a specific value
		 *  Note that the default is to not set already-set features to new values
		 *  pass override=true to ignore this safety
		 *  lua: feature('magic-support','yes',true)
		 */
		void setFeature(std::string key, std::string value, bool override = false)
		{
			std::unique_lock<std::mutex> lk(this->lock);
			// look for this key
			if(features.find(key) != features.end()) {
				if(override) {
					// only over-write the value if we are explicitly told to
					features[key] = value;
				}
			} else {
				features.insert(std::pair<std::string, std::string>(key, value));
			}
		}
		/** Set a feature using a key=value string
		 *  This is used to handle the command line input of feature/value settings
		 */
		bool setFeature(const std::string &kv)
		{
			auto pos = kv.find('=');
			if(pos == std::string::npos) {
				error("Features must be described as feature=value");
				return false;
			}

			std::string key = kv.substr(0, pos);
			std::string value = kv.substr(pos + 1);

			this->setFeature(key, value, true);
			return true;
		}
		/** Get the value of a specific feature
		 *  lua: feature('magic-support')
		 */
		std::string getFeature(const std::string &key) const
		{
			std::unique_lock<std::mutex> lk(this->lock);
			if(features.find(key) != features.end()) {
				return features.at(key);
			}
			throw NoKeyException();
		}
		/** Print all feature/values to the console
		 */
		void printFeatureValues() const
		{
			std::unique_lock<std::mutex> lk(this->lock);
			std::cout << std::endl << "----BEGIN FEATURE VALUES----" << std::endl;
			for(auto &feature : this->features) {
				std::cout << feature.first << "\t" << feature.second << std::endl;
			}
			std::cout << "----END FEATURE VALUES----" << std::endl;
		}
	};

	//! The world, everything that everything needs to access
	class World
	{
	private:
		FeatureMap features;
		string_list forcedDeps;
		std::list<NameSpace> namespaces;
		std::list<DLObject> dlobjects;
		Package *p{nullptr};
		std::list<std::string> overlays;
		Internal_Graph graph;
		Internal_Graph topo_graph;
		std::string fetch_from;
		std::string tarball_cache;
		std::string pwd;
		string_list ignoredFeatures;
		bool failed{false};
		bool cleaning{false};
		bool parseOnly{false};
		bool keepGoing{false};
		mutable std::mutex cond_lock;
		mutable std::condition_variable cond;
		std::atomic<int> threads_running{0};
		int threads_limit{0};
		bool outputPrefix{true};

		mutable std::mutex dlobjects_lock;
		const DLObject *_findDLObject(const std::string &);

	public:
		World()
		{
			overlays.emplace_back(std::string("."));
			char *_pwd = getcwd(nullptr, 0);
			this->pwd = std::string(_pwd);
			free(_pwd); // NOLINT
		};

		/** Are we operating in 'forced' mode
		 *  If more than 1 parameter was passed on the command line,
		 *  we are operating in forced mode.
		 *  This means that we ignore any detection of what needs building,
		 *  and build only a specific set of packages (all the arguments, except the first
		 * one)
		 */
		bool forcedMode() const
		{
			return !forcedDeps.empty();
		};
		/** Add a package to 'forced' mode
		 *  This will automatically turn on forced mode
		 */
		void forceBuild(const std::string &name)
		{
			forcedDeps.push_back(name);
		};
		//! Check if a specific package is being forced
		bool isForced(const std::string &name) const
		{
			return (std::find(this->forcedDeps.begin(), this->forcedDeps.end(), name) !=
			        this->forcedDeps.end());
		}
		/** Are we operating in 'cleaning' mode
		 *  If --clean is parsed as a parameter, we run in cleaning mode
		 *  This will make any package that would have been built
		 *  clean out its working directory instead
		 *  (Ignoring dependency scanning, but obeying forced mode)
		 */
		bool areCleaning() const
		{
			return this->cleaning;
		}
		//! Set cleaning mode
		void setCleaning()
		{
			this->cleaning = true;
		}

		/** Are we operating in 'parse only' mode
		 *  If --parse-only is parsed as a parameter, we run in 'parse-only' mode
		 *  This will make buildsys stop after parsing all packages (package filtering rules
		 * have no impact)
		 */
		bool areParseOnly() const
		{
			return this->parseOnly;
		}
		//! Set parase only mode
		void setParseOnly()
		{
			this->parseOnly = true;
		}

		/** Are we operating in 'keep going' mode
		 *  If --keep-going is parsed as a parameter, we run in 'keep-going' mode
		 *  This will make buildsys finish any packages it is currently building before
		 * exiting
		 * if there is a fault
		 */
		bool areKeepGoing() const
		{
			return this->keepGoing;
		}
		//! Set keep only mode
		void setKeepGoing()
		{
			this->keepGoing = true;
		}

		/** Are we expected to output the package name as a prefix
		 *  If --no-output-prefix is parsed as a parameter, we don't prefix package output.
		 *  This will make it so that menuconfig doesn't look horrible.
		 *  At the expense of making it much harder to debug when the build breaks.
		 */
		bool areOutputPrefix() const
		{
			return this->outputPrefix;
		}
		//! clear output prefix mode
		void clearOutputPrefix()
		{
			this->outputPrefix = false;
		}
		//! Ignore a feature for build.info
		void ignoreFeature(const std::string &feature)
		{
			this->ignoredFeatures.push_back(feature);
		}
		//! Is a feature ignored
		bool isIgnoredFeature(const std::string &feature)
		{
			return (std::find(this->ignoredFeatures.begin(), this->ignoredFeatures.end(),
			                  feature) != this->ignoredFeatures.end());
		}
		//! Is the ignore list empty ?
		bool noIgnoredFeatures() const
		{
			return this->ignoredFeatures.empty();
		}

		//! Find (or create) a DLObject for a given full file name
		const DLObject *findDLObject(const std::string &fname)
		{
			std::unique_lock<std::mutex> lk(this->dlobjects_lock);
			return this->_findDLObject(fname);
		}

		//! Start the processing and building steps with the given meta package
		bool basePackage(const std::string &filename);

		//! Get the namespace list
		const std::list<NameSpace> &getNameSpaces() const
		{
			return this->namespaces;
		}

		//! Find (or create) a namespace
		NameSpace *findNameSpace(const std::string &name);

		//! Tell everything that we have failed
		void setFailed()
		{
			this->failed = true;
		};
		//! Test if we have failed
		bool isFailed() const
		{
			return this->failed;
		}
		//! Declare a package built
		bool packageFinished(Package *_p);

		//! Allow the fetch from location to be set
		void setFetchFrom(std::string from)
		{
			this->fetch_from = std::move(from);
		}
		//! Test if the fetch from location is set
		bool canFetchFrom() const
		{
			return (!this->fetch_from.empty());
		}
		//! Test if the fetch from location is set
		const std::string &fetchFrom() const
		{
			return this->fetch_from;
		}

		//! Set the location of the local tarball cache
		void setTarballCache(std::string cache)
		{
			this->tarball_cache = std::move(cache);
		}
		//! Test if the tarball cache location is set
		bool haveTarballCache() const
		{
			return (!this->tarball_cache.empty());
		}
		//! Get the the tarball Cache location
		const std::string &tarballCache() const
		{
			return this->tarball_cache;
		}

		//! Get 'pwd'
		const std::string &getWorkingDir() const
		{
			return this->pwd;
		}

		//! Add an overlay to search for packages
		void addOverlayPath(const std::string &path)
		{
			this->overlays.push_back(path);
		};
		//! Get the overlay list
		const std::list<std::string> &getOverlays() const
		{
			return this->overlays;
		}
		//! A thread has started
		void threadStarted()
		{
			this->threads_running++;
		}
		//! A thread has finished
		void threadEnded()
		{
			std::unique_lock<std::mutex> lk(this->cond_lock);
			this->threads_running--;
			this->cond.notify_all();
		};
		//! How many threads are currently running ?
		int threadsRunning() const
		{
			return this->threads_running;
		};
		//! Adjust the thread limit
		void setThreadsLimit(int tl)
		{
			this->threads_limit = tl;
		};
		//! Get the thread limit value
		int getThreadsLimit() const
		{
			return this->threads_limit;
		}
		//! output the dependency graph
		bool output_graph()
		{
			graph.output();
			return true;
		};
		FeatureMap *featureMap()
		{
			return &this->features;
		}
		void printNameSpaces() const;
	};
} // namespace buildsys

using namespace buildsys;

#endif // INCLUDE_BUILDSYS_H_
