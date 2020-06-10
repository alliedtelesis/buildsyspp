#define CATCH_CONFIG_MAIN

#include "include/buildsys.h"
#include "interface/luainterface.h"

#include <catch2/catch.hpp>

using namespace buildsys;

class TopLevelTestsFixture
{
protected:
	std::string cwd{"lua_test_dir"};

public:
	TopLevelTestsFixture()
	{
		filesystem::create_directories(this->cwd);
	}
	~TopLevelTestsFixture()
	{
		filesystem::remove_all(this->cwd);
	}
};

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'name' function usage", "")
{
	std::string file_path = this->cwd + "/test_lua.lua";
	std::ofstream test_file;
	test_file.open(file_path);

	// Call the name() function and check the result
	test_file << "assert(name() == 'test_namespace')";
	test_file.close();

	NameSpace *ns = NameSpace::findNameSpace("test_namespace");
	Package p(ns, "test_package", file_path, file_path);

	Lua lua;
	interfaceSetup(&lua);
	li_set_package(&p);

	bool exception_caught = false;
	try {
		lua.processFile(file_path);
	} catch(CustomException &e) {
		exception_caught = true;
	}

	REQUIRE(!exception_caught);
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test invalid 'name' function usage", "")
{
	std::string file_path = this->cwd + "/test_lua.lua";
	std::ofstream test_file;
	test_file.open(file_path);

	// Call the name() function with more than zero arguments and
	// check that it fails.
	test_file << "assert(name('blah') == 'test_namespace')";
	test_file.close();

	NameSpace *ns = NameSpace::findNameSpace("test_namespace");
	Package p(ns, "test_package", file_path, file_path);

	Lua lua;
	interfaceSetup(&lua);
	li_set_package(&p);

	bool exception_caught = false;
	try {
		lua.processFile(file_path);
	} catch(CustomException &e) {
		exception_caught = true;
	}

	REQUIRE(exception_caught);
}
