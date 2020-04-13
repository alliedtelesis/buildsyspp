#define CATCH_CONFIG_MAIN

#include "include/buildsys.h"
#include <catch2/catch.hpp>

class BuildDirTestsFixture
{
protected:
	std::string cwd{"test_dir"};

public:
	BuildDirTestsFixture()
	{
		filesystem::create_directories(this->cwd);
	}
	~BuildDirTestsFixture()
	{
		filesystem::remove_all(this->cwd);
		// BuildDir objects create the "output" directory. In the future this should
		// be moved to the responsibility of World.
		filesystem::remove_all("output");
	}
};

TEST_CASE_METHOD(BuildDirTestsFixture, "Test BuildDir object creation and initialisation",
                 "")
{
	BuildDir test_dir(this->cwd, "test_ns", "test_package");

	REQUIRE(test_dir.getPath() == (this->cwd + "/output/test_ns/test_package/work"));
	REQUIRE(filesystem::exists(test_dir.getPath()));

	REQUIRE(test_dir.getShortPath() == "output/test_ns/test_package/work");

	REQUIRE(test_dir.getStaging() == (this->cwd + "/output/test_ns/test_package/staging"));
	REQUIRE(filesystem::exists(test_dir.getStaging()));

	REQUIRE(test_dir.getNewPath() == (this->cwd + "/output/test_ns/test_package/new"));
	REQUIRE(filesystem::exists(test_dir.getNewPath()));

	REQUIRE(test_dir.getNewStaging() ==
	        (this->cwd + "/output/test_ns/test_package/new/staging"));
	REQUIRE(filesystem::exists(test_dir.getNewStaging()));

	REQUIRE(test_dir.getNewInstall() ==
	        (this->cwd + "/output/test_ns/test_package/new/install"));
	REQUIRE(filesystem::exists(test_dir.getNewInstall()));
}

TEST_CASE_METHOD(BuildDirTestsFixture, "Test BuildDir 'clean' method", "")
{
	BuildDir test_dir(this->cwd, "test_ns", "test_package");
	std::string test_file_path = test_dir.getPath() + "/test_file";

	REQUIRE(test_dir.getPath() == (this->cwd + "/output/test_ns/test_package/work"));
	REQUIRE(filesystem::exists(test_dir.getPath()));
	REQUIRE(!filesystem::exists(test_file_path));

	std::string cmd = "touch " + test_file_path;
	std::system(cmd.c_str());
	REQUIRE(filesystem::exists(test_file_path));

	test_dir.clean();
	REQUIRE(filesystem::exists(test_dir.getPath()));
	REQUIRE(!filesystem::exists(test_file_path));
}

TEST_CASE_METHOD(BuildDirTestsFixture, "Test BuildDir 'cleanStaging' method", "")
{
	BuildDir test_dir(this->cwd, "test_ns", "test_package");

	REQUIRE(filesystem::exists(test_dir.getStaging()));

	test_dir.cleanStaging();
	REQUIRE(!filesystem::exists(test_dir.getStaging()));
}