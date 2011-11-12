#include <iostream>
#include <map>
#include <list>

extern "C" {
	#include <lua.h>
	#include <lualib.h>
	#include <lauxlib.h>
};

#ifdef UNDERSCORE
extern "C" {
	#include <us_datacoding.h>	
	#include <us_client.h>
	#include <us_fifo.h>
};
#endif

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

typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS> Graph;
typedef boost::graph_traits < Graph >::vertex_descriptor Vertex;
typedef boost::graph_traits < Graph >::edge_descriptor Edge;

#define error(M) \
	do {\
		log((char *)"BuildSys","%s:%s():%i: %s", __FILE__, __FUNCTION__ , __LINE__, M); \
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
	int li_bd_fetch(lua_State *L);
	int li_bd_extract(lua_State *L);
	int li_bd_patch(lua_State *L);
	int li_bd_cmd(lua_State *L);
	int li_bd_install(lua_State *L);
};

namespace buildsys {
	typedef std::map<std::string, std::string> key_value;
	typedef std::list<std::string> string_list;
	
	class Package;

	/*! The catchable exception */
	class Exception {
		public:
			//! Return the error message for this exception 
			virtual std::string error_msg() = 0;
	};
	
	/*! An exception with a custom error message */
	class CustomException : public Exception {
		private:
			std::string errmsg;
		public:
			//! Construct an exception with a specific error message
			/** \param err The erorr message
			  */
			CustomException(std::string err) : errmsg(err) {};
			virtual std::string error_msg()
			{
				return errmsg;
			}
	};

	/*! A Lua fault */
	class LuaException : public Exception {
		public:
			virtual std::string error_msg()
			{
				return "Lua Error";
			}
	};
	
	/*! A Memory fault */
	class MemoryException : public Exception {
		public:
			virtual std::string error_msg()
			{
				return "Memory Error";
			}
	};
	
	/*! No Such Key fault */
	class NoKeyException : public Exception {
		public:
			virtual std::string error_msg()
			{
				return "Key does not exist";
			}
	};
	
	/*! Error using/creating directory */
	class DirException : public Exception {
		private:
			std::string errmsg;
		public:
			//! Create a directory exception
			/** \param location The directory path
			  * \param err The error message
			  */
			DirException(char *location, char *err)
			{
				char *em = NULL;
				asprintf(&em, "Error with directory '%s': %s", location, err);
				errmsg = std::string(em);
				free(em);
			}
			virtual std::string error_msg()
			{
				return errmsg;
			}
	};
	
	/*! A C++ wrapper for a lua state (instance) */
	class Lua {
		private:
			lua_State *state;
		public:
			//! Create a lua state
			/** Creates a new lua state, and opens loads our libraries into it
			  */
			Lua() {
				state = luaL_newstate();
				if(state == NULL) throw LuaException();
				luaL_openlibs(state);
			};
			//! Load and exexcute a lua file in this instance
			/** \param filename The name of the lua file to load and run
			  */
			void processFile(const char *filename) {

				int res = luaL_dofile(state, filename);
	
				if(res != 0)
				{
					throw CustomException(lua_tostring(state, -1));
				}
			}
			//! Register a function in this lua instance
			/** \param name The name of the function
			  * \param fn The function to call
			  */
			void registerFunc(std::string name, lua_CFunction fn)
			{
				lua_register(state, name.c_str(), fn);
			}
			//! Set a global variable to given data
			/** \param name the name to store this data as
			  * \param data the data
			  */
			void setGlobal(std::string name, void *data)
			{
				lua_pushlightuserdata(state, data);
				lua_setglobal(state, name.c_str());
			}
	};
	
	//! A directory to perform actions on
	class Dir {
		private:
		public:
			//! Create a directory
			Dir() {};
			//! Set this as a Dir for lua
			static void lua_table_r(lua_State *L) { LUA_SET_TABLE_TYPE(L,Dir) }
			//! Register as a lua table for return from a function
			virtual void lua_table(lua_State *L) { lua_table_r(L); };
	};
	
	//! A directory for building a package in
	/** Each package has one build directory which is used to run all the commands in
	  */
	class BuildDir : public Dir {
		private:
			typedef Dir super;
			std::string path;
			std::string staging;
			std::string new_staging;
			std::string new_install;
		public:
			//! Create a build directory
			/** \param pname The package name
			  * \param clean If true, this directory will be emptied before creation
			  */
			BuildDir(std::string pname, bool clean);
			//! Return the full path to this directory
			const char *getPath() { return this->path.c_str(); };
			//! Return the full path to the staging directory
			const char *getStaging() { return this->staging.c_str(); };
			//! Return the full path to the new staging directory
			const char *getNewStaging() { return this->new_staging.c_str(); };
			//! Return the full path to the new install directory
			const char *getNewInstall() { return this->new_install.c_str(); };

