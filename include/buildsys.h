#include <iostream>
#include <map>
#include <list>

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


#define error(M) \
	do {\
		fprintf(stderr, "%s:%s():%i: %s\n", __FILE__, __FUNCTION__ , __LINE__, M.c_str()); \
		exit(-1); \
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

extern "C" {
	int run(char *program, char *argv[], const char *path, char *newenvp[]);
};

namespace buildsys {
	typedef std::map<std::string, std::string> key_value;
	typedef std::list<std::string> string_list;

	class Exception {
		public:
			virtual std::string error_msg() = 0;
	};
	
	class CustomException : public Exception {
		private:
			std::string errmsg;
		public:
			CustomException(std::string err) : errmsg(err) {};
			virtual std::string error_msg()
			{
				return errmsg;
			}
	};

	class LuaException : public Exception {
		public:
			virtual std::string error_msg()
			{
				return "Lua Error";
			}
	};
	
	class MemoryException : public Exception {
		public:
			virtual std::string error_msg()
			{
				return "Memory Error";
			}
	};
	
	class NoKeyException : public Exception {
		public:
			virtual std::string error_msg()
			{
				return "Key does not exist";
			}
	};
	
	class DirException : public Exception {
		private:
			std::string errmsg;
		public:
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
	
	class Lua {
		private:
			lua_State *state;
		public:
			Lua() {
				state = luaL_newstate();
				if(state == NULL) throw LuaException();
				luaL_openlibs(state);
			};
			void processFile(const char *filename) {

				int res = luaL_dofile(state, filename);
	
				if(res != 0)
				{
					throw CustomException(lua_tostring(state, -1));
				}
			}
			void registerFunc(std::string name, lua_CFunction fn)
			{
				lua_register(state, name.c_str(), fn);
			}
			void setGlobal(std::string name, void *data)
			{
				lua_pushlightuserdata(state, data);
				lua_setglobal(state, name.c_str());
			}
	};
	
	class Dir {
		private:
		public:
			Dir() {};
			static void lua_table_r(lua_State *L) { LUA_SET_TABLE_TYPE(L,Dir) }
			virtual void lua_table(lua_State *L) { lua_table_r(L); };
	};
	
	class BuildDir : public Dir {
		private:
			typedef Dir super;
			std::string path;
			std::string staging;
			std::string new_staging;
			std::string new_install;
		public:
			BuildDir(std::string pname);
			
			const char *getPath() { return this->path.c_str(); };
			const char *getStaging() { return this->staging.c_str(); };
			const char *getNewStaging() { return this->new_staging.c_str(); };
			const char *getNewInstall() { return this->new_install.c_str(); };

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
	
	class PackageCmd {
		private:
			char *path;
			char *app;
			char **args;
			size_t arg_count;
			char **envp;
			size_t envp_count;
		public:
			PackageCmd(const char *path, const char *app) : path(strdup(path)) , app(strdup(app)) , args(NULL), arg_count(0), envp(NULL), envp_count(0) {};
			void addArg(const char *arg)
			{
				this->arg_count++;
				this->args = (char **)realloc(this->args, sizeof(char *) * (this->arg_count + 1));
				this->args[this->arg_count-1] = strdup(arg);
				this->args[this->arg_count] = NULL;
			}
			void addEnv(const char *env)
			{
				this->envp_count++;
				this->envp = (char **)realloc(this->envp, sizeof(char *) * (this->envp_count + 1));
				this->envp[this->envp_count-1] = strdup(env);
				this->envp[this->envp_count] = NULL;
			}
			bool Run();
	};
	
	class Package {
		private:
			std::list<Package *> depends;
			std::list<PackageCmd *> commands;
			std::string name;
			std::string file;
			BuildDir *bd;
			bool intercept;
			char *depsExtraction;
			char *installFile;
			bool visiting;
			bool processed;
			bool built;
		protected:
			bool extract_staging(const char *dir, std::list<std::string> *done);
			bool extract_install(const char *dir, std::list<std::string> *done);
		public:
			Package(std::string name, std::string file) : name(name), file(file) , bd(NULL), intercept(false), depsExtraction(NULL), installFile(NULL), visiting(false), processed(false), built(false) {};
			BuildDir *builddir();
			void setIntercept() { this->intercept = true; };
			std::string getName() { return this->name; };
			void depend(Package *p) { this->depends.push_back(p); };
			void setDepsExtract(char *de) { this->depsExtraction = de; };
			void addCommand(PackageCmd *pc) { this->commands.push_back(pc); };
			void setInstallFile(char *i) { this->installFile = i; };
			bool process();
			bool build();

			std::list<Package *>::iterator dependsStart();
			std::list<Package *>::iterator dependsEnd();
			
			void printLabel(std::ostream& out);
	};

	class World {
		private:
			key_value *features;
			string_list *forcedDeps;
			Lua *lua;
			std::string name;
			Package *p;
			std::list<Package *> packages;
		public:
			World() : features(new key_value()), forcedDeps(new string_list()),
					lua(new Lua())  {};

			Lua *getLua() { return this->lua; };

			std::string getName() { return this->name; };
			void setName(std::string n) { this->name = n; };

			bool forcedMode() {return !forcedDeps->empty(); };
			void forceBuild(std::string name) { forcedDeps->push_back(name); };
			bool isForced(std::string name);
			void setFeature(std::string key, std::string value, bool override=false);
			void setFeature(char *kv);
			std::string getFeature(std::string key);
			
			bool basePackage(char *filename);
			Package *findPackage(std::string name, std::string file);

			std::list<Package *>::iterator packagesStart();
			std::list<Package *>::iterator packagesEnd();
	};
	
	void interfaceSetup(Lua *lua);
	
	extern World *WORLD;
	void graph();
};

using namespace buildsys;
