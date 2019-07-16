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

#include <buildsys.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

BuildDir::BuildDir(Package * P)
{
	std::string gname = P->getNS()->getName();
	std::string pname = P->getName();
	std::string pwd = P->getWorld()->getWorkingDir();
	this->WORLD = P->getWorld();

	create_directories("output");
	create_directories("output/" + gname);
	create_directories("output/" + gname + "/staging");
	create_directories("output/" + gname + "/install");

	auto position = pname.rfind("/");
	if(position != std::string::npos) {
		std::string subpart = pname.substr(0, position);
		create_directories("output/" + gname + "/staging/" + subpart);
		create_directories("output/" + gname + "/install/" + subpart);
	}

	create_directories("output/" + gname + "/" + pname);

	this->path = pwd + "/output/" + gname + "/" + pname + "/work";
	create_directories(this->path);

	this->rpath = "output/" + gname + "/" + pname + "/work";

	this->new_path = pwd + "/output/" + gname + "/" + pname + "/new";
	create_directories(this->new_path);

	this->staging = pwd + "/output/" + gname + "/" + pname + "/staging";
	create_directories(this->staging);

	this->new_staging = pwd + "/output/" + gname + "/" + pname + "/new/staging";
	create_directories(this->new_staging);

	this->new_install = pwd + "/output/" + gname + "/" + pname + "/new/install";
	create_directories(this->new_install);
}

void BuildDir::clean()
{
	std::string cmd = string_format("rm -fr %s", this->path.c_str());
	std::system(cmd.c_str());

	int res = mkdir(this->path.c_str(), 0700);
	if(res < 0) {
		// We should complain here
	}
}

void BuildDir::cleanStaging()
{
	std::string cmd = string_format("rm -fr %s", this->staging.c_str());
	std::system(cmd.c_str());
}
