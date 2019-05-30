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
		char *exinfoFname = NULL;
		asprintf(&exinfoFname, "%s/.extraction.info.new", bd->getPath());
		std::ofstream exInfo(exinfoFname);
		this->print(exInfo);
		exInfo.close();
		free(exinfoFname);
	}
}

bool Extraction::extractionRequired(Package * P, BuildDir * bd)
{
	if(this->extracted) {
		return false;
	}

	char *cmd = NULL;
	asprintf(&cmd, "cmp -s %s/.extraction.info.new %s/.extraction.info",
		 bd->getPath(), bd->getPath());
	int res = system(cmd);
	free(cmd);
	cmd = NULL;

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
	char *oldfname = NULL;
	char *newfname = NULL;
	asprintf(&oldfname, "%s/.extraction.info.new", bd->getPath());
	asprintf(&newfname, "%s/.extraction.info", bd->getPath());
	rename(oldfname, newfname);
	free(oldfname);
	free(newfname);

	return true;
};

ExtractionInfoFileUnit *Extraction::extractionInfo(Package * P, BuildDir * bd)
{
	char *extractionInfoFname = NULL;
	asprintf(&extractionInfoFname, "%s/.extraction.info", bd->getShortPath());
	ExtractionInfoFileUnit *ret = new ExtractionInfoFileUnit(extractionInfoFname);
	free(extractionInfoFname);
	return ret;
}

CompressedFileExtractionUnit::CompressedFileExtractionUnit(FetchUnit * f)
{
	this->fetch = f;
	this->uri = f->relative_path();
}

CompressedFileExtractionUnit::CompressedFileExtractionUnit(const char *fname)
{
	this->fetch = NULL;
	this->uri = std::string(fname);
}

std::string CompressedFileExtractionUnit::HASH()
{
	if(this->hash == NULL) {
		if(this->fetch) {
			this->hash = new std::string(this->fetch->HASH());
		} else {
			char *Hash = hash_file(this->uri.c_str());
			this->hash = new std::string(Hash);
			free(Hash);
		}
	}
	return *this->hash;
};


bool TarExtractionUnit::extract(Package * P, BuildDir * bd)
{
	std::unique_ptr < PackageCmd > pc(new PackageCmd(bd->getPath(), "tar"));

	int res = mkdir("dl", 0700);
	if((res < 0) && (errno != EEXIST)) {
		throw CustomException("Error: Creating download directory");
	}
	pc->addArg("xf");
	pc->addArgFmt("%s/%s", P->getWorld()->getWorkingDir()->c_str(), this->uri.c_str());

	if(!pc->Run(P))
		throw CustomException("Failed to extract file");

	return true;
}

bool ZipExtractionUnit::extract(Package * P, BuildDir * bd)
{
	std::unique_ptr < PackageCmd > pc(new PackageCmd(bd->getPath(), "unzip"));

	int res = mkdir("dl", 0700);
	if((res < 0) && (errno != EEXIST)) {
		throw CustomException("Error: Creating download directory");
	}

	pc->addArg("-o");
	pc->addArgFmt("%s/%s", P->getWorld()->getWorkingDir()->c_str(), this->uri.c_str());

	if(!pc->Run(P))
		throw CustomException("Failed to extract file");

	return true;
}


PatchExtractionUnit::PatchExtractionUnit(int level, const char *pp, const char *uri,
					 const char *fname_short)
{
	this->uri = std::string(uri);
	this->fname_short = std::string(fname_short);
	char *Hash = hash_file(uri);
	this->hash = new std::string(Hash);
	free(Hash);
	this->level = level;
	this->patch_path = strdup(pp);
}

bool PatchExtractionUnit::extract(Package * P, BuildDir * bd)
{
	std::unique_ptr < PackageCmd > pc_dry(new PackageCmd(this->patch_path, "patch"));
	std::unique_ptr < PackageCmd > pc(new PackageCmd(this->patch_path, "patch"));

	pc_dry->addArgFmt("-p%i", this->level);
	pc->addArgFmt("-p%i", this->level);

	pc_dry->addArg("-stN");
	pc->addArg("-stN");

	pc_dry->addArg("-i");
	pc->addArg("-i");

	const char *pwd = P->getWorld()->getWorkingDir()->c_str();
	pc_dry->addArgFmt("%s/%s", pwd, this->uri.c_str());
	pc->addArgFmt("%s/%s", pwd, this->uri.c_str());

	pc_dry->addArg("--dry-run");

	if(!pc_dry->Run(P)) {
		log(P->getName().c_str(), "Patch file: %s", this->uri.c_str());
		throw CustomException("Will fail to patch");
	}

	if(!pc->Run(P))
		throw CustomException("Truely failed to patch");

	return true;
}

FileCopyExtractionUnit::FileCopyExtractionUnit(const char *fname, const char *fname_short)
{
	this->uri = std::string(fname);
	this->fname_short = std::string(fname_short);
	char *Hash = hash_file(fname);
	this->hash = new std::string(Hash);
	free(Hash);
}

bool FileCopyExtractionUnit::extract(Package * P, BuildDir * bd)
{
	const char *path = this->uri.c_str();
	std::unique_ptr < PackageCmd > pc(new PackageCmd(bd->getPath(), "cp"));
	pc->addArg("-pRLuf");

	if(path[0] == '/') {
		pc->addArg(path);
	} else {
		pc->addArgFmt("%s/%s", P->getWorld()->getWorkingDir()->c_str(), path);
	}

	pc->addArg(".");

	if(!pc->Run(P)) {
		throw CustomException("Failed to copy file");
	}

	return true;
}
