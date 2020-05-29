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
#include "options.hpp"

int main(int argc, char *argv[])
{
	steady_clock::time_point start = steady_clock::now();

	Logger logger("BuildSys");
	logger.log("Buildsys (C++ version)");
	logger.log(boost::format{"Built: %1% %2%"} % __TIME__ % __DATE__);

	World WORLD;
	hash_setup();

	std::string target;
	try {
		target = parse_command_line(argc, argv, &WORLD);
	} catch(CustomException &e) {
		logger.log(e.what());
		exit(-1);
	} catch(std::exception &e) {
		logger.log("Invalid command line arguments");
		exit(-1);
	}

	if(target.find(".lua") == std::string::npos) {
		target = target + ".lua";
	}

	// Resolve any symbolic links
	char *resolved_path = realpath(target.c_str(), nullptr);
	if(resolved_path == nullptr) {
		logger.log("Base package path does not exist");
		return false;
	}
	std::string filename = std::string(resolved_path);
	free(resolved_path); // NOLINT

	if(!WORLD.basePackage(filename)) {
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

	logger.log("Finished: " + target);

	steady_clock::time_point end = steady_clock::now();
	auto duration = duration_cast<std::chrono::milliseconds>(end - start).count();
	logger.log(boost::format{"Total time: %1%s and %2%ms"} % (duration / 1000) %
	           (duration % 1000));

	hash_shutdown();

	return 0;
}
