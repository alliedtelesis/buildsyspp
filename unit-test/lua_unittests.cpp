#define CATCH_CONFIG_MAIN

#include "exceptions.hpp"
#include "include/filesystem.h"
#include "lua.hpp"

#include <catch2/catch.hpp>

using namespace buildsys;

class LuaTestsFixture
{
protected:
	std::string cwd{"lua_test_dir"};

public:
	LuaTestsFixture()
	{
		filesystem::create_directories(this->cwd);
	}
	~LuaTestsFixture()
	{
		filesystem::remove_all(this->cwd);
	}
};

TEST_CASE_METHOD(LuaTestsFixture, "Test processFile() function with valid lua file", "")
{
	Lua lua;

	std::string file_path = this->cwd + "/test_lua.lua";
	std::ofstream test_file;
	test_file.open(file_path);
	// Create some valid lua code
	test_file << "test = 2 + 2";
	test_file.close();

	bool exception_caught = false;
	try {
		lua.processFile(file_path);
	} catch(CustomException &e) {
		exception_caught = true;
	}

	REQUIRE(!exception_caught);
}

TEST_CASE_METHOD(LuaTestsFixture, "Test processFile() function with invalid lua file", "")
{
	Lua lua;

	std::string file_path = this->cwd + "/test_lua.lua";
	std::ofstream test_file;
	test_file.open(file_path);
	// Create some invalid lua code
	test_file << "test = nil .. nil";
	test_file.close();

	bool exception_caught = false;
	std::string exception_message("");
	try {
		lua.processFile(file_path);
	} catch(CustomException &e) {
		exception_caught = true;
		exception_message = e.what();
	}

	REQUIRE(exception_caught);
	REQUIRE(exception_message ==
	        "lua_test_dir/test_lua.lua:1: attempt to concatenate a nil value");
}

static int test_fn(lua_State *L)
{
	lua_pushstring(L, "test");
	return 1;
}

TEST_CASE_METHOD(LuaTestsFixture, "Test registerFunc() function", "")
{
	Lua lua;

	std::string file_path = this->cwd + "/test_lua.lua";
	std::ofstream test_file;
	test_file.open(file_path);
	// Create some lua code with the function we will register
	test_file << "test = test_fn() .. '2'";
	test_file.close();

	lua.registerFunc("test_fn", test_fn);

	bool exception_caught = false;
	try {
		lua.processFile(file_path);
	} catch(CustomException &e) {
		exception_caught = true;
	}

	REQUIRE(!exception_caught);
}
