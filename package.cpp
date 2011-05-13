#include <buildsys.h>

BuildDir *Package::builddir()
{
	if(this->bd == NULL)
	{
		this->bd = new BuildDir(this->name);
	}
	
	return this->bd;
}

bool Package::process()
{
	if(this->visiting == true)
	{
		std::cout << this->name << ": dependency loop!!!" << std::endl;
		return false;
	}
	if(this->processed == true) return true;
	
	this->visiting = true;

	std::cout << "Processing: " << this->name << " (" << this->file << ")" << std::endl;

	WORLD->getLua()->setGlobal(std::string("P"), this);

	WORLD->getLua()->processFile(file.c_str());

	std::cout << "Done: " << this->name << std::endl;

	this->processed = true;

	std::list<Package *>::iterator iter = this->depends.begin();
	std::list<Package *>::iterator end = this->depends.end();

	for(; iter != end; iter++)
	{
		if(!(*iter)->process())
		{
			std::cout << this->name << ": dependency failed" << std::endl;
			return false;
		}
	}

	this->visiting = false;
	return true;
}
