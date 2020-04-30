#define CATCH_CONFIG_MAIN

#include "exceptions.hpp"
#include <catch2/catch.hpp>

using namespace buildsys;

TEST_CASE("Test CustomException", "")
{
	bool exception_caught = false;
	std::string exception_message("");
	std::string test_message("Test exception message");

	try {
		throw CustomException(test_message);
	} catch(CustomException &e) {
		exception_caught = true;
		exception_message = e.what();
	}

	REQUIRE(exception_caught);
	REQUIRE(exception_message == test_message);

	exception_caught = false;
	exception_message = "";

	try {
		throw CustomException(test_message);
	} catch(std::runtime_error &e) {
		exception_caught = true;
		exception_message = e.what();
	}

	REQUIRE(exception_caught);
	REQUIRE(exception_message == test_message);

	exception_caught = false;
	exception_message = "";

	try {
		throw CustomException(test_message);
	} catch(std::exception &e) {
		exception_caught = true;
		exception_message = e.what();
	}

	REQUIRE(exception_caught);
	REQUIRE(exception_message == test_message);
}

TEST_CASE("Test NoKeyException", "")
{
	bool exception_caught = false;
	std::string exception_message("");
	std::string expected_message("Key does not exist");

	try {
		throw NoKeyException();
	} catch(NoKeyException &e) {
		exception_caught = true;
		exception_message = e.what();
	}

	REQUIRE(exception_caught);
	REQUIRE(exception_message == expected_message);

	exception_caught = false;
	exception_message = "";

	try {
		throw NoKeyException();
	} catch(std::runtime_error &e) {
		exception_caught = true;
		exception_message = e.what();
	}

	REQUIRE(exception_caught);
	REQUIRE(exception_message == expected_message);

	exception_caught = false;
	exception_message = "";

	try {
		throw NoKeyException();
	} catch(std::exception &e) {
		exception_caught = true;
		exception_message = e.what();
	}

	REQUIRE(exception_caught);
	REQUIRE(exception_message == expected_message);
}

TEST_CASE("Test FileNotFoundException", "")
{
	bool exception_caught = false;
	std::string exception_message("");
	std::string expected_message("test_location: File not found 'test_file'");

	try {
		throw FileNotFoundException("test_file", "test_location");
	} catch(FileNotFoundException &e) {
		exception_caught = true;
		exception_message = e.what();
	}

	REQUIRE(exception_caught);
	REQUIRE(exception_message == expected_message);

	exception_caught = false;
	exception_message = "";

	try {
		throw FileNotFoundException("test_file", "test_location");
	} catch(std::runtime_error &e) {
		exception_caught = true;
		exception_message = e.what();
	}

	REQUIRE(exception_caught);
	REQUIRE(exception_message == expected_message);

	exception_caught = false;
	exception_message = "";

	try {
		throw FileNotFoundException("test_file", "test_location");
	} catch(std::exception &e) {
		exception_caught = true;
		exception_message = e.what();
	}

	REQUIRE(exception_caught);
	REQUIRE(exception_message == expected_message);
}