			//! Remove all the contents of this directory
			void clean();

			static void lua_table_r(lua_State *L) { LUA_SET_TABLE_TYPE(L,BuildDir)
						LUA_ADD_TABLE_FUNC(L, "fetch", li_bd_fetch);
						LUA_ADD_TABLE_FUNC(L, "extract", li_bd_extract);
						LUA_ADD_TABLE_FUNC(L, "cmd", li_bd_cmd);
						LUA_ADD_TABLE_FUNC(L, "patch", li_bd_patch);
						LUA_ADD_TABLE_FUNC(L, "install", li_bd_install);
						super::lua_table_r(L); }
			virtual void lua_table(lua_State *L) { lua_table_r(L); 
					LUA_ADD_TABLE_STRING(L, "path", path.c_str());
					LUA_ADD_TABLE_STRING(L, "staging", staging.c_str());
					LUA_ADD_TABLE_STRING(L, "new_staging", new_staging.c_str());
					LUA_ADD_TABLE_STRING(L, "new_install", new_install.c_str());
				};
	};
	
	//! A command to run as part of a packages build step
	/** Stores the enviroment and arguements for a given command
	  * Used to hold commands betwen the config loading step and package build steps.
	  */
	class PackageCmd {
		private:
			char *path;
			char *app;
			char **args;
			size_t arg_count;
			char **envp;
			size_t envp_count;
		public:
			//! Create a Package Command
			/** \param path The path to run this command in
			  * \param app The program to invoke
			  */
			PackageCmd(const char *path, const char *app) : path(strdup(path)) , app(strdup(app)) , args(NULL), arg_count(0), envp(NULL), envp_count(0) {};
			//! Add an argument to this command
			/** \param arg The argument to append to this command
			  */
			void addArg(const char *arg)
			{
				this->arg_count++;
				this->args = (char **)realloc(this->args, sizeof(char *) * (this->arg_count + 1));
				this->args[this->arg_count-1] = strdup(arg);
				this->args[this->arg_count] = NULL;
			}
			//! Add an enviroment variable to this command
			/** \param env The enviroment variable to append to this command
			  */
			void addEnv(const char *env)
			{
				this->envp_count++;
				this->envp = (char **)realloc(this->envp, sizeof(char *) * (this->envp_count + 1));
				this->envp[this->envp_count-1] = strdup(env);
				this->envp[this->envp_count] = NULL;
			}
			//! Run this command
			/** \param package The package name to use in the command logging
			 */
			bool Run(const char *package);
	};
	
	//! An extraction unit
	/** Describes a single step required to re-extract a package
	  */
	class ExtractionUnit {
		protected:
			std::string uri; //!< URI of this unit
			std::string hash; //!< Hash of this unit
		public:
			ExtractionUnit() : uri(std::string()), hash(std::string()) {};
			virtual bool same(ExtractionUnit *eu) = 0;
			virtual bool print(std::ostream& out) = 0;
			virtual std::string type() = 0;
			virtual bool extract(Package *P, BuildDir *b) = 0;
			virtual std::string URI() { return this->uri; };
			virtual std::string HASH() { return this->hash; };
	};
	
	//! A tar extraction unit
	class TarExtractionUnit : public ExtractionUnit {
		public:
			//! Create an extraction unit for a tar file
			TarExtractionUnit(const char *fName);
			virtual bool same(ExtractionUnit *eu)
			{
				if(eu->type().compare("TarFile") != 0) return false;
				if(eu->URI().compare(this->uri) != 0) return false;
				if(eu->HASH().compare(this->hash) != 0) return false;
				return true;
			}
			virtual bool print(std::ostream& out)
			{
				out << this->type() << " " << this->uri << " " << this->hash << std::endl;
				return true;
			}
			virtual std::string type()
			{
				return std::string("TarFile");
			}
			virtual bool extract(Package *P, BuildDir *b);
	};
	
