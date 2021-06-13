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

#include "options.hpp"
#include "exceptions.hpp"
#include "include/buildsys.h"
#include "interface/luainterface.h"
#include <vector>

/**
 * Parse the command line arguments provided to buildsys++.
 *
 * @param argc - The number of arguments.
 * @param argc - The arguments.
 *
 * @returns The parsed target string from the command line. If any parsing errors
 *          occur then an exception is thrown.
 */
std::string buildsys::parse_command_line(int argc, char *argv[], World *WORLD)
{
	if(argc <= 1) {
		throw CustomException("At least 1 parameter is required");
	}

	std::vector<std::string> argList(argv, argv + argc);

	size_t a = 2;
	bool foundDashDash = false;
	std::vector<std::string> ignored_features;
	while(a < argList.size() && !foundDashDash) {
		if(argList[a] == "--clean") {
			Package::set_clean_packages(true);
		} else if(argList[a] == "--cache-server") {
			Package::set_build_cache(argList[a + 1]);
			a++;
		} else if(argList[a] == "--tarball-cache") {
			DownloadFetch::setTarballCache(argList[a + 1]);
			a++;
		} else if(argList[a] == "--overlay") {
			Package::add_overlay_path(argList[a + 1]);
			a++;
		} else if(argList[a] == "--build-info-ignore-fv") {
			ignored_features.push_back(argList[a + 1]);
			a++;
		} else if(argList[a] == "--parse-only") {
			WORLD->setParseOnly();
		} else if(argList[a] == "--keep-going") {
			WORLD->setKeepGoing();
		} else if(argList[a] == "--quietly") {
			Package::set_quiet_packages(true);
		} else if(argList[a] == "--keep-staging") {
			Package::set_keep_all_staging(true);
		} else if(argList[a] == "--parallel-packages") {
			WORLD->setThreadsLimit(std::stoi(argList[a + 1]));
			// If a thread limit is specified then don't allow packages to extract
			// in parallel.
			Package::set_extract_in_parallel(false);
			a++;
		} else if(argList[a] == "--") {
			foundDashDash = true;
		} else {
			Package::add_forced_package(argList[a]);
		}
		a++;
	}
	// then we find a --
	if(foundDashDash) {
		// then we can preload the feature set
		while(a < argList.size()) {
			if(!li_get_feature_map()->setFeature(argList[a])) {
				throw CustomException(
				    "setFeature Failed: Features must be described as feature=value");
			}
			a++;
		}
	}
	BuildDescription::set_ignored_features(ignored_features);

	return argList[1];
}
