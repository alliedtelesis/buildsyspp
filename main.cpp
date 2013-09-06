/****************************************************************************************************************************
 Copyright 2013 Allied Telesis Labs Ltd. All rights reserved.
 
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the
distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************************************************************/

#include <buildsys.h>
#include <color.h>

World *buildsys::WORLD;

void buildsys::log(const char *package, const char *fmt, ...)
{
	char *message = NULL;
	va_list args;
	va_start(args, fmt);
	vasprintf(&message, fmt, args);
	va_end(args);

	fprintf(stderr, "%s: %s\n", package, message);
	free(message);
}

static inline const char* get_color(const char *mesg)
{
	if (strstr(mesg, "error:"))
		return COLOR_RED;
	else if (strstr(mesg, "warning:"))
		return COLOR_BLUE;

	return COLOR_NORMAL;
}

#ifdef UNDERSCORE
void buildsys::program_output(const char *package, const char *mesg)
{
	if (isatty(fileno(stdout)))
		fprintf(stdout, "%s: %s%s%s\n",
			package, get_color(mesg), mesg, COLOR_RESET);
	else
		fprintf(stdout, "%s: %s\n", package, mesg);
}
#endif

int main(int argc, char *argv[])
{
	struct timespec start, end;
	
	clock_gettime(CLOCK_REALTIME, &start);

	log((char *)"BuildSys",(char *)"Buildsys (C++ version)");
	log((char *)"BuildSys", "Built: %s %s", __TIME__, __DATE__);

	if(argc <= 1)
	{
		error((char *)"At least 1 parameter is required");
		exit(-1);
	}
	
	WORLD = new World(argv[0]);
	
	if(!interfaceSetup(WORLD->getLua()))
	{
		error((char *)"interfaceSetup: Failed");
		exit(-1);
	}
	
	// process arguments ...
	// first we take a list of package names to exclusevily build
	// this will over-ride any dependency checks and force them to be built
	// without first building their dependencies
	int a = 2;
	bool foundDashDash = false;
	while(a < argc && !foundDashDash)
	{
		if(!strcmp(argv[a],"--clean"))
		{
			WORLD->setCleaning();
		} else
		if(!strcmp(argv[a],"--skip-configure"))
		{
			WORLD->setSkipConfigure();
		} else
#ifdef UNDERSCORE
		if(!strcmp(argv[a],"--no-output-prefix") || !strcmp(argv[a],"--nop"))
		{
			WORLD->clearOutputPrefix();
		} else
#else
		if(!strcmp(argv[a],"--no-output-prefix") || !strcmp(argv[a],"--nop"))
		{
			std::cerr << argv[0] << ": " << argv[a] << " is only supported with UNDERSCORE=y, ignoring" << std::endl;
		} else
#endif
		if(!strcmp(argv[a],"--cache-server") || !strcmp(argv[a],"--ff"))
		{
			WORLD->setFetchFrom(argv[a+1]);
			a++;
		} else
		if(!strcmp(argv[a],"--extract-only"))
		{
			WORLD->setExtractOnly();
		} else
		if(!strcmp(argv[a],"--"))
		{
			foundDashDash = true;
		} else
		{
			WORLD->forceBuild(argv[a]);
		}
		a++;
	}
	// then we find a --
	if(foundDashDash)
	{
		// then we can preload the feature set
		while(a < argc)
		{
			if(!WORLD->setFeature(argv[a]))
			{
				error("setFeature: Failed");
				exit(-1);
			}
			a++;
		}
	}
	char *target = NULL;
	int tn_len = strlen(argv[1]);
	if(argv[1][tn_len-4] != '.')
	{
		asprintf(&target, "%s.lua", argv[1]);
	} else {
		target = strdup(argv[1]);
	}
	if(!WORLD->basePackage(target))
	{
		error("Building: Failed");
		free(target);
		exit(-1);
	}
	free(target);

	// Write out the dependency graph
	WORLD->output_graph();

	clock_gettime(CLOCK_REALTIME, &end);
	
	if (end.tv_nsec >= start.tv_nsec)
	    log(argv[1], (char *)"Total time: %ds and %dms",
		(end.tv_sec - start.tv_sec),
		(end.tv_nsec - start.tv_nsec) / 1000000);
	else
	    log(argv[1], (char *)"Total time: %ds and %dms",
		(end.tv_sec - start.tv_sec - 1),
		(1000 + end.tv_nsec /1000000) - start.tv_nsec / 1000000);
	
	return 0;
}
