#define CATCH_CONFIG_MAIN

#include "include/buildsys.h"
#include "interface/luainterface.h"

#include <catch2/catch.hpp>

using namespace buildsys;

class TopLevelTestsFixture
{
protected:
	std::string cwd{"lua_test_dir"};
	NameSpace *ns{nullptr};

public:
	TopLevelTestsFixture()
	{
		filesystem::create_directories(this->cwd);
		this->ns = NameSpace::findNameSpace("test_namespace");
	}
	~TopLevelTestsFixture()
	{
		filesystem::remove_all(this->cwd);
	}
	/**
	 * Execute some lua code for a test package and namespace.
	 *
	 * @param p - The package to execute for.
	 * @param lua_code - The code to execute.
	 * @returns true if the code executes successfully, false if an exception occurs.
	 */
	bool execute_lua(Package &p, const std::string &lua_code)
	{
		std::string file_path = this->cwd + "/test_lua.lua";
		std::ofstream test_file;
		test_file.open(file_path);

		test_file << lua_code;
		test_file.close();

		Lua lua;
		interfaceSetup(&lua);
		li_set_package(&p);

		bool exception_caught = false;
		try {
			lua.processFile(file_path);
		} catch(CustomException &e) {
			exception_caught = true;
		}

		return !exception_caught;
	}
};

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'name' function usage", "")
{
	Package p(this->ns, "test_package", ".", ".");

	// Call the name() function and check the result
	REQUIRE(execute_lua(p, "assert(name() == 'test_namespace')"));
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test invalid 'name' function usage", "")
{
	Package p(this->ns, "test_package", ".", ".");

	// Call the name() function with more than zero arguments and
	// check that it fails.
	REQUIRE(!execute_lua(p, "assert(name('blah') == 'test_namespace')"));
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'keepstaging' function usage", "")
{
	Package p(this->ns, "test_package", ".", ".");

	// Should default to false
	REQUIRE(!p.getSuppressRemoveStaging());

	REQUIRE(execute_lua(p, "keepstaging()"));

	// Should now be true
	REQUIRE(p.getSuppressRemoveStaging());
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test invalid 'keepstaging' function usage", "")
{
	Package p(this->ns, "test_package", ".", ".");

	// Should default to false
	REQUIRE(!p.getSuppressRemoveStaging());

	REQUIRE(!execute_lua(p, "keepstaging(true)"));

	// Should still be false
	REQUIRE(!p.getSuppressRemoveStaging());
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'intercept' function usage", "")
{
	Package p(this->ns, "test_package", ".", ".");

	// Should default to false
	REQUIRE(!p.getIntercept());

	REQUIRE(execute_lua(p, "intercept()"));

	// Should now be true
	REQUIRE(p.getIntercept());
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test invalid 'intercept' function usage", "")
{
	Package p(this->ns, "test_package", ".", ".");

	// Should default to false
	REQUIRE(!p.getIntercept());

	REQUIRE(!execute_lua(p, "intercept(true)"));

	// Should still be false
	REQUIRE(!p.getIntercept());
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'hashoutput' function usage", "")
{
	Package p(this->ns, "test_package", ".", ".");

	// Should default to false
	REQUIRE(!p.isHashingOutput());

	REQUIRE(execute_lua(p, "hashoutput()"));

	// Should now be true
	REQUIRE(p.isHashingOutput());
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test invalid 'hashoutput' function usage", "")
{
	Package p(this->ns, "test_package", ".", ".");

	// Should default to false
	REQUIRE(!p.isHashingOutput());

	REQUIRE(!execute_lua(p, "hashoutput(true)"));

	// Should still be false
	REQUIRE(!p.isHashingOutput());
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'builddir' function usage (no parameter)", "")
{
	Package p(this->ns, "test_package", ".", ".");

	// Should default to false
	REQUIRE(!p.get_clean_before_build());

	// Call 'builddir()' and then test we can call a related builddir
	// function/variable that should now be available.
	REQUIRE(execute_lua(p, "bd = builddir()\ntest = bd.path"));

	// Should still be false
	REQUIRE(!p.get_clean_before_build());
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'builddir' function usage (one boolean parameter)", "")
{
	Package p(this->ns, "test_package", ".", ".");

	// Should default to false
	REQUIRE(!p.get_clean_before_build());

	// Call 'builddir()' and then test we can call a related builddir
	// function/variable that should now be available.
	REQUIRE(execute_lua(p, "bd = builddir(true)\ntest = bd.path"));

	// Should now be true
	REQUIRE(p.get_clean_before_build());
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test invalid 'builddir' function usage (two boolean parameters)", "")
{
	Package p(this->ns, "test_package", ".", ".");

	REQUIRE(!execute_lua(p, "bd = builddir(true, true)"));
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test invalid 'builddir' function usage (single string parameter)", "")
{
	Package p(this->ns, "test_package", ".", ".");

	REQUIRE(!execute_lua(p, "bd = builddir('test')"));
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'feature' function usage (get package prefixed feature)", "")
{
	Package p(this->ns, "test_package", ".", ".");

	li_get_feature_map()->setFeature("test_package:test_feature1", "value1", true);
	li_get_feature_map()->setFeature("test_feature1", "value2", true);

	REQUIRE(execute_lua(p, "assert(feature('test_feature1') == 'value1')"));

	std::stringstream buffer;
	p.buildDescription()->print(buffer);
	REQUIRE(buffer.str() == "FeatureValue test_feature1 value1\n");
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'feature' function usage (get non package prefixed feature)", "")
{
	Package p(this->ns, "test_package", ".", ".");

	li_get_feature_map()->setFeature("test_feature2", "value3", true);

	REQUIRE(execute_lua(p, "assert(feature('test_feature2') == 'value3')"));

	std::stringstream buffer;
	p.buildDescription()->print(buffer);
	REQUIRE(buffer.str() == "FeatureValue test_feature2 value3\n");
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'feature' function usage (get non existent feature)", "")
{
	Package p(this->ns, "test_package", ".", ".");

	REQUIRE(execute_lua(p, "assert(feature('test_feature3') == nil)"));

	std::stringstream buffer;
	p.buildDescription()->print(buffer);
	REQUIRE(buffer.str() == "FeatureNil test_feature3\n");
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'feature' function usage (set non existent feature)", "")
{
	Package p(this->ns, "test_package", ".", ".");

	REQUIRE(execute_lua(p, "feature('test_feature4', 'value4')"));
	REQUIRE(li_get_feature_map()->getFeature("test_feature4") == "value4");
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'feature' function usage (set existent feature)", "")
{
	Package p(this->ns, "test_package", ".", ".");

	li_get_feature_map()->setFeature("test_feature5", "value10", true);
	REQUIRE(execute_lua(p, "feature('test_feature5', 'value5')"));
	REQUIRE(li_get_feature_map()->getFeature("test_feature5") == "value10");
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test valid 'feature' function usage (force set existent feature)", "")
{
	Package p(this->ns, "test_package", ".", ".");

	li_get_feature_map()->setFeature("test_feature6", "value12", true);
	REQUIRE(execute_lua(p, "feature('test_feature6', 'value6', true)"));
	REQUIRE(li_get_feature_map()->getFeature("test_feature6") == "value6");
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test invalid 'feature' function usage (zero arguments)", "")
{
	Package p(this->ns, "test_package", ".", ".");
	REQUIRE(!execute_lua(p, "feature()"));
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test invalid 'feature' function usage (non string first argument)", "")
{
	Package p(this->ns, "test_package", ".", ".");
	REQUIRE(!execute_lua(p, "feature(true)"));
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test invalid 'feature' function usage (non string second argument)", "")
{
	Package p(this->ns, "test_package", ".", ".");
	REQUIRE(!execute_lua(p, "feature('test_feature7', true)"));
}

TEST_CASE_METHOD(TopLevelTestsFixture, "Test invalid 'feature' function usage (non boolean third argument)", "")
{
	Package p(this->ns, "test_package", ".", ".");
	REQUIRE(!execute_lua(p, "feature('test_feature7', 'value7', 'blah')"));
}
