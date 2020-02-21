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

#include "include/buildsys.h"

void BuildDescription::add(BuildUnit *bu)
{
	this->BUs.push_back(bu);
}

PackageFileUnit::PackageFileUnit(const std::string &fname, const std::string &_fname_short)
{
	this->uri = _fname_short;
	this->hash = hash_file(fname);
}

RequireFileUnit::RequireFileUnit(const std::string &fname, const std::string &_fname_short)
{
	this->uri = _fname_short;
	this->hash = hash_file(fname);
}

ExtractionInfoFileUnit::ExtractionInfoFileUnit(const std::string &fname)
{
	this->uri = fname;
	this->hash = hash_file(fname + ".new");
}

BuildInfoFileUnit::BuildInfoFileUnit(const std::string &fname, const std::string &_hash)
{
	this->uri = fname;
	this->hash = _hash;
}

OutputInfoFileUnit::OutputInfoFileUnit(const std::string &fname)
{
	this->uri = fname;
	this->hash = hash_file(this->uri);
}

void FeatureValueUnit::print(std::ostream &out)
{
	if(!this->WORLD->isIgnoredFeature(this->feature)) {
		out << this->type() << " " << this->feature << " " << this->value << std::endl;
	}
}
