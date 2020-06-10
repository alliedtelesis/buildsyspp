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

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'keepstaging' function usage", "")
{
	std::string file_path = this->cwd + "/test_lua.lua";
	std::ofstream test_file;
	test_file.open(file_path);

	test_file << "keepstaging()";
	test_file.close();

	NameSpace *ns = NameSpace::findNameSpace("test_namespace");
	Package p(ns, "test_package", file_path, file_path);

	// Should default to false
	REQUIRE(!p.getSuppressRemoveStaging());

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
	// Should now be true
	REQUIRE(p.getSuppressRemoveStaging());
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test invalid 'keepstaging' function usage", "")
{
	std::string file_path = this->cwd + "/test_lua.lua";
	std::ofstream test_file;
	test_file.open(file_path);

	test_file << "keepstaging(true)";
	test_file.close();

	NameSpace *ns = NameSpace::findNameSpace("test_namespace");
	Package p(ns, "test_package", file_path, file_path);

	// Should default to false
	REQUIRE(!p.getSuppressRemoveStaging());

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
	// Should still be false
	REQUIRE(!p.getSuppressRemoveStaging());
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'intercept' function usage", "")
{
	std::string file_path = this->cwd + "/test_lua.lua";
	std::ofstream test_file;
	test_file.open(file_path);

	test_file << "intercept()";
	test_file.close();

	NameSpace *ns = NameSpace::findNameSpace("test_namespace");
	Package p(ns, "test_package", file_path, file_path);

	// Should default to false
	REQUIRE(!p.getIntercept());

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
	// Should now be true
	REQUIRE(p.getIntercept());
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test invalid 'intercept' function usage", "")
{
	std::string file_path = this->cwd + "/test_lua.lua";
	std::ofstream test_file;
	test_file.open(file_path);

	test_file << "intercept(true)";
	test_file.close();

	NameSpace *ns = NameSpace::findNameSpace("test_namespace");
	Package p(ns, "test_package", file_path, file_path);

	// Should default to false
	REQUIRE(!p.getIntercept());

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
	// Should still be false
	REQUIRE(!p.getIntercept());
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'hashoutput' function usage", "")
{
	std::string file_path = this->cwd + "/test_lua.lua";
	std::ofstream test_file;
	test_file.open(file_path);

	test_file << "hashoutput()";
	test_file.close();

	NameSpace *ns = NameSpace::findNameSpace("test_namespace");
	Package p(ns, "test_package", file_path, file_path);

	// Should default to false
	REQUIRE(!p.isHashingOutput());

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
	// Should now be true
	REQUIRE(p.isHashingOutput());
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test invalid 'hashoutput' function usage", "")
{
	std::string file_path = this->cwd + "/test_lua.lua";
	std::ofstream test_file;
	test_file.open(file_path);

	test_file << "hashoutput(true)";
	test_file.close();

	NameSpace *ns = NameSpace::findNameSpace("test_namespace");
	Package p(ns, "test_package", file_path, file_path);

	// Should default to false
	REQUIRE(!p.isHashingOutput());

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
	// Should still be false
	REQUIRE(!p.isHashingOutput());
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'builddir' function usage (no parameter)", "")
{
	std::string file_path = this->cwd + "/test_lua.lua";
	std::ofstream test_file;
	test_file.open(file_path);

	// Call 'builddir()' and then test we can call a related builddir
	// function/variable that should now be available.
	test_file << "bd = builddir()\n";
	test_file << "test = bd.path";
	test_file.close();

	NameSpace *ns = NameSpace::findNameSpace("test_namespace");
	Package p(ns, "test_package", file_path, file_path);

	// Should default to false
	REQUIRE(!p.get_clean_before_build());

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
	// Should still be false
	REQUIRE(!p.get_clean_before_build());
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'builddir' function usage (one boolean parameter)", "")
{
	std::string file_path = this->cwd + "/test_lua.lua";
	std::ofstream test_file;
	test_file.open(file_path);

	// Call 'builddir()' and then test we can call a related builddir
	// function/variable that should now be available.
	test_file << "bd = builddir(true)\n";
	test_file << "test = bd.path";
	test_file.close();

	NameSpace *ns = NameSpace::findNameSpace("test_namespace");
	Package p(ns, "test_package", file_path, file_path);

	// Should default to false
	REQUIRE(!p.get_clean_before_build());

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
	// Should now be true
	REQUIRE(p.get_clean_before_build());
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test invalid 'builddir' function usage (two boolean parameters)", "")
{
	std::string file_path = this->cwd + "/test_lua.lua";
	std::ofstream test_file;
	test_file.open(file_path);

	// Call 'builddir()' and then test we can call a related builddir
	// function/variable that should now be available.
	test_file << "bd = builddir(true, true)\n";
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

TEST_CASE_METHOD(TopLevelTestsFixture, "Test invalid 'builddir' function usage (single string parameter)", "")
{
	std::string file_path = this->cwd + "/test_lua.lua";
	std::ofstream test_file;
	test_file.open(file_path);

	// Call 'builddir()' and then test we can call a related builddir
	// function/variable that should now be available.
	test_file << "bd = builddir('test')\n";
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