	//! A patch file as part of the extraction step
	class PatchExtractionUnit : public ExtractionUnit {
		private:
			int level;
			char *patch_path;
		public:
			PatchExtractionUnit(int level, char *patch_path, char *patch);
			virtual bool same(ExtractionUnit *eu)
			{
				if(eu->type().compare("PatchFile") != 0) return false;
				if(eu->URI().compare(this->uri) != 0) return false;
				if(eu->HASH().compare(this->hash) != 0) return false;
				return true;
			}
			virtual bool print(std::ostream& out)
			{
				out << this->type() << " " << this->level << " " << this->patch_path << " " << this->uri << " " << this->hash << std::endl;
				return true;
			}
			virtual std::string type()
			{
				return std::string("PatchFile");
			}
			virtual bool extract(Package *P, BuildDir *b);
	};
	//! An extraction description
	/** Describes all the steps and files required to re-extract a package
	  * Used for checking a package needs extracting again
	  */
	class Extraction {
		private:
			ExtractionUnit **EUs;
			size_t EU_count;
		public:
			Extraction() : EUs(NULL), EU_count(0) {};
			bool add(ExtractionUnit *eu);
			bool print(std::ostream& out)
			{
				for(size_t i = 0; i < this->EU_count; i++)
				{
					if(!EUs[i]->print(out)) return false;
				}
				return true;
			}
			bool extract(Package *P, BuildDir *d)
			{
				for(size_t i = 0; i < this->EU_count; i++)
				{
					if(!EUs[i]->extract(P,d)) return false;
				}
				return true;
			};
	};
	
	//! A package to build
	class Package {
		private:
			std::list<Package *> depends;
			std::list<PackageCmd *> commands;
			std::string name;
			std::string file;
			BuildDir *bd;
			Extraction *Extract;
			bool intercept;
			char *depsExtraction;
			char *installFile;
			bool visiting;
			bool processed;
			bool built;
			bool building;
			bool extracted;
			bool codeUpdated;
#ifdef UNDERSCORE
			us_mutex *lock;
#endif
			time_t run_secs;
		protected:
			//! Extract the new staging directory this package created in the given path
			bool extract_staging(const char *dir, std::list<std::string> *done);
			//! Extract the new install directory this package created in the given path
			bool extract_install(const char *dir, std::list<std::string> *done);
		public:
			//! Create a package
			/** Constucts a package with a given name, based on a specific file
			  * \param name The name of this package
			  * \param file The lua file describing this package
			  */
			Package(std::string name, std::string file) : name(name), file(file) , bd(NULL), Extract(new Extraction()), intercept(false), depsExtraction(NULL), installFile(NULL), visiting(false), processed(false), built(false), building(false), extracted(false), codeUpdated(false),
#ifdef UNDERSCORE
			lock(us_mutex_create(true)),
#endif
			 run_secs(0) {};
			//! Returns the build directory being used by this package
			BuildDir *builddir();
			//! Returns the extraction
			Extraction *extraction() { return this->Extract; };
			//! Convert this package to the intercepting type
			/** Intercepting packages stop the extract install method from recursing past them.
			  */
			void setIntercept() { this->intercept = true; };
			//! Return the name of this package
			std::string getName() { return this->name; };
			//! Depend on another package
			/** \param p The package to depend on
			  */
			void depend(Package *p) { this->depends.push_back(p); };
			//! Set the location to extract install directories to
			/** During the build, all files that all dependencies have installed
			  * will be extracted to the given path
			  * \param de relative path to extract dependencies to
			  */
			void setDepsExtract(char *de) { this->depsExtraction = de; };
			//! Add a command to run during the build stage
			/** \param pc The comamnd to run
			  */
			void addCommand(PackageCmd *pc) { this->commands.push_back(pc); };
			//! Set the file to install
			/** Setting this overrides to standard zipping up of the entire new install directory
			  * and just installs this specific file 
			  * \param i the file to install
			  */
			void setInstallFile(char *i) { this->installFile = i; };
			//! Parse and load the lua file for this package
			bool process();
			//! Extract all the sources required for this package
			bool extract();
			//! Sets the code updated flag
			void setCodeUpdated() { this->codeUpdated = true; };
			//! Is this package ready for building yet ?
			/** \return true iff all all dependencies are built
			  */
			bool canBuild();
			//! Is this package already built ?
			/** \return true if this package has already been built during this invocation of buildsys
			  */
			bool isBuilt();
			//! Build this package
			bool build();
			//! Has building of this package already started ?
			bool isBuilding();
			//! Tell this package it is now building
			void setBuilding();

			//! Get the start iterator for the dependencies list
			std::list<Package *>::iterator dependsStart();
			//! Get the end iterator for the dependencies list
			std::list<Package *>::iterator dependsEnd();
			
			//! Print the label for use on the graph
			/** Prints the package name, number of commands to run, and time spent building
			  */
			void printLabel(std::ostream& out);
	};
	
