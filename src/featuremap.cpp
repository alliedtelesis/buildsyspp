/******************************************************************************
 Copyright 2020 Allied Telesis Labs Ltd. All rights reserved.

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

#include "featuremap.hpp"
#include "exceptions.hpp"
#include <iostream>

using namespace buildsys;

/**
 * Set a feature to a specific value
 *
 * @param key - The feature to set the value for.
 * @param value - The value to set.
 * @param override - true to overwrite the feature value if it already exists,
 *                   false to not overwrite an existing feature.
 */
void FeatureMap::setFeature(std::string key, std::string value, bool override)
{
	std::unique_lock<std::mutex> lk(this->lock);
	// look for this key
	if(features.find(key) != features.end()) {
		if(override) {
			// only over-write the value if we are explicitly told to
			features[key] = value;
		}
	} else {
		features.insert(std::pair<std::string, std::string>(key, value));
	}
}

/**
 * Set a feature using a string with format: "key=value"
 *
 * @param kv - The key:value string to set.
 *
 * @returns true if it was set, false otherwise (invalid string).
 */
bool FeatureMap::setFeature(const std::string &kv)
{
	auto pos = kv.find('=');
	if(pos == std::string::npos) {
		return false;
	}

	std::string key = kv.substr(0, pos);
	std::string value = kv.substr(pos + 1);

	this->setFeature(key, value, true);
	return true;
}

/**
 * Get the value of a specific feature
 *
 * @param key - The feature key to get the value for.
 *
 * @returns The string value if the feature was found, otherwise
 *          throws a NoKeyException exception.
 */
std::string FeatureMap::getFeature(const std::string &key) const
{
	std::unique_lock<std::mutex> lk(this->lock);
	if(features.find(key) != features.end()) {
		return features.at(key);
	}
	throw NoKeyException();
}

/**
 * Print all feature/values to std::cout.
 */
void FeatureMap::printFeatureValues(std::ostream &out) const
{
	std::unique_lock<std::mutex> lk(this->lock);
	out << std::endl << "----BEGIN FEATURE VALUES----" << std::endl;
	for(auto &feature : this->features) {
		out << feature.first << "\t" << feature.second << std::endl;
	}
	out << "----END FEATURE VALUES----" << std::endl;
}
