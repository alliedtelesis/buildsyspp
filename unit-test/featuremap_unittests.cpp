#define CATCH_CONFIG_MAIN

#include "exceptions.hpp"
#include "featuremap.hpp"
#include <catch2/catch.hpp>

using namespace buildsys;

TEST_CASE("Test set/get with override", "")
{
	FeatureMap featuremap;

	featuremap.setFeature("test_feature", "test_value1");
	REQUIRE(featuremap.getFeature("test_feature") == "test_value1");

	// By default we don't override
	featuremap.setFeature("test_feature", "test_value2");
	REQUIRE(featuremap.getFeature("test_feature") == "test_value1");

	// Explicitly don't override
	featuremap.setFeature("test_feature", "test_value2", false);
	REQUIRE(featuremap.getFeature("test_feature") == "test_value1");

	// Explicitly override
	featuremap.setFeature("test_feature", "test_value2", true);
	REQUIRE(featuremap.getFeature("test_feature") == "test_value2");
}

TEST_CASE("Test setFeature with key=value string", "")
{
	FeatureMap featuremap;
	bool ret;

	ret = featuremap.setFeature("test_feature=test_value1");
	REQUIRE(ret);
	REQUIRE(featuremap.getFeature("test_feature") == "test_value1");

	// Using setFeature with key=value string should override
	ret = featuremap.setFeature("test_feature=test_value2");
	REQUIRE(ret);
	REQUIRE(featuremap.getFeature("test_feature") == "test_value2");

	// Using setFeature with incorrectly formatted string should fail
	ret = featuremap.setFeature("test_feature:test_value3");
	REQUIRE(!ret);
	// Value should remain unchanged
	REQUIRE(featuremap.getFeature("test_feature") == "test_value2");
}

TEST_CASE("Test getFeature throws NoKeyException for non-existent feature", "")
{
	FeatureMap featuremap;
	bool exception_caught = false;

	try {
		featuremap.getFeature("test_feature");
	} catch(NoKeyException &e) {
		exception_caught = true;
	}

	REQUIRE(exception_caught);
}

TEST_CASE("Test printFeatureValues function", "")
{
	FeatureMap featuremap;

	featuremap.setFeature("test_feature1", "test_value1");
	featuremap.setFeature("test_feature2", "test_value2");
	featuremap.setFeature("test_feature3", "test_value3");

	std::string expected = std::string("\n----BEGIN FEATURE VALUES----\n") +
	                       std::string("test_feature1\ttest_value1\n") +
	                       std::string("test_feature2\ttest_value2\n") +
	                       std::string("test_feature3\ttest_value3\n") +
	                       std::string("----END FEATURE VALUES----\n");

	std::stringstream buffer;
	featuremap.printFeatureValues(buffer);
	REQUIRE(buffer.str() == expected);
}
