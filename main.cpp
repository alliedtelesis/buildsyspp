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
#include "include/color.h"

static bool quietly = false;

void buildsys::log(const std::string &package, const std::string &str)
{
	std::cerr << boost::format{"%1%: %2%"} % package % str << std::endl;
}

void buildsys::log(const std::string &package, const boost::format &str)
{
	log(package, str.str());
}

void buildsys::log(Package *P, const std::string &str)
{
	std::ostream &s = quietly ? P->getLogFile() : std::cerr;
	auto msg = boost::format{"%1%,%2%: %3%"} % P->getNS()->getName() % P->getName() % str;
	s << msg << std::endl;
}

void buildsys::log(Package *P, const boost::format &str)
{
	log(P, str.str());
}

static inline const char *get_color(const std::string &mesg)
{
	if(mesg.find("error:") != std::string::npos) {
		return COLOR_BOLD_RED;
	}
	if(mesg.find("warning:") != std::string::npos) {
		return COLOR_BOLD_BLUE;
	}

	return nullptr;
}

void buildsys::program_output(Package *P, const std::string &mesg)
{
	static int isATTY = isatty(fileno(stdout));
	const char *color;
	std::ostream &s = quietly ? P->getLogFile() : std::cout;
	boost::format msg;

	if(!quietly && (isATTY != 0) && ((color = get_color(mesg)) != nullptr)) {
		msg = boost::format{"%1%,%2%: %3%%4%%5%"} % P->getNS()->getName() % P->getName() %
		      color % mesg % COLOR_RESET;
	} else {
		msg = boost::format{"%1%,%2%: %3%"} % P->getNS()->getName() % P->getName() % mesg;
	}
	s << msg << std::endl;
}

int main(int argc, char *argv[])
{
	struct timespec start, end;

	clock_gettime(CLOCK_REALTIME, &start);

	log("BuildSys", "Buildsys (C++ version)");
	log("BuildSys", boost::format{"Built: %1% %2%"} % __TIME__ % __DATE__);

	if(argc <= 1) {
		error("At least 1 parameter is required");
		exit(-1);
	}

	std::vector<std::string> argList(argv, argv + argc);

	World WORLD(argList[0]);
	hash_setup();

	// process arguments ...
	// first we take a list of package names to exclusevily build
	// this will over-ride any dependency checks and force them to be built
	// without first building their dependencies
	size_t a = 2;
	bool foundDashDash = false;
	while(a < argList.size() && !foundDashDash) {
		if(argList[a] == "--clean") {
			WORLD.setCleaning();
		} else if(argList[a] == "--no-output-prefix" || argList[a] == "--nop") {
			WORLD.clearOutputPrefix();
		} else if(argList[a] == "--cache-server" || argList[a] == "--ff") {
			WORLD.setFetchFrom(argList[a + 1]);
			a++;
		} else if(argList[a] == "--tarball-cache") {
			log("BuildSys",
			    boost::format{"Setting tarball cache to %1%"} % (argList[a + 1]));
			WORLD.setTarballCache(argList[a + 1]);
			a++;
		} else if(argList[a] == "--overlay") {
			WORLD.addOverlayPath(argList[a + 1]);
			a++;
		} else if(argList[a] == "--extract-only") {
			WORLD.setExtractOnly();
		} else if(argList[a] == "--build-info-ignore-fv") {
			WORLD.ignoreFeature(argList[a + 1]);
			a++;
		} else if(argList[a] == "--parse-only") {
			WORLD.setParseOnly();
		} else if(argList[a] == "--keep-going") {
			WORLD.setKeepGoing();
		} else if(argList[a] == "--quietly") {
			quietly = true;
		} else if(argList[a] == "--parallel-packages" || argList[a] == "-j") {
			WORLD.setThreadsLimit(std::stoi(argList[a + 1]));
			a++;
		} else if(argList[a] == "--") {
			foundDashDash = true;
		} else {
			WORLD.forceBuild(argList[a]);
		}
		a++;
	}
	// then we find a --
	if(foundDashDash) {
		// then we can preload the feature set
		while(a < argList.size()) {
			if(!WORLD.featureMap()->setFeature(argList[a])) {
				error("setFeature: Failed");
				exit(-1);
			}
			a++;
		}
	}

	std::string target = argList[1];
	if(target.find(".lua") == std::string::npos) {
		target = target + ".lua";
	}

	if(WORLD.noIgnoredFeatures()) {
		// Implement old behaviour
		WORLD.ignoreFeature("job-limit");
		WORLD.ignoreFeature("load-limit");
	}

	if(!WORLD.basePackage(target)) {
		error("Building: Failed");
		if(WORLD.areKeepGoing()) {
			hash_shutdown();
		}
		exit(-1);
	}

	if(WORLD.areParseOnly()) {
		// Print all the feature/values
		WORLD.featureMap()->printFeatureValues();
	}
	// Write out the dependency graph
	WORLD.output_graph();

	clock_gettime(CLOCK_REALTIME, &end);

	if(end.tv_nsec >= start.tv_nsec) {
		log(argList[1], boost::format{"Total time: %1%s and %2%ms"} %
		                    (end.tv_sec - start.tv_sec) %
		                    ((end.tv_nsec - start.tv_nsec) / 1000000));
	} else {
		log(argList[1], boost::format{"Total time: %1%s and %2%ms"} %
		                    (end.tv_sec - start.tv_sec - 1) %
		                    ((1000 + end.tv_nsec / 1000000) - start.tv_nsec / 1000000));
	}

	hash_shutdown();

	return 0;
}
