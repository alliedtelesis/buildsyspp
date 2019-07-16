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

bool Extraction::add(ExtractionUnit * eu)
{
	ExtractionUnit **t = this->EUs;
	this->EU_count++;
	this->EUs =
	    (ExtractionUnit **) realloc(t, sizeof(ExtractionUnit *) * this->EU_count);
	if(this->EUs == NULL) {
		this->EUs = t;
		this->EU_count--;
		return false;
	}
	this->EUs[this->EU_count - 1] = eu;
	return true;
}

void Extraction::prepareNewExtractInfo(Package * P, BuildDir * bd)
{
	if(this->extracted) {
		log(P, "Already extracted");
		return;
	}

	if(bd) {
		// Create the new extraction info file
		std::string fname = string_format("%s/.extraction.info.new", bd->getPath());
		std::ofstream exInfo(fname.c_str());
		this->print(exInfo);
		exInfo.close();
	}
}

bool Extraction::extractionRequired(Package * P, BuildDir * bd)
{
	if(this->extracted) {
		return false;
	}

	std::string cmd =
	    string_format("cmp -s %s/.extraction.info.new %s/.extraction.info",
			  bd->getPath(), bd->getPath());
	int res = std::system(cmd.c_str());

	// if there are changes,
	if(res != 0 || P->isCodeUpdated()) {	// Extract our source code
		return true;
	}

	return false;
}

bool Extraction::extract(Package * P, BuildDir * bd)
{
	log(P, "Extracting sources and patching");
	for(size_t i = 0; i < this->EU_count; i++) {
		if(!EUs[i]->extract(P, bd))
			return false;
	}

	// mv the file into the regular place
	std::string oldfname = string_format("%s/.extraction.info.new", bd->getPath());
	std::string newfname = string_format("%s/.extraction.info", bd->getPath());
	rename(oldfname.c_str(), newfname.c_str());

	return true;
};

ExtractionInfoFileUnit *Extraction::extractionInfo(Package * P, BuildDir * bd)
{
	std::string fname = string_format("%s/.extraction.info", bd->getShortPath());
	ExtractionInfoFileUnit *ret = new ExtractionInfoFileUnit(fname);
	return ret;
}

CompressedFileExtractionUnit::CompressedFileExtractionUnit(FetchUnit * f)
{
	this->fetch = f;
	this->uri = f->relative_path();
}

CompressedFileExtractionUnit::CompressedFileExtractionUnit(const std::string & fname)
{
	this->fetch = NULL;
	this->uri = fname;
}

std::string CompressedFileExtractionUnit::HASH()
{
	if(this->hash.empty()) {
		if(this->fetch) {
			this->hash = std::string(this->fetch->HASH());
		} else {
			this->hash = hash_file(this->uri);
		}
	}
	return this->hash;
};


bool TarExtractionUnit::extract(Package * P, BuildDir * bd)
{
	std::unique_ptr < PackageCmd > pc(new PackageCmd(bd->getPath(), "tar"));

	create_directories("dl");

	pc->addArg("xf");
	pc->addArg(P->getWorld()->getWorkingDir() + "/" + this->uri);

	if(!pc->Run(P))
		throw CustomException("Failed to extract file");

	return true;
}

bool ZipExtractionUnit::extract(Package * P, BuildDir * bd)
{
	std::unique_ptr < PackageCmd > pc(new PackageCmd(bd->getPath(), "unzip"));

	create_directories("dl");

	pc->addArg("-o");
	pc->addArg(P->getWorld()->getWorkingDir() + "/" + this->uri);

	if(!pc->Run(P))
		throw CustomException("Failed to extract file");

	return true;
}


PatchExtractionUnit::PatchExtractionUnit(int level, const std::string & patch_path,
					 const std::string & patch_fname,
					 const std::string & fname_short)
{
	this->uri = patch_fname;
	this->fname_short = fname_short;
	this->hash = hash_file(this->uri);
	this->level = level;
	this->patch_path = patch_path;
}

bool PatchExtractionUnit::extract(Package * P, BuildDir * bd)
{
	std::unique_ptr < PackageCmd >
	    pc_dry(new PackageCmd(this->patch_path.c_str(), "patch"));
	std::unique_ptr < PackageCmd >
	    pc(new PackageCmd(this->patch_path.c_str(), "patch"));

	pc_dry->addArg("-p" + std::to_string(this->level));
	pc->addArg("-p" + std::to_string(this->level));

	pc_dry->addArg("-stN");
	pc->addArg("-stN");

	pc_dry->addArg("-i");
	pc->addArg("-i");

	auto pwd = P->getWorld()->getWorkingDir();
	pc_dry->addArg(pwd + "/" + this->uri);
	pc->addArg(pwd + "/" + this->uri);

	pc_dry->addArg("--dry-run");

	if(!pc_dry->Run(P)) {
		log(P->getName().c_str(), "Patch file: %s", this->uri.c_str());
		throw CustomException("Will fail to patch");
	}

	if(!pc->Run(P))
		throw CustomException("Truely failed to patch");

	return true;
}

FileCopyExtractionUnit::FileCopyExtractionUnit(const std::string & fname,
					       const std::string & fname_short)
{
	this->uri = fname;
	this->fname_short = fname_short;
	this->hash = hash_file(this->uri);
}

bool FileCopyExtractionUnit::extract(Package * P, BuildDir * bd)
{
	std::string path = this->uri;
	std::unique_ptr < PackageCmd > pc(new PackageCmd(bd->getPath(), "cp"));
	pc->addArg("-pRLuf");

	if(path.at(0) == '/') {
		pc->addArg(path);
	} else {
		std::string arg = P->getWorld()->getWorkingDir() + "/" + path;
		pc->addArg(arg);
	}

	pc->addArg(".");

	if(!pc->Run(P)) {
		throw CustomException("Failed to copy file");
	}

	return true;
}

FetchedFileCopyExtractionUnit::FetchedFileCopyExtractionUnit(FetchUnit * fetched,
							     const char *fname_short)
{
	this->fetched = fetched;
	this->uri = fetched->relative_path();
	this->fname_short = std::string(fname_short);
	this->hash = std::string("");
}

std::string FetchedFileCopyExtractionUnit::HASH()
{
	if(this->hash.empty()) {
		this->hash = std::string(this->fetched->HASH());
	}
	return this->hash;
}

bool FetchedFileCopyExtractionUnit::extract(Package * P, BuildDir * bd)
{
	const char *path = this->uri.c_str();
	std::unique_ptr < PackageCmd > pc(new PackageCmd(bd->getPath(), "cp"));
	pc->addArg("-pRLuf");

	if(path[0] == '/') {
		pc->addArg(path);
	} else {
		pc->addArg(P->getWorld()->getWorkingDir() + "/" + path);
	}

	pc->addArg(".");

	if(!pc->Run(P)) {
		throw CustomException("Failed to copy file");
	}

	return true;
}
