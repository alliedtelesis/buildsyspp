#define CATCH_CONFIG_MAIN

#include "include/filesystem.h"
#include "logger.hpp"
#include <catch2/catch.hpp>

using namespace buildsys;

class LoggerTestsFixture
{
protected:
	std::streambuf *coutbuf{nullptr};

public:
	LoggerTestsFixture()
	{
		// Some of the tests may redirect std::cout. Ensure that we restore it
		// at the end of each test.
		this->coutbuf = std::cout.rdbuf();
	}
	~LoggerTestsFixture()
	{
		std::cout.rdbuf(this->coutbuf);
	}
};

TEST_CASE_METHOD(LoggerTestsFixture, "Test Logger default constructor", "")
{
	Logger logger;

	std::stringstream buffer;
	// Redirect std::cout so we can verify what is printed.
	std::cout.rdbuf(buffer.rdbuf());

	std::string test_string("This is a test log message");
	logger.log(test_string);
	REQUIRE(buffer.str() == test_string + "\n");

	buffer.str("");

	auto test_format = boost::format{"Check %1%"} % test_string;
	logger.log(test_format);
	REQUIRE(buffer.str() == "Check " + test_string + "\n");
}

TEST_CASE_METHOD(LoggerTestsFixture, "Test Logger prefix only constructor", "")
{
	std::string test_prefix("test_prefix");
	Logger logger(test_prefix);

	std::stringstream buffer;
	// Redirect std::cout so we can verify what is printed.
	std::cout.rdbuf(buffer.rdbuf());

	std::string test_string("This is a test log message");
	logger.log(test_string);
	REQUIRE(buffer.str() == test_prefix + ": " + test_string + "\n");
}

TEST_CASE_METHOD(LoggerTestsFixture, "Test Logger prefix and file path constructor", "")
{
	std::string file_path = "test_output_file.txt";
	std::string test_prefix("test_prefix");
	Logger logger(test_prefix, file_path);

	std::stringstream buffer;
	// Redirect std::cout so we can verify what is printed.
	std::cout.rdbuf(buffer.rdbuf());

	std::string test_string("This is a test log message");
	logger.log(test_string);

	// Nothing should be printed to std::cout
	REQUIRE(buffer.str() == "");

	std::ifstream t(file_path);
	std::stringstream file_contents;
	file_contents << t.rdbuf();
	REQUIRE(file_contents.str() == test_prefix + ": " + test_string + "\n");

	filesystem::remove(file_path);
}
