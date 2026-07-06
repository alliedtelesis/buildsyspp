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

#include "include/buildsys.h"
#include <string>
#include <utility>
#include <vector>

void Extraction::add(std::unique_ptr<ExtractionUnit> eu)
{
	this->EUs.push_back(std::move(eu));
}

std::vector<FetchInfo> Extraction::sourceInfo()
{
	std::vector<FetchInfo> sources;
	for(auto &unit : this->EUs) {
		FetchInfo source = unit->fetchInfo();
		if(!source.uri.empty() || !source.path.empty()) {
			sources.push_back(source);
		}
	}
	return sources;
}

void Extraction::prepareNewExtractInfo(Package *P, BuildDir *bd)
{
	if(this->extracted) {
		P->log("Already extracted");
		return;
	}

	if(bd != nullptr) {
		// Create the new extraction info file
		std::string fname = bd->getPath() + "/.extraction.info.new";
		std::ofstream exInfo(fname);
		this->print(exInfo);
		exInfo.close();
	}
}

bool Extraction::extractionRequired(Package *P, BuildDir *bd) const
{
	if(this->extracted) {
		return false;
	}

	auto cmd = boost::format{"cmp -s %1%/.extraction.info.new %1%/.extraction.info"} %
	           bd->getPath();
	int res = std::system(cmd.str().c_str());
	return (res != 0 || P->isCodeUpdated());
}

bool Extraction::extract(Package *P)
{
	P->log("Extracting sources and patching");
	for(auto &unit : this->EUs) {
		if(!unit->extract(P)) {
			return false;
		}
	}

	// mv the file into the regular place. A silent failure here would leave a
	// stale .extraction.info, which feeds cache-key and should-build decisions,
	// so use filesystem::rename which throws on error.
	std::string oldfname = P->builddir()->getPath() + "/.extraction.info.new";
	std::string newfname = P->builddir()->getPath() + "/.extraction.info";
	filesystem::rename(oldfname, newfname);

	return true;
}

void Extraction::extractionInfo(BuildDir *bd, std::string *file_path,
                                std::string *hash) const
{
	*file_path = bd->getShortPath() + "/.extraction.info";
	*hash = hash_file(*file_path + ".new");
}

FetchInfo ExtractionUnit::fetchInfo()
{
	FetchInfo source;
	source.type = this->type();
	source.uri = this->URI();
	source.hash = this->HASH();
	if(!source.hash.empty()) {
		source.hash_algorithm = "SHA-256";
	}
	return source;
}

CompressedFileExtractionUnit::CompressedFileExtractionUnit(FetchUnit *f)
{
	this->fetch = f;
	this->uri = f->relative_path();
}

CompressedFileExtractionUnit::CompressedFileExtractionUnit(const std::string &fname)
{
	this->fetch = nullptr;
	this->uri = fname;
}

std::string CompressedFileExtractionUnit::HASH()
{
	if(this->hash.empty()) {
		if(this->fetch != nullptr) {
			this->hash = this->fetch->HASH();
		} else {
			this->hash = hash_file(this->uri);
		}
	}
	return this->hash;
}

FetchInfo CompressedFileExtractionUnit::fetchInfo()
{
	FetchInfo source;
	if(this->fetch != nullptr) {
		source = this->fetch->fetchInfo();
	} else {
		source = ExtractionUnit::fetchInfo();
	}
	source.type = this->type();
	return source;
}

bool TarExtractionUnit::extract(Package *P)
{
	PackageCmd pc(P->builddir()->getPath(), "tar");

	pc.addArg("xf");
	pc.addArg(P->getPwd() + "/" + this->uri);

	if(!pc.Run(P->getLogger())) {
		throw CustomException("Failed to extract file");
	}

	return true;
}

bool ZipExtractionUnit::extract(Package *P)
{
	PackageCmd pc(P->builddir()->getPath(), "unzip");

	pc.addArg("-o");
	pc.addArg(P->getPwd() + "/" + this->uri);

	if(!pc.Run(P->getLogger())) {
		throw CustomException("Failed to extract file");
	}

	return true;
}

PatchExtractionUnit::PatchExtractionUnit(int _level, const std::string &_patch_path,
                                         const std::string &patch_fname,
                                         const std::string &_fname_short)
{
	this->uri = patch_fname;
	this->fname_short = _fname_short;
	this->hash = hash_file(this->uri);
	this->level = _level;
	this->patch_path = _patch_path;
}

bool PatchExtractionUnit::extract(Package *P)
{
	PackageCmd pc_dry(this->patch_path, "patch");
	PackageCmd pc(this->patch_path, "patch");

	pc_dry.addArg("-p" + std::to_string(this->level));
	pc.addArg("-p" + std::to_string(this->level));

	pc_dry.addArg("-stN");
	pc.addArg("-stN");

	pc_dry.addArg("-i");
	pc.addArg("-i");

	auto pwd = P->getPwd();
	pc_dry.addArg(pwd + "/" + this->uri);
	pc.addArg(pwd + "/" + this->uri);

	pc_dry.addArg("--dry-run");

	if(!pc_dry.Run(P->getLogger())) {
		P->log("Patch file: " + this->uri);
		throw CustomException("Will fail to patch");
	}

	if(!pc.Run(P->getLogger())) {
		throw CustomException("Truely failed to patch");
	}

	return true;
}

FetchInfo PatchExtractionUnit::fetchInfo()
{
	FetchInfo source = ExtractionUnit::fetchInfo();
	source.path = this->patch_path;
	return source;
}

FileCopyExtractionUnit::FileCopyExtractionUnit(const std::string &fname,
                                               const std::string &_fname_short)
{
	this->uri = fname;
	this->fname_short = _fname_short;
	this->hash = hash_file(this->uri);
}

bool FileCopyExtractionUnit::extract(Package *P)
{
	std::string path = this->uri;
	PackageCmd pc(P->builddir()->getPath(), "cp");
	pc.addArg("-pRLuf");

	if(path.at(0) == '/') {
		pc.addArg(path);
	} else {
		std::string arg = P->getPwd() + "/" + path;
		pc.addArg(arg);
	}

	pc.addArg(".");

	if(!pc.Run(P->getLogger())) {
		throw CustomException("Failed to copy file");
	}

	return true;
}

FetchInfo FileCopyExtractionUnit::fetchInfo()
{
	FetchInfo source = ExtractionUnit::fetchInfo();
	source.path = this->fname_short;
	return source;
}

FetchedFileCopyExtractionUnit::FetchedFileCopyExtractionUnit(
    FetchUnit *_fetched, const std::string &_fname_short)
{
	this->fetched = _fetched;
	this->uri = _fetched->relative_path();
	this->fname_short = _fname_short;
	this->hash = std::string("");
}

std::string FetchedFileCopyExtractionUnit::HASH()
{
	if(this->hash.empty()) {
		this->hash = this->fetched->HASH();
	}
	return this->hash;
}

FetchInfo FetchedFileCopyExtractionUnit::fetchInfo()
{
	FetchInfo source = this->fetched->fetchInfo();
	source.type = this->type();
	source.path = this->fname_short;
	return source;
}

bool FetchedFileCopyExtractionUnit::extract(Package *P)
{
	PackageCmd pc(P->builddir()->getPath(), "cp");
	pc.addArg("-pRLuf");

	if(boost::algorithm::starts_with(this->uri, "/")) {
		pc.addArg(this->uri);
	} else {
		pc.addArg(P->getPwd() + "/" + this->uri);
	}

	pc.addArg(".");

	if(!pc.Run(P->getLogger())) {
		throw CustomException("Failed to copy file");
	}

	return true;
}
