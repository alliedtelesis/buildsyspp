/****************************************************************************************************************************
 Copyright 2015 Allied Telesis Labs Ltd. All rights reserved.
 
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the
distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
***************************************************************************************************************************/

#include <buildsys.h>

std::list<Package *>::iterator NameSpace::packagesStart()
{
	return this->packages.begin();
}

std::list<Package *>::iterator NameSpace::packagesEnd()
{
	return this->packages.end();
}


Package *NameSpace::findPackage(std::string name)
{
	std::string file;
	std::string overlay;
	std::list<Package *>::iterator iter = this->packagesStart();
	std::list<Package *>::iterator iterEnd = this->packagesEnd();
	for(; iter != iterEnd; iter++)
	{
		if((*iter)->getName().compare(name) == 0)
			return (*iter);
	}

	// Package not found, create it
	{
		// check that the dependency exists
		char *luaFile = NULL;
		char *dependPath  = strdup(name.c_str());
		char *lastPart = strrchr(dependPath, '/');
		bool found = false;

		if(lastPart == NULL)
		{
			lastPart = strdup(dependPath);
		} else {
			lastPart = strdup(lastPart+1);
		}

		string_list::iterator iter = WORLD->overlaysStart();
		string_list::iterator iterEnd = WORLD->overlaysEnd();
		for(; iter != iterEnd; iter++)
		{
			if(asprintf(&luaFile, "%s/%s/%s.lua", iter->c_str(), dependPath, lastPart) <= 0)
			{
				throw CustomException("Error with asprintf");
			}
			FILE *f = fopen(luaFile, "r");
			if(f == NULL)
			{
				log(name.c_str(), "Opening %s: %s", luaFile, strerror(errno));
			} else {
				found = true;
				overlay = *iter;
				fclose(f);
			}
			if(found)
			{
				break;
			}
			free(luaFile);
			luaFile = NULL;
		}

		if (!found)
		{
			throw CustomException("Failed opening Package");
		}


		free(dependPath);
		free(lastPart);

		file = std::string(luaFile);
		free(luaFile);
	}

	Package *p = new Package(name, file, overlay);
	this->addPackage(p);

	return p;
}

void NameSpace::addPackage (Package *p)
{
	this->packages.push_back(p);
	p->setNS(this);
};
