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

std::string NameSpace::getStagingDir() const
{
	std::stringstream res;
	res << "output/" << this->getName() << "/staging";
	return res.str();
}

std::string NameSpace::getInstallDir() const
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

	for(const auto package : this->getPackages()) {
		if(package->getName() == _name) {
			return package;
		}
	}

	// Package not found, create it
	{
		// check that the dependency exists
		std::string lua_file;
		std::string lastPart = _name;

		size_t lastSlash = _name.rfind('/');
		if(lastSlash != std::string::npos) {
			lastPart = _name.substr(lastSlash + 1);
		}

		bool found = false;
		auto relative_fname = boost::format{"package/%1%/%2%.lua"} % _name % lastPart;
		for(const auto &ov : this->WORLD->getOverlays()) {
			lua_file = ov + "/" + relative_fname.str();
			if(filesystem::exists(lua_file)) {
				found = true;
				overlay = ov;
				break;
			}
		}

		if(!found) {
			throw CustomException("Package not found: " + _name);
		}

		file = lua_file;
		file_short = relative_fname.str();
	}

	Package *p =
	    new Package(this, _name, file_short, file, overlay, this->WORLD->getWorkingDir());
	this->packages.push_back(p);

	return p;
}

void NameSpace::addPackage(Package *p)
{
	std::unique_lock<std::mutex> lk(this->lock);
	this->packages.push_back(p);
}

World *NameSpace::getWorld()
{
	return this->WORLD;
}
