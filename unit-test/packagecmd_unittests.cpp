#define CATCH_CONFIG_MAIN

#include "include/filesystem.h"
#include "packagecmd.hpp"
#include <catch2/catch.hpp>

using namespace buildsys;

class PackageCmdTestsFixture
{
protected:
	std::streambuf *coutbuf{nullptr};

public:
	PackageCmdTestsFixture()
	{
		// Some of the tests may redirect std::cout. Ensure that we restore it
		// at the end of each test.
		this->coutbuf = std::cout.rdbuf();
	}
	~PackageCmdTestsFixture()
	{
		std::cout.rdbuf(this->coutbuf);
	}
};

TEST_CASE_METHOD(PackageCmdTestsFixture, "Test PackageCmd basic behaviour", "")
{
	PackageCmd pc("test_path", "test_app");
	pc.addArg("test_arg");
	pc.addEnv("test_env");

	REQUIRE(pc.getPath() == std::string("test_path"));
	REQUIRE(pc.getApp() == std::string("test_app"));
	REQUIRE(pc.getArgs().at(0) == std::string("test_app"));
	REQUIRE(pc.getArgs().at(1) == std::string("test_arg"));
	// The PackageCmd appends on the current environment so don't assume
	// the only environment value is the one we set.
	REQUIRE(std::find(pc.getEnvp().begin(), pc.getEnvp().end(), std::string("test_env")) !=
	        pc.getEnvp().end());
	REQUIRE(pc.getLogOutput());
}

TEST_CASE_METHOD(PackageCmdTestsFixture, "Test disableLogging() function", "")
{
	PackageCmd pc("test_path", "test_app");

	REQUIRE(pc.getLogOutput());
	pc.disableLogging();
	REQUIRE(!pc.getLogOutput());
}

TEST_CASE_METHOD(PackageCmdTestsFixture, "Test printCmd() function", "")
{
	PackageCmd pc("test_path", "test_app");
	pc.addArg("test_arg");

	Logger logger;
	std::stringstream buffer;
	// Redirect std::cout so we can verify what is printed.
	std::cout.rdbuf(buffer.rdbuf());

	pc.printCmd(&logger);
	std::string expected = std::string("Path: test_path\n") +
	                       std::string("Arg[0] = 'test_app'\n") +
	                       std::string("Arg[1] = 'test_arg'\n");
	REQUIRE(buffer.str() == expected);
}

TEST_CASE_METHOD(PackageCmdTestsFixture, "Test Run() function logs output if configured",
                 "")
{
	PackageCmd pc(".", "/bin/echo");
	pc.addArg("echoed_output");

	std::string file_path = "test_output_file.txt";
	std::string test_prefix("test_prefix");
	Logger logger(test_prefix, file_path);

	std::stringstream buffer;
	// Redirect std::cout so we can verify what is printed.
	std::cout.rdbuf(buffer.rdbuf());

	bool ret = pc.Run(&logger);
	REQUIRE(ret);

	// Nothing should be printed to std::cout
	REQUIRE(buffer.str() == "");

	std::ifstream t(file_path);
	std::stringstream file_contents;
	file_contents << t.rdbuf();

	std::string expected = "test_prefix: echoed_output\n";
	REQUIRE(file_contents.str() == expected);

	filesystem::remove(file_path);
}

TEST_CASE_METHOD(PackageCmdTestsFixture,
                 "Test Run() function does not log output if configured", "")
{
	PackageCmd pc(".", "/bin/echo");
	pc.addArg("echoed_output");
	pc.disableLogging();

	std::string file_path = "test_output_file.txt";
	std::string test_prefix("test_prefix");
	Logger logger(test_prefix, file_path);

	std::stringstream buffer;
	// Redirect std::cout so we can verify what is printed.
	std::cout.rdbuf(buffer.rdbuf());

	bool ret = pc.Run(&logger);
	REQUIRE(ret);

	// Nothing should be printed to the log file or std::cout
	std::ifstream t(file_path);
	std::stringstream file_contents;
	file_contents << t.rdbuf();
	REQUIRE(file_contents.str() == "");
	REQUIRE(buffer.str() == "");

	filesystem::remove(file_path);
}

TEST_CASE_METHOD(PackageCmdTestsFixture,
                 "Test Run() function returns false if underlying command failed", "")
{
	PackageCmd pc(".", "/bin/false");
	pc.disableLogging();

	Logger logger;

	std::stringstream buffer;
	std::cout.rdbuf(buffer.rdbuf());

	bool ret = pc.Run(&logger);
	REQUIRE(!ret);
}
