/******************************************************************************
 Copyright 2019 Allied Telesis Labs Ltd. All rights reserved.
 
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

bool BuildDescription::add(BuildUnit * bu)
{
	BuildUnit **t = this->BUs;
	this->BU_count++;
	this->BUs = (BuildUnit **) realloc(t, sizeof(BuildUnit *) * this->BU_count);
	if(this->BUs == NULL) {
		this->BUs = t;
		this->BU_count--;
		return false;
	}
	this->BUs[this->BU_count - 1] = bu;
	return true;
}

PackageFileUnit::PackageFileUnit(const char *fname)
{
	this->uri = std::string(fname);
	char *Hash = hash_file(fname);
	this->hash = std::string(Hash);
	free(Hash);
}

RequireFileUnit::RequireFileUnit(const char *fname)
{
	this->uri = std::string(fname);
	char *Hash = hash_file(fname);
	this->hash = std::string(Hash);
	free(Hash);
}

ExtractionInfoFileUnit::ExtractionInfoFileUnit(const char *fname)
{
	this->uri = std::string(fname);
	char *new_fname = NULL;
	asprintf(&new_fname, "%s.new", fname);
	char *Hash = hash_file(new_fname);
	this->hash = std::string(Hash);
	free(Hash);
	free(new_fname);
}

BuildInfoFileUnit::BuildInfoFileUnit(const char *fname, std::string hash)
{
	this->uri = std::string(fname);
	this->hash = hash;
}

OutputInfoFileUnit::OutputInfoFileUnit(const char *fname)
{
	this->uri = std::string(fname);
	char *Hash = hash_file(fname);
	this->hash = std::string(Hash);
	free(Hash);
}

bool FeatureValueUnit::print(std::ostream & out)
{
	if(!WORLD->isIgnoredFeature(this->feature)) {
		out << this->type() << " " << this->
		    feature << " " << this->value << std::endl;
	}
	return true;
}
