#define CATCH_CONFIG_MAIN

#include "include/buildsys.h"
#include <catch2/catch.hpp>

using namespace buildsys;

class NameSpaceTestsFixture
{
protected:
	std::streambuf *coutbuf{nullptr};

public:
	NameSpaceTestsFixture()
	{
		// Some of the tests may redirect std::cout. Ensure that we restore it
		// at the end of each test.
		this->coutbuf = std::cout.rdbuf();
	}
	~NameSpaceTestsFixture()
	{
		NameSpace::deleteAll();
		std::cout.rdbuf(this->coutbuf);
	}
};

/**
 * Count the number of namespaces that have been created.
 *
 * @returns The number of namespaces.
 */
static int
count_namespaces ()
{
	int count = 0;
	NameSpace::for_each([&count](const NameSpace &ns) { count++; });
	return count;
}

TEST_CASE_METHOD(NameSpaceTestsFixture, "Test creation of NameSpace and lookup of existing", "")
{
	REQUIRE(count_namespaces() == 0);
	NameSpace::findNameSpace("test1");
	REQUIRE(count_namespaces() == 1);
	NameSpace::findNameSpace("test2");
	REQUIRE(count_namespaces() == 2);
	NameSpace::findNameSpace("test1");
	REQUIRE(count_namespaces() == 2);
}

TEST_CASE_METHOD(NameSpaceTestsFixture, "Test getName(), getStagingDir() and getInstallDir", "")
{
	NameSpace *ns = NameSpace::findNameSpace("test1");

	REQUIRE(ns->getName() == "test1" );
	REQUIRE(ns->getStagingDir() == "output/test1/staging");
	REQUIRE(ns->getInstallDir() == "output/test1/install");
}

TEST_CASE_METHOD(NameSpaceTestsFixture, "Test printNameSpaces()", "")
{
	NameSpace::findNameSpace("test1");
	NameSpace::findNameSpace("test2");

	std::stringstream buffer;
	// Redirect std::cout so we can verify what is printed.
	std::cout.rdbuf(buffer.rdbuf());

	NameSpace::printNameSpaces();

	std::string expected = std::string("\n----BEGIN NAMESPACES----\n") +
	                       std::string("test1\n") +
	                       std::string("test2\n") +
	                       std::string("----END NAMESPACES----\n");
	REQUIRE(buffer.str() == expected);
}

/**
 * Count the number of packages that have been created in the NameSpace.
 *
 * @returns The number of packages.
 */
static int
count_packages (const NameSpace *ns)
{
	int count = 0;
	ns->for_each_package([&count](Package &p) { count++; });
	return count;
}

TEST_CASE_METHOD(NameSpaceTestsFixture, "Test addPackage() and findPackage()", "")
{
	NameSpace *ns = NameSpace::findNameSpace("test1");

	REQUIRE(count_packages(ns) == 0);
	ns->addPackage(std::make_unique<Package>(ns, "test_package1", "", ""));
	REQUIRE(count_packages(ns) == 1);
	ns->addPackage(std::make_unique<Package>(ns, "test_package2", "", ""));
	REQUIRE(count_packages(ns) == 2);

	Package *p = ns->findPackage("test_package1");
	REQUIRE(count_packages(ns) == 2);
	REQUIRE(p->getName() == "test_package1");
}

