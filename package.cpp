#include <buildsys.h>

BuildDir *Package::builddir()
{
	if(this->bd == NULL)
	{
		this->bd = new BuildDir(this->name);
	}
	
	return this->bd;
}
