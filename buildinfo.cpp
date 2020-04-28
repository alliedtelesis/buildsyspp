/******************************************************************************
 Copyright 2019 Allied Telesis Labs Ltd. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright
       notice, this list of conditions and the following disclaimer in the
       documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#include "buildinfo.hpp"
#include <boost/format.hpp>

using namespace buildsys;

static std::vector<std::string> ignored_features;

/**
 * Add an ignored feature for all BuildDescription instances.
 *
 * @param feature - The name of the feature to ignore.
 */
void BuildDescription::ignore_feature(const std::string &feature)
{
	ignored_features.push_back(feature);
}

/**
 * Add a feature value pair to the BuildDescription.
 *
 * @param feature - The name of the feature.
 * @param value - The value for the feature.
 */
void BuildDescription::add_feature_value(const std::string &feature,
                                         const std::string &value)
{
	bool is_ignored = (std::find(ignored_features.begin(), ignored_features.end(),
	                             feature) != ignored_features.end());
	if(!is_ignored) {
		auto desc = boost::format{"FeatureValue %1% %2%"} % feature % value;
		this->BUs.push_back(desc.str());
	}
}

/**
 * Add a nil feature to the BuildDescription.
 *
 * @param feature - The name of the feature that was nil.
 */
void BuildDescription::add_nil_feature_value(const std::string &feature)
{
	auto desc = boost::format{"FeatureNil %1%"} % feature;
	this->BUs.push_back(desc.str());
}

/**
 * Add the package file to the BuildDescription.
 *
 * @param fname - The name of the file describing the package being built.
 * @param hash - The hash of the file.
 */
void BuildDescription::add_package_file(const std::string &fname, const std::string &hash)
{
	auto desc = boost::format{"PackageFile %1% %2%"} % fname % hash;
	this->BUs.push_back(desc.str());
}

/**
 * Add a require file to the BuildDescription.
 *
 * @param fname - The name of the file required by the package being built.
 * @param hash - The hash of the file.
 */
void BuildDescription::add_require_file(const std::string &fname, const std::string &hash)
{
	auto desc = boost::format{"RequireFile %1% %2%"} % fname % hash;
	this->BUs.push_back(desc.str());
}

/**
 * Add an output info file of a depended package to the BuildDescription.
 *
 * @param fname - The name of the output info file of a depended package.
 * @param hash - The hash of the file.
 */
void BuildDescription::add_output_info_file(const std::string &fname,
                                            const std::string &hash)
{
	auto desc = boost::format{"OutputInfoFile %1% %2%"} % fname % hash;
	this->BUs.push_back(desc.str());
}

/**
 * Add a build info file of a depended package to the BuildDescription.
 *
 * @param fname - The name of the build info file of a depended package.
 * @param hash - The hash of the file.
 */
void BuildDescription::add_build_info_file(const std::string &fname,
                                           const std::string &hash)
{
	auto desc = boost::format{"BuildInfoFile %1% %2%"} % fname % hash;
	this->BUs.push_back(desc.str());
}

/**
 * Add the extraction info for the package being built to the BuildDescription.
 *
 * @param fname - The name of the extraction info file.
 * @param hash - The hash of the file.
 */
void BuildDescription::add_extraction_info_file(const std::string &fname,
                                                const std::string &hash)
{
	auto desc = boost::format{"ExtractionInfoFile %1% %2%"} % fname % hash;
	this->BUs.push_back(desc.str());
}

/**
 * Print the BuildDescription.
 *
 * @param out - The std::ostream to print to.
 */
void BuildDescription::print(std::ostream &out) const
{
	for(auto &unit : this->BUs) {
		out << unit << std::endl;
	}
}
