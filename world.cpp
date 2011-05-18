#include <buildsys.h>

std::list<Package *>::iterator World::packagesStart()
{
	return this->packages.begin();
}
std::list<Package *>::iterator World::packagesEnd()
{
	return this->packages.end();
}

void World::setFeature(std::string key, std::string value, bool override)
{
	// look for this key
	if(features->find(key) != features->end())
	{
		if(override)
		{
			// only over-write the value if we are explicitly told to
			(*features)[key] = value;
		}
		return;
	}
	features->insert(std::pair<std::string, std::string>(key, value));
}

void World::setFeature(char *kv)
{
	char *eq = strchr(kv, '=');
	if(eq == NULL) throw CustomException("Features must be described as feature=value\n");
	char *temp = (char *)calloc(1, (eq - kv) + 1);
	if(temp == NULL) throw MemoryException();
	strncpy(temp, kv, (eq-kv));
	std::string key(temp);
	free(temp);
	eq++;
	temp = strdup(eq);
	if(temp == NULL) throw MemoryException();
	std::string value(temp);
	free(temp);
	this->setFeature(key, value, true);
}


std::string World::getFeature(std::string key)
{
	if(features->find(key) != features->end())
	{
		return (*features)[key];
	}
	throw NoKeyException();
}

bool World::basePackage(char *filename)
{
	this->p = findPackage(filename, filename);

	this->p->process();
	
	this->p->build();

	return true;
}

Package *World::findPackage(std::string name, std::string file)
{
	std::list<Package *>::iterator iter = this->packagesStart();
	std::list<Package *>::iterator iterEnd = this->packagesEnd();
	for(; iter != iterEnd; iter++)
	{
		if((*iter)->getName().compare(name) == 0)
			return (*iter);
	}
	Package *p = new Package(name, file);
	this->packages.push_back(p);
	return p;
}

bool World::isForced(std::string name)
{
	string_list::iterator fIt = this->forcedDeps->begin();
	string_list::iterator fEnd = this->forcedDeps->end();
	
	for(; fIt != fEnd; fIt++)
	{
		if((*fIt).compare(name)==0) return true;
	}
	return false;
}
