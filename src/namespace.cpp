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

	std::unique_lock<std::mutex> lk(this->lock);

	for(const auto &package : this->getPackages()) {
		if(package->getName() == _name) {
			return package.get();
		}
	}

	// Package not found, create it
	std::unique_ptr<Package> p = std::make_unique<Package>(this, _name);
	Package *ret = p.get();
	this->packages.push_back(std::move(p));

	return ret;
}

void NameSpace::addPackage(std::unique_ptr<Package> p)
{
	std::unique_lock<std::mutex> lk(this->lock);
	this->packages.push_back(std::move(p));
}

World *NameSpace::getWorld()
{
	return this->WORLD;
}