	//! A graph of dependencies between packages
	class Internal_Graph {
		private:
			typedef std::map<Package *, Vertex > NodeVertexMap;
			typedef std::map<Vertex, Package * > VertexNodeMap;
			typedef std::vector< Vertex > container;
			Graph g;
			NodeVertexMap *Nodes;
			VertexNodeMap *NodeMap;
			container *c;
		public:
			//! Create an Internal_Graph
			Internal_Graph();
			//! Output the graph to dependencies.dot
			void output();
			//! Perform a topological sort
			void topological();
			//! Find a package with no dependencies
			Package *topoNext();
			//! Remove a package from this graph
			void deleteNode(Package *p);

	};

	//! The world, everything that everything needs to access
	class World {
		private:
			key_value *features;
			string_list *forcedDeps;
			Lua *lua;
			std::string name;
			Package *p;
			std::list<Package *> packages;
			Internal_Graph *graph;
			Internal_Graph *topo_graph;
			bool failed;
#ifdef UNDERSCORE_MONITOR
			us_event_set *es;
#endif
#ifdef UNDERSCORE
			us_condition *cond;
#endif
		public:
			World() : features(new key_value()), forcedDeps(new string_list()),
					lua(new Lua()), graph(NULL), failed(false)
#ifdef UNDERSCORE
					,cond(us_cond_create()) 
#endif
					{};

			//! Return the lua instance being used
			Lua *getLua() { return this->lua; };

			//! Return the global name, in lua this is name()
			std::string getName() { return this->name; };
			//! Set the global name, in lua this is name('somevalue')
			void setName(std::string n);

			//! Are we operating in 'forced' mode
			/** If more than 1 parameter was passed on the command line,
			  * we are operating in forced mode.
			  * This means that we ignore any detection of what needs building,
			  * and build only a specific set of packages (all the arguments, except the first one)
			  */
			bool forcedMode() {return !forcedDeps->empty(); };
			//! Add a package to 'forced' mode
			/** This will automatically turn on forced mode
			  */
			void forceBuild(std::string name) { forcedDeps->push_back(name); };
			//! Check if a specific package is being forced
			bool isForced(std::string name);
			//! Set a feature to a specific value
			/** Note that the default is to not set already-set features to new values
			  * pass override=true to ignore this safety
			  * lua: feature('magic-support','yes',true)
			  */
			bool setFeature(std::string key, std::string value, bool override=false);
			//! Set a feature using a key=value string
			/** This is used to handle the command line input of feature/value settings
			  */
			bool setFeature(char *kv);
			//! Get the value of a specific feature
			/** lua: feature('magic-support')
			  */
			std::string getFeature(std::string key);
			
			//! Start the processing and building steps with the given meta package
			bool basePackage(char *filename);
			//! Find or create a package with the given name, using the filename if needed
			Package *findPackage(std::string name, std::string file);

			//! Get the start iterator for the package list
			std::list<Package *>::iterator packagesStart();
			//! Get the end iterator for the package list
			std::list<Package *>::iterator packagesEnd();
			
			//! Tell everything that we have failed
			void setFailed() { this->failed = true; };
			//! Test if we have failed
			bool isFailed() { return this->failed; }
			//! Declare a package built
			bool packageFinished(Package *p);

#ifdef UNDERSCORE
			void condTrigger() { us_cond_lock(this->cond); us_cond_signal(this->cond, true); us_cond_unlock(this->cond); };
#endif
			//! output the dependency graph
			bool output_graph() { if(this->graph != NULL) { graph->output(); } ; return true; };

#ifdef UNDERSCORE_MONITOR
			bool setES(us_event_set *es) { this->es = es; return true; };
			us_event_set *getES(void) { return this->es; };
#endif
	};
	
	bool interfaceSetup(Lua *lua);
	
	extern World *WORLD;

	void log(const char *package, const char *fmt, ...);
	void program_output(const char *pacakge, const char *mesg);

	int run(const char *, char *program, char *argv[], const char *path, char *newenvp[]);

#ifdef UNDERSCORE_MONITOR
	void sendTarget(const char *name);
#endif

};

using namespace buildsys;

#ifdef UNDERSCORE
// lm/linux.c
extern unsigned long kb_main_buffers;
extern unsigned long kb_main_cached;
extern unsigned long kb_main_free;
extern unsigned long kb_main_total;
extern unsigned long kb_swap_free;
extern unsigned long kb_swap_total;
void meminfo(void);
void loadavg(double *av1, double *av5, double *av15);

#endif
