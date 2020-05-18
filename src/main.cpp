/******************************************************************************
 Copyright 2013 Allied Telesis Labs Ltd. All rights reserved.

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

#include "include/buildsys.h"
#include "interface/luainterface.h"
#include "logger.hpp"

int main(int argc, char *argv[])
{
	// clang-format off
	struct timespec start {}, end {};
	// clang-format on
	Logger logger("BuildSys");

	clock_gettime(CLOCK_REALTIME, &start);

	logger.log("Buildsys (C++ version)");
	logger.log(boost::format{"Built: %1% %2%"} % __TIME__ % __DATE__);

	if(argc <= 1) {
		logger.log("At least 1 parameter is required");
		exit(-1);
	}

	std::vector<std::string> argList(argv, argv + argc);

	World WORLD;
	hash_setup();

	// process arguments ...
	// first we take a list of package names to exclusevily build
	// this will over-ride any dependency checks and force them to be built
	// without first building their dependencies
	size_t a = 2;
	bool foundDashDash = false;
	std::vector<std::string> ignored_features;
	while(a < argList.size() && !foundDashDash) {
		if(argList[a] == "--clean") {
			Package::set_clean_packages(true);
		} else if(argList[a] == "--cache-server" || argList[a] == "--ff") {
			Package::set_build_cache(argList[a + 1]);
			a++;
		} else if(argList[a] == "--tarball-cache") {
			logger.log(boost::format{"Setting tarball cache to %1%"} % (argList[a + 1]));
			DownloadFetch::setTarballCache(argList[a + 1]);
			a++;
		} else if(argList[a] == "--overlay") {
			Package::add_overlay_path(argList[a + 1]);
			a++;
		} else if(argList[a] == "--build-info-ignore-fv") {
			ignored_features.push_back(argList[a + 1]);
			a++;
		} else if(argList[a] == "--parse-only") {
			WORLD.setParseOnly();
		} else if(argList[a] == "--keep-going") {
			WORLD.setKeepGoing();
		} else if(argList[a] == "--quietly") {
			Package::set_quiet_packages(true);
		} else if(argList[a] == "--parallel-packages" || argList[a] == "-j") {
			WORLD.setThreadsLimit(std::stoi(argList[a + 1]));
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
				logger.log(
				    "setFeature Failed: Features must be described as feature=value");
				exit(-1);
			}
			a++;
		}
	}

	std::string target = argList[1];
	if(target.find(".lua") == std::string::npos) {
		target = target + ".lua";
	}

	BuildDescription::set_ignored_features(ignored_features);

	if(!WORLD.basePackage(target)) {
		logger.log("Building: Failed");
		if(WORLD.areKeepGoing()) {
			hash_shutdown();
		}
		exit(-1);
	}

	if(WORLD.areParseOnly()) {
		// Print all the feature/values
		li_get_feature_map()->printFeatureValues(std::cout);
		NameSpace::printNameSpaces();
	}
	// Write out the dependency graph
	WORLD.output_graph();

	clock_gettime(CLOCK_REALTIME, &end);

	logger.log("Finished: " + argList[1]);
	if(end.tv_nsec >= start.tv_nsec) {
		logger.log(boost::format{"Total time: %1%s and %2%ms"} %
		           (end.tv_sec - start.tv_sec) % ((end.tv_nsec - start.tv_nsec) / 1000000));
	} else {
		logger.log(boost::format{"Total time: %1%s and %2%ms"} %
		           (end.tv_sec - start.tv_sec - 1) %
		           ((1000 + end.tv_nsec / 1000000) - start.tv_nsec / 1000000));
	}

	hash_shutdown();

	return 0;
}
