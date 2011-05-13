#include <buildsys.h>

World *buildsys::WORLD;

int main(int argc, char *argv[])
{
	struct timespec start, end;
	
	clock_gettime(CLOCK_REALTIME, &start);

	std::cout << "Buildsys (C++ version)" << std::endl;
	std::cout << "Built: " << __TIME__ << " " << __DATE__ << std::endl;

	
	if(argc <= 1)
	{
		error(std::string("At least 1 parameter is required"));
	}
	
	try {
		WORLD = new World();
	}
	catch(Exception &E)
	{
		error(E.error_msg());
	}
	
	try {
		interfaceSetup(WORLD->getLua());
	}
	catch(Exception &E)
	{
		error(E.error_msg());
	}
	
	// process arguments ...
	// first we take a list of package names to exclusevily build
	// this will over-ride any dependency checks and force them to be built
	// without first building their dependencies
	int a = 1;
	bool foundDashDash = false;
	while(a < argc && !foundDashDash)
	{
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
		try {
		
			// then we can preload the feature set
			while(a < argc)
			{
				WORLD->setFeature(argv[a]);
				a++;
			}
		}
		catch(Exception &E)
		{
			error(E.error_msg());
		}
	}
	try {
		WORLD->basePackage(argv[1]);
	}
	catch(Exception &E)
	{
		error(E.error_msg());
	}

	clock_gettime(CLOCK_REALTIME, &end);
	
	std::cout << "Total time: " << (end.tv_sec - start.tv_sec)  << "s and " << (end.tv_nsec - start.tv_nsec) / 1000 << " ms" << std::endl;
	
	return 0;
}
