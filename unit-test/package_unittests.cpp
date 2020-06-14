#define CATCH_CONFIG_MAIN

#include "include/buildsys.h"
#include <catch2/catch.hpp>

using namespace buildsys;

class PackageTestsFixture
{
protected:
	NameSpace *ns{nullptr};

public:
	PackageTestsFixture()
	{
		this->ns = NameSpace::findNameSpace("test_namespace");
	}
	~PackageTestsFixture()
	{
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

