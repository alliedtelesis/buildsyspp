#define CATCH_CONFIG_MAIN

#include "hash.hpp"
#include "packagecmd.hpp"
#include <boost/format.hpp>
#include <catch2/catch.hpp>
#include <filesystem>

using namespace buildsys;
namespace filesystem = std::filesystem;

class HashTestsFixture
{
protected:
	std::string cwd{"hash_test_dir"};
	std::streambuf *coutbuf{nullptr};
	std::stringstream stdout_buffer;

public:
	HashTestsFixture()
	{
		hash_setup();
		// Ensure that we restore std::cout at the end of each test.
		this->coutbuf = std::cout.rdbuf();
		// Redirect std::cout so we can verify what is printed.
		std::cout.rdbuf(this->stdout_buffer.rdbuf());
		filesystem::create_directories(this->cwd);
	}
	~HashTestsFixture()
	{
		hash_shutdown();
		std::cout.rdbuf(this->coutbuf);
		filesystem::remove_all(this->cwd);
	}
};

TEST_CASE_METHOD(HashTestsFixture, "Test hash_file() function for non-existent file", "")
{
	std::string file_path("qwekjh123.txt");
	REQUIRE(!filesystem::exists(file_path));
	REQUIRE(hash_file(file_path) == "");
	auto expected = boost::format{"BuildSys: Failed opening: %1%\n"} % file_path;
	REQUIRE(this->stdout_buffer.str() == expected.str());
}

TEST_CASE_METHOD(HashTestsFixture, "Test hash_file() function for existent file", "")
{
	std::string file_path = this->cwd + "/test_file.txt";
	std::ofstream test_file;
	test_file.open(file_path);
	test_file << "This is some test data.\n";
	test_file.close();

	std::string hash = hash_file(file_path);
	// REQUIRE(hash_file(file_path) ==
	// "524d7edddd0f6e364120af132ce1100d4200246aecb2540519d8280c648f026b");
	REQUIRE(!hash.empty());
	// We don't expect any output
	REQUIRE(this->stdout_buffer.str() == "");

	PackageCmd pc(".", "sha256sum");
	pc.addArg(file_path);
	Logger logger;
	bool ret = pc.Run(&logger);
	REQUIRE(ret);

	// The first part of the packagecmd (sha256sum) output in stdout
	// should match the hash we got via the hash_file function
	std::string output = this->stdout_buffer.str();
	std::string expected_hash = output.substr(0, output.find(' '));
	REQUIRE(expected_hash == hash);
}

TEST_CASE_METHOD(HashTestsFixture, "Test hash_directory() is stable and content-sensitive",
                 "")
{
	std::string dir_path = this->cwd + "/test_dir";
	filesystem::create_directories(dir_path + "/subdir");

	std::ofstream(dir_path + "/a.txt") << "a";
	std::ofstream(dir_path + "/subdir/b.txt") << "b";

	std::string hash = hash_directory(dir_path);
	REQUIRE(!hash.empty());

	// Hashing again without any change gives the same result.
	REQUIRE(hash_directory(dir_path) == hash);

	// Changing a file's content changes the directory hash.
	std::ofstream(dir_path + "/subdir/b.txt") << "b changed";
	REQUIRE(hash_directory(dir_path) != hash);
}

TEST_CASE_METHOD(HashTestsFixture,
                 "Test hash_directory() is independent of the order files were created in",
                 "")
{
	std::string dir_a = this->cwd + "/order_a";
	std::string dir_b = this->cwd + "/order_b";
	filesystem::create_directories(dir_a);
	filesystem::create_directories(dir_b);

	// Same content, opposite creation order.
	std::ofstream(dir_a + "/one.txt") << "1";
	std::ofstream(dir_a + "/two.txt") << "2";

	std::ofstream(dir_b + "/two.txt") << "2";
	std::ofstream(dir_b + "/one.txt") << "1";

	REQUIRE(hash_directory(dir_a) == hash_directory(dir_b));
}

TEST_CASE_METHOD(HashTestsFixture, "Test hash_directory() follows symlinked subdirectories",
                 "")
{
	// extract()'s cp runs with -L, so a symlink to a directory is
	// dereferenced and its contents are copied; hash_directory() needs to
	// see those files too, or a change inside the linked-to directory
	// won't be noticed as a change to the copied source.
	std::string real_dir = this->cwd + "/real_dir";
	std::string dir_path = this->cwd + "/test_dir";
	filesystem::create_directories(real_dir);
	filesystem::create_directories(dir_path);

	std::ofstream(real_dir + "/linked.txt") << "original";
	filesystem::create_directory_symlink(filesystem::absolute(real_dir),
	                                     dir_path + "/link");

	std::string hash = hash_directory(dir_path);
	REQUIRE(!hash.empty());

	std::ofstream(real_dir + "/linked.txt") << "changed";
	REQUIRE(hash_directory(dir_path) != hash);
}
