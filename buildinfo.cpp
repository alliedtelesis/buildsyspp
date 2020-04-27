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

#include "buildinfo.hpp"
#include "hash.hpp"

using namespace buildsys;

//! A feature/value as part of the build step
class FeatureValueUnit : public BuildUnit
{
private:
	const std::string feature;
	const std::string value;
	const bool ignored;

public:
	FeatureValueUnit(bool _ignored, std::string _feature, std::string _value)
		: feature(std::move(_feature)), value(std::move(_value)), ignored(_ignored)
	{
	}
	void print(std::ostream &out) override
	{
		if(!this->ignored) {
			out << this->type() << " " << this->feature << " " << this->value
				<< std::endl;
		}
	}
	std::string type() override
	{
		return std::string("FeatureValue");
	}
};


//! A feature that is nil as part of the build step
class FeatureNilUnit : public BuildUnit
{
private:
	const std::string feature;

public:
	explicit FeatureNilUnit(std::string _feature) : feature(std::move(_feature))
	{
	}
	void print(std::ostream &out) override
	{
		out << this->type() << " " << this->feature << std::endl;
	}
	std::string type() override
	{
		return std::string("FeatureNil");
	}
};

//! A lua package file as part of the build step
class PackageFileUnit : public BuildUnit
{
private:
	std::string uri;  //!< URI of this package file
	std::string hash; //!< Hash of this package file
public:
	PackageFileUnit(const std::string &fname, const std::string &_fname_short);
	void print(std::ostream &out) override
	{
		out << this->type() << " " << this->uri << " " << this->hash << std::endl;
	}
	std::string type() override
	{
		return std::string("PackageFile");
	}
};

//! A lua require file as part of the build step
class RequireFileUnit : public BuildUnit
{
private:
	std::string uri;  //!< URI of this package file
	std::string hash; //!< Hash of this package file
public:
	RequireFileUnit(const std::string &fname, const std::string &_fname_short);
	void print(std::ostream &out) override
	{
		out << this->type() << " " << this->uri << " " << this->hash << std::endl;
	}
	std::string type() override
	{
		return std::string("RequireFile");
	}
};

//! A build info file as part of the build step
class BuildInfoFileUnit : public BuildUnit
{
private:
	std::string uri;  //!< URI of this build info file
	std::string hash; //!< Hash of this build info file
public:
	BuildInfoFileUnit(const std::string &fname, const std::string &_hash);
	void print(std::ostream &out) override
	{
		out << this->type() << " " << this->uri << " " << this->hash << std::endl;
	}
	std::string type() override
	{
		return std::string("BuildInfoFile");
	}
};

//! An output (hash) info file as part of the build step
class OutputInfoFileUnit : public BuildUnit
{
private:
	std::string uri;  //!< URI of this output info file
	std::string hash; //!< Hash of this output info file
public:
	OutputInfoFileUnit(const std::string &fname, const std::string &hash);
	void print(std::ostream &out) override
	{
		out << this->type() << " " << this->uri << " " << this->hash << std::endl;
	}
	std::string type() override
	{
		return std::string("OutputInfoFile");
	}
};

//! An extraction info file as part of the build step
class ExtractionInfoFileUnit : public BuildUnit
{
private:
	std::string uri;  //!< URI of this extraction info file
	std::string hash; //!< Hash of this extraction info file
public:
	ExtractionInfoFileUnit(const std::string &fname, const std::string &_hash);
	void print(std::ostream &out) override
	{
		out << this->type() << " " << this->uri << " " << this->hash << std::endl;
	}
	std::string type() override
	{
		return std::string("ExtractionInfoFile");
	}
};

void BuildDescription::add_feature_value(bool _ignored, std::string _feature, std::string _value)
{
	this->BUs.push_back(std::make_unique<FeatureValueUnit>(_ignored, _feature, _value));
}

void BuildDescription::add_nil_feature_value(std::string _feature)
{
	this->BUs.push_back(std::make_unique<FeatureNilUnit>(_feature));
}

void BuildDescription::add_package_file(const std::string &fname, const std::string &_fname_short)
{
	this->BUs.push_back(std::make_unique<PackageFileUnit>(fname, _fname_short));
}

void BuildDescription::add_require_file(const std::string &fname, const std::string &_fname_short)
{
	this->BUs.push_back(std::make_unique<RequireFileUnit>(fname, _fname_short));
}

void BuildDescription::add_output_info_file(const std::string &fname,
                                            const std::string &_hash)
{
	this->BUs.push_back(std::make_unique<OutputInfoFileUnit>(fname, _hash));
}

void BuildDescription::add_build_info_file(const std::string &fname,
                                           const std::string &_hash)
{
	this->BUs.push_back(std::make_unique<BuildInfoFileUnit>(fname, _hash));
}

void BuildDescription::add_extraction_info_file(const std::string &fname,
                                                const std::string &_hash)
{
	this->BUs.push_back(std::make_unique<ExtractionInfoFileUnit>(fname, _hash));
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

ExtractionInfoFileUnit::ExtractionInfoFileUnit(const std::string &fname,
                                               const std::string &_hash)
{
	this->uri = fname;
	this->hash = _hash;
}

BuildInfoFileUnit::BuildInfoFileUnit(const std::string &fname, const std::string &_hash)
{
	this->uri = fname;
	this->hash = _hash;
}

OutputInfoFileUnit::OutputInfoFileUnit(const std::string &fname, const std::string &_hash)
{
	this->uri = fname;
	this->hash = _hash;
}
