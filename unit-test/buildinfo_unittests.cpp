#define CATCH_CONFIG_MAIN

#include "buildinfo.hpp"
#include <catch2/catch.hpp>

using namespace buildsys;

class BuildDescriptionTestsFixture
{
public:
	BuildDescriptionTestsFixture()
	{
		// Reset the ignored features at the start of each test
		BuildDescription::set_ignored_features({});
	}
};

TEST_CASE_METHOD(BuildDescriptionTestsFixture, "Test add_feature_value() function", "")
{
	BuildDescription desc;

	desc.add_feature_value("test_feature1", "test_value1");
	desc.add_feature_value("test_feature2", "test_value2");

	std::stringstream buffer;
	desc.print(buffer);
	REQUIRE(buffer.str() == "FeatureValue test_feature1 test_value1\n"
	                        "FeatureValue test_feature2 test_value2\n");
}

TEST_CASE_METHOD(BuildDescriptionTestsFixture, "Test set_ignored_features() function", "")
{
	BuildDescription desc;

	BuildDescription::set_ignored_features({"test_feature1", "test_feature2"});
	desc.add_feature_value("test_feature1", "test_value1");
	desc.add_feature_value("test_feature2", "test_value2");

	std::stringstream buffer;
	desc.print(buffer);
	REQUIRE(buffer.str() == "");
}

TEST_CASE_METHOD(BuildDescriptionTestsFixture, "Test add_nil_feature_value() function", "")
{
	BuildDescription desc;

	desc.add_nil_feature_value("test_feature1");

	std::stringstream buffer;
	desc.print(buffer);
	REQUIRE(buffer.str() == "FeatureNil test_feature1\n");
}

TEST_CASE_METHOD(BuildDescriptionTestsFixture, "Test add_package_file() function", "")
{
	BuildDescription desc;

	desc.add_package_file("test_package_file", "test_hash_abc123");

	std::stringstream buffer;
	desc.print(buffer);
	REQUIRE(buffer.str() == "PackageFile test_package_file test_hash_abc123\n");
}

TEST_CASE_METHOD(BuildDescriptionTestsFixture, "Test add_require_file() function", "")
{
	BuildDescription desc;

	desc.add_require_file("test_require_file", "test_hash_abc123");

	std::stringstream buffer;
	desc.print(buffer);
	REQUIRE(buffer.str() == "RequireFile test_require_file test_hash_abc123\n");
}

TEST_CASE_METHOD(BuildDescriptionTestsFixture, "Test add_output_info_file() function", "")
{
	BuildDescription desc;

	desc.add_output_info_file("test_output_info_file", "test_hash_abc123");

	std::stringstream buffer;
	desc.print(buffer);
	REQUIRE(buffer.str() == "OutputInfoFile test_output_info_file test_hash_abc123\n");
}

TEST_CASE_METHOD(BuildDescriptionTestsFixture, "Test add_build_info_file() function", "")
{
	BuildDescription desc;

	desc.add_build_info_file("test_build_info_file", "test_hash_abc123");

	std::stringstream buffer;
	desc.print(buffer);
	REQUIRE(buffer.str() == "BuildInfoFile test_build_info_file test_hash_abc123\n");
}

TEST_CASE_METHOD(BuildDescriptionTestsFixture, "Test add_extraction_info_file() function",
                 "")
{
	BuildDescription desc;

	desc.add_extraction_info_file("test_extraction_info_file", "test_hash_abc123");

	std::stringstream buffer;
	desc.print(buffer);
	REQUIRE(buffer.str() ==
	        "ExtractionInfoFile test_extraction_info_file test_hash_abc123\n");
}
