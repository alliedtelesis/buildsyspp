/******************************************************************************
 Copyright 2016 Allied Telesis Labs Ltd. All rights reserved.
 
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

bool DownloadFetch::fetch(Package * P, BuildDir * d)
{
	char *location = strdup(this->fetch_uri.c_str());

	int res = mkdir("dl", 0700);
	if((res < 0) && (errno != EEXIST)) {
		throw CustomException("Error: Creating download directory");
	}

	bool get = true;
	char *fname = strrchr(location, '/');
	if(fname != NULL) {
		fname++;
		get = false;
		char *t = fname;
		if(decompress) {
			fname = strdup(fname);
			char *ext = strrchr(fname, '.');
			if(ext != NULL)
				ext[0] = '\0';
		}
		char *fpath = NULL;
		asprintf(&fpath, "%s/dl/%s", WORLD->getWorkingDir()->c_str(), fname);
		FILE *f = fopen(fpath, "r");
		if(f == NULL) {
			get = true;
		} else {
			fclose(f);
		}
		free(fpath);
		if(decompress)
			free(fname);
		fname = t;
	}
	if(get) {
		bool localCacheHit;
		//Attempt to get file from local tarball cache if one is configured.
		if(WORLD->haveTarballCache()) {
			PackageCmd *pc = new PackageCmd("dl", "wget");
			char *url = NULL;
			asprintf(&url, "%s/%s", WORLD->tarballCache().c_str(), fname);
			pc->addArg(url);
			free(url);
			localCacheHit = pc->Run(P);
			delete pc;
		}
		//If we didn't get the file from the local cache, look upstream.
		if(!localCacheHit) {
			PackageCmd *pc = new PackageCmd("dl", "wget");
			pc->addArg(location);
			if(!pc->Run(P))
				throw CustomException("Failed to fetch file");
			delete pc;
		}
		if(decompress) {
			// We want to run a command on this file
			char *cmd = NULL;
			char *ext = strrchr(fname, '.');
			if(ext == NULL) {
				log(P->getName().c_str(),
				    "Could not guess decompression based on extension: %s\n",
				    fname);
			}

			if(strcmp(ext, ".bz2") == 0) {
				asprintf(&cmd, "bunzip2 -d dl/%s", fname);
			} else if(strcmp(ext, ".gz") == 0) {
				asprintf(&cmd, "gunzip -d dl/%s", fname);
			}
			system(cmd);
			free(cmd);
		}
	}
	free(location);
	return true;
}


bool LinkFetch::fetch(Package * P, BuildDir * d)
{
	char *location = strdup(this->fetch_uri.c_str());
	PackageCmd *pc = new PackageCmd(d->getPath(), "ln");
	pc->addArg("-sf");
	char *l = P->relative_fetch_path(location);
	pc->addArg(l);
	pc->addArg(".");
	if(!pc->Run(P)) {
		// An error occured, try remove the file, then relink
		PackageCmd *rmpc = new PackageCmd(d->getPath(), "rm");
		rmpc->addArg("-fr");
		char *l2 = strrchr(l, '/');
		if(l2[1] == '\0') {
			l2[0] = '\0';
			l2 = strrchr(l, '/');
		}
		l2++;
		rmpc->addArg(l2);
		if(!rmpc->Run(P))
			throw
			    CustomException
			    ("Failed to ln (symbolically), could not remove target first");
		if(!pc->Run(P))
			throw
			    CustomException
			    ("Failed to ln (symbolically), even after removing target first");
		delete rmpc;
	}
	free(l);
	delete pc;
	free(location);
	return true;
}

bool CopyFetch::fetch(Package * P, BuildDir * d)
{
	char *location = strdup(this->fetch_uri.c_str());
	PackageCmd *pc = new PackageCmd(d->getPath(), "cp");
	pc->addArg("-dpRuf");
	char *l = P->absolute_fetch_path(location);
	pc->addArg(l);
	pc->addArg(".");
	free(l);
	if(!pc->Run(P))
		throw CustomException("Failed to copy (recursively)");
	delete pc;
	fprintf(stderr, "Copied data in, considering code updated\n");
	P->setCodeUpdated();
	free(location);
	return true;
}

bool Fetch::add(FetchUnit * fu)
{
	FetchUnit **t = this->FUs;
	this->FU_count++;
	this->FUs = (FetchUnit **) realloc(t, sizeof(FetchUnit *) * this->FU_count);
	if(this->FUs == NULL) {
		this->FUs = t;
		this->FU_count--;
		return false;
	}
	this->FUs[this->FU_count - 1] = fu;
	return true;
}
