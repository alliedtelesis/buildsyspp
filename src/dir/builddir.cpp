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

#include "dir/builddir.hpp"
#include <filesystem>

using namespace buildsys;
namespace filesystem = std::filesystem;

/**
 * Create a build directory.
 *
 * @param pwd - The current working directory
 * @param gname - The namespace name of the package
 * @param pname - The package name
 */
BuildDir::BuildDir(const std::string &pwd, const std::string &gname,
                   const std::string &pname)
{
	filesystem::create_directories("output/" + gname);
	filesystem::create_directories("output/" + gname + "/staging");
	filesystem::create_directories("output/" + gname + "/install");

	auto position = pname.rfind('/');
	if(position != std::string::npos) {
		std::string subpart = pname.substr(0, position);
		filesystem::create_directories("output/" + gname + "/staging/" + subpart);
		filesystem::create_directories("output/" + gname + "/install/" + subpart);
	}

	filesystem::create_directories("output/" + gname + "/" + pname);

	this->path = pwd + "/output/" + gname + "/" + pname + "/work";
	filesystem::create_directories(this->path);

	this->rpath = "output/" + gname + "/" + pname + "/work";

	this->new_path = pwd + "/output/" + gname + "/" + pname + "/new";
	filesystem::create_directories(this->new_path);

	this->staging = pwd + "/output/" + gname + "/" + pname + "/staging";
	filesystem::create_directories(this->staging);

	this->new_staging = pwd + "/output/" + gname + "/" + pname + "/new/staging";
	filesystem::create_directories(this->new_staging);

	this->new_install = pwd + "/output/" + gname + "/" + pname + "/new/install";
	filesystem::create_directories(this->new_install);
}

/**
 * Return the full path to this directory.
 */
const std::string &BuildDir::getPath() const
{
	return this->path;
}

/**
 * Get the short version (relative only) of the working directory.
 */
const std::string &BuildDir::getShortPath() const
{
	return this->rpath;
}

/**
 * Return the full path to the staging directory.
 */
const std::string &BuildDir::getStaging() const
{
	return this->staging;
}

/**
 * Return the full path to the new directory.
 */
const std::string &BuildDir::getNewPath() const
{
	return this->new_path;
}

/**
 * Return the full path to the new staging directory.
 */
const std::string &BuildDir::getNewStaging() const
{
	return this->new_staging;
}

/**
 * Return the full path to the new install directory.
 */
const std::string &BuildDir::getNewInstall() const
{
	return this->new_install;
}

/**
 * Remove all of the work directory contents.
 */
void BuildDir::clean() const
{
	filesystem::remove_all(this->path);
	filesystem::create_directories(this->path);
}

/**
 * Remove all of the staging directory contents.
 */
void BuildDir::cleanStaging() const
{
	filesystem::remove_all(this->staging);
}
