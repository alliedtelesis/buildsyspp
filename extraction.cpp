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

void Extraction::add(ExtractionUnit *eu)
{
	this->EUs.push_back(eu);
}

void Extraction::prepareNewExtractInfo(Package *P, BuildDir *bd)
{
	if(this->extracted) {
		log(P, "Already extracted");
		return;
	}

	if(bd) {
		// Create the new extraction info file
		std::string fname = bd->getPath() + "/.extraction.info.new";
		std::ofstream exInfo(fname.c_str());
		this->print(exInfo);
		exInfo.close();
	}
}

bool Extraction::extractionRequired(Package *P, BuildDir *bd)
{
	if(this->extracted) {
		return false;
	}

	std::string cmd = string_format("cmp -s %s/.extraction.info.new %s/.extraction.info",
	                                bd->getPath().c_str(), bd->getPath().c_str());
	int res = std::system(cmd.c_str());

	// if there are changes,
	if(res != 0 || P->isCodeUpdated()) { // Extract our source code
		return true;
	}

	return false;
}

bool Extraction::extract(Package *P)
{
	log(P, "Extracting sources and patching");
	for(auto unit : this->EUs) {
		if(!unit->extract(P)) {
			return false;
		}
	}

	// mv the file into the regular place
	std::string oldfname = P->builddir()->getPath() + "/.extraction.info.new";
	std::string newfname = P->builddir()->getPath() + "/.extraction.info";
	rename(oldfname.c_str(), newfname.c_str());

	return true;
}

ExtractionInfoFileUnit *Extraction::extractionInfo(BuildDir *bd)
{
	std::string fname = bd->getShortPath() + "/.extraction.info";
	auto *ret = new ExtractionInfoFileUnit(fname);
	return ret;
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
		if(this->fetch) {
			this->hash = this->fetch->HASH();
		} else {
			this->hash = hash_file(this->uri);
		}
	}
	return this->hash;
}

bool TarExtractionUnit::extract(Package *P)
{
	PackageCmd pc(P->builddir()->getPath(), "tar");

	filesystem::create_directories("dl");

	pc.addArg("xf");
	pc.addArg(P->getWorld()->getWorkingDir() + "/" + this->uri);

	if(!pc.Run(P))
		throw CustomException("Failed to extract file");

	return true;
}

bool ZipExtractionUnit::extract(Package *P)
{
	PackageCmd pc(P->builddir()->getPath(), "unzip");

	filesystem::create_directories("dl");

	pc.addArg("-o");
	pc.addArg(P->getWorld()->getWorkingDir() + "/" + this->uri);

	if(!pc.Run(P))
		throw CustomException("Failed to extract file");

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
	PackageCmd pc_dry(this->patch_path.c_str(), "patch");
	PackageCmd pc(this->patch_path.c_str(), "patch");

	pc_dry.addArg("-p" + std::to_string(this->level));
	pc.addArg("-p" + std::to_string(this->level));

	pc_dry.addArg("-stN");
	pc.addArg("-stN");

	pc_dry.addArg("-i");
	pc.addArg("-i");

	auto pwd = P->getWorld()->getWorkingDir();
	pc_dry.addArg(pwd + "/" + this->uri);
	pc.addArg(pwd + "/" + this->uri);

	pc_dry.addArg("--dry-run");

	if(!pc_dry.Run(P)) {
		log(P->getName().c_str(), "Patch file: %s", this->uri.c_str());
		throw CustomException("Will fail to patch");
	}

	if(!pc.Run(P))
		throw CustomException("Truely failed to patch");

	return true;
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
		std::string arg = P->getWorld()->getWorkingDir() + "/" + path;
		pc.addArg(arg);
	}

	pc.addArg(".");

	if(!pc.Run(P)) {
		throw CustomException("Failed to copy file");
	}

	return true;
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

bool FetchedFileCopyExtractionUnit::extract(Package *P)
{
	PackageCmd pc(P->builddir()->getPath(), "cp");
	pc.addArg("-pRLuf");

	if(boost::algorithm::starts_with(this->uri, "/")) {
		pc.addArg(this->uri);
	} else {
		pc.addArg(P->getWorld()->getWorkingDir() + "/" + this->uri);
	}

	pc.addArg(".");

	if(!pc.Run(P)) {
		throw CustomException("Failed to copy file");
	}

	return true;
}
