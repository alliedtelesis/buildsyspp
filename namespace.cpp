/******************************************************************************
 Copyright 2015 Allied Telesis Labs Ltd. All rights reserved.

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

NameSpace::~NameSpace()
{
	while(!this->packages.empty()) {
		Package *p = this->packages.front();
		this->packages.pop_front();
		delete p;
	}
}

std::list<Package *>::iterator NameSpace::packagesStart()
{
	return this->packages.begin();
}

std::list<Package *>::iterator NameSpace::packagesEnd()
{
	return this->packages.end();
}

std::string NameSpace::getStagingDir()
{
	std::stringstream res;
	res << "output/" << this->getName() << "/staging";
	return res.str();
}

std::string NameSpace::getInstallDir()
{
	std::stringstream res;
	res << "output/" << this->getName() << "/install";
	return res.str();
}

Package *NameSpace::findPackage(const std::string &_name)
{
	std::string file;
	std::string file_short;
	std::string overlay;

	std::unique_lock<std::mutex> lk(this->lock);

	auto iter = this->packagesStart();
	auto iterEnd = this->packagesEnd();
	for(; iter != iterEnd; iter++) {
		if((*iter)->getName().compare(_name) == 0) {
			return (*iter);
		}
	}

	// Package not found, create it
	{
		// check that the dependency exists
		std::string lua_file;
		char *dependPath = strdup(_name.c_str());
		char *lastPart = strrchr(dependPath, '/');
		bool found = false;

		if(lastPart == nullptr) {
			lastPart = strdup(dependPath);
		} else {
			lastPart = strdup(lastPart + 1);
		}

		std::string relative_fname =
		    string_format("package/%s/%s.lua", dependPath, lastPart);
		auto _iter = this->WORLD->overlaysStart();
		auto _iterEnd = this->WORLD->overlaysEnd();
		for(; _iter != _iterEnd; _iter++) {
			lua_file = string_format("%s/%s", _iter->c_str(), relative_fname.c_str());
			if(filesystem::exists(lua_file)) {
				found = true;
				overlay = *_iter;
				break;
			}
		}

		if(!found) {
			throw CustomException("Package not found: " + _name);
		}

		free(dependPath);
		free(lastPart);

		file = lua_file;
		file_short = relative_fname;
	}

	Package *p = new Package(this, _name, file_short, file, overlay);
	this->_addPackage(p);

	return p;
}

void NameSpace::_addPackage(Package *p)
{
	this->packages.push_back(p);
	if(p->getNS() != this) {
		p->setNS(this);
	}
}

void NameSpace::addPackage(Package *p)
{
	std::unique_lock<std::mutex> lk(this->lock);
	this->_addPackage(p);
}

void NameSpace::_removePackage(Package *p)
{
	this->packages.remove(p);
}

void NameSpace::removePackage(Package *p)
{
	std::unique_lock<std::mutex> lk(this->lock);
	this->_removePackage(p);
}

World *NameSpace::getWorld()
{
	return this->WORLD;
}
