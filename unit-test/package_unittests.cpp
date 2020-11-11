#define CATCH_CONFIG_MAIN

#include "include/buildsys.h"
#include <catch2/catch.hpp>

using namespace buildsys;

class PackageTestsFixture
{
protected:
	std::string cwd{"package_test_dir"};
	NameSpace *ns{nullptr};

public:
	PackageTestsFixture()
	{
		hash_setup();
		filesystem::create_directories(this->cwd);
		this->ns = NameSpace::findNameSpace("test_namespace");
	}
	~PackageTestsFixture()
	{
		hash_shutdown();
		filesystem::remove_all(this->cwd);
		filesystem::remove_all("package");
		filesystem::remove_all("output");
	}
};

TEST_CASE_METHOD(PackageTestsFixture, "Test setSuppressRemoveStaging method", "")
{
	Package p(this->ns, "test_package", ".", ".");

	REQUIRE(filesystem::exists(p.builddir()->getStaging()));

	REQUIRE(!p.getSuppressRemoveStaging());
	p.setSuppressRemoveStaging(true);
	REQUIRE(p.getSuppressRemoveStaging());

	p.cleanStaging();
	REQUIRE(filesystem::exists(p.builddir()->getStaging()));

	p.setSuppressRemoveStaging(false);
	REQUIRE(!p.getSuppressRemoveStaging());

	p.cleanStaging();
	REQUIRE(!filesystem::exists(p.builddir()->getStaging()));
}

TEST_CASE_METHOD(PackageTestsFixture, "Test relative_fetch_path method (non searched paths)", "")
{
	Package p(this->ns, "test_package", ".", ".");

	std::string test_path1 = "/blah";
	REQUIRE(p.relative_fetch_path(test_path1, true) == test_path1);
	REQUIRE(p.relative_fetch_path(test_path1, false) == test_path1);

	std::string test_path2 = "dl/blah";
	REQUIRE(p.relative_fetch_path(test_path2, true) == test_path2);
	REQUIRE(p.relative_fetch_path(test_path2, false) == test_path2);
}

TEST_CASE_METHOD(PackageTestsFixture, "Test relative_fetch_path method (relative ('.') paths)", "")
{
	Package p(this->ns, "test_package", ".", ".");

	std::string file_path = "./" + this->cwd + "/blah";
	std::ofstream test_file;
	test_file.open(file_path);
	test_file << "blah";
	test_file.close();

	REQUIRE(p.relative_fetch_path(file_path, true) == ("./" + file_path));
	REQUIRE(p.relative_fetch_path(file_path, false) == ("./" + file_path));

	filesystem::remove(file_path);

	bool exception_caught = false;
	try {
		p.relative_fetch_path(file_path, true);
	} catch(FileNotFoundException &e) {
		exception_caught = true;
	}
	REQUIRE(exception_caught);

	exception_caught = false;
	try {
		p.relative_fetch_path(file_path, false);
	} catch(FileNotFoundException &e) {
		exception_caught = true;
	}
	REQUIRE(exception_caught);
}

TEST_CASE_METHOD(PackageTestsFixture, "Test relative_fetch_path method (non relative ('.') paths)", "")
{
	Package p(this->ns, "test_package", ".", ".");

	std::string file_path = this->cwd + "/blah";
	std::ofstream test_file;
	test_file.open(file_path);
	test_file << "blah";
	test_file.close();

	REQUIRE(p.relative_fetch_path(file_path, true) == ("./" + file_path));

	bool exception_caught = false;
	try {
		p.relative_fetch_path(file_path, false);
	} catch(FileNotFoundException &e) {
		exception_caught = true;
	}
	REQUIRE(exception_caught);
}

TEST_CASE_METHOD(PackageTestsFixture, "Test relative_fetch_path method (package and non-package path)", "")
{
	Package p(this->ns, "test_package", ".", ".");

	filesystem::create_directories("package/test_package/" + this->cwd);

	std::string file_path1 = "package/test_package/"+ this->cwd + "/blah";
	std::ofstream test_file1;
	test_file1.open(file_path1);
	test_file1 << "blah";
	test_file1.close();

	std::string file_path2 = this->cwd + "/blah";
	std::ofstream test_file2;
	test_file2.open(file_path2);
	test_file2 << "blah";
	test_file2.close();

	REQUIRE(p.relative_fetch_path(file_path2, false) == ("./" + file_path1));
	REQUIRE(p.relative_fetch_path(file_path2, true) == ("./" + file_path1));
}

TEST_CASE_METHOD(PackageTestsFixture, "Test buildInfo method", "")
{
	Package p(this->ns, "test_package", ".", ".");

	filesystem::create_directories("output/test_namespace/test_package/work/");

	std::string file_path1 = "output/test_namespace/test_package/work/.output.info";
	std::ofstream test_file1;
	test_file1.open(file_path1);
	test_file1 << "blah";
	test_file1.close();

	std::string path;
	std::string hash;
	p.setHashOutput(false);
	auto type = p.buildInfo(&path, &hash);

	REQUIRE(type == Package::BuildInfoType::Build);
	REQUIRE(path == "output/test_namespace/test_package/work/.build.info");
	// Due to the way the test is run a hash won't be set for .build.info
	REQUIRE(hash == "");

	path = "";
	hash = "";
	p.setHashOutput(true);
	type = p.buildInfo(&path, &hash);

	REQUIRE(type == Package::BuildInfoType::Output);
	REQUIRE(path == "output/test_namespace/test_package/work/.output.info");
	REQUIRE(hash == "8b7df143d91c716ecfa5fc1730022f6b421b05cedee8fd52b1fc65a96030ad52");
}

