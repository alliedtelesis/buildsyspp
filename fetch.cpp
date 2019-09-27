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

/* This is the full name of the file to be downloaded */
std::string DownloadFetch::full_name()
{
	auto fname = std::string("");

	if(this->filename.empty()) {
		auto position = this->fetch_uri.rfind("/");
		if(position != std::string::npos) {
			fname = this->fetch_uri.substr(position + 1);
		}
	} else {
		fname = this->filename;
	}

	return fname;
}

/* This is the final name, without any compressed extension */
std::string DownloadFetch::final_name()
{
	auto full_name = this->full_name();
	auto ret = full_name;

	if(this->decompress) {
		auto position = full_name.rfind(".");
		if(position != std::string::npos) {
			ret = full_name.substr(0, position);
		}
	}

	return ret;
}


bool DownloadFetch::fetch(BuildDir * d)
{
	filesystem::create_directories("dl");

	bool get = false;

	std::string fullname = this->full_name();
	std::string fname = this->final_name();

	/* Hold a lock while we download this file
	 * Also checks for conflicting hashes for the same file
	 */
	DLObject *dlobj = d->getWorld()->findDLObject(fullname);
	if(dlobj == NULL) {
		log(this->P, "Failed to get the DLObject for %s\n", fullname.c_str());
		return false;
	}

	std::unique_lock<std::mutex> lk (dlobj->getLock ());

	if(this->hash.length() != 0) {
		if(dlobj->HASH().length() != 0) {
			if(dlobj->HASH().compare(this->hash) != 0) {
				log(this->P,
				    "Another package has already downloaded %s with hash %s (but we need %s)\n",
				    fullname.c_str(), dlobj->HASH().c_str(),
				    this->hash.c_str());
				return false;
			}
		}
	}

	{
		std::string fpath = string_format("%s/dl/%s",
						  d->getWorld()->getWorkingDir().c_str(),
						  fname.c_str());
		FILE *f = fopen(fpath.c_str(), "r");
		if(f == NULL) {
			get = true;
		} else {
			fclose(f);
		}
	}

	if(get) {
		bool localCacheHit = false;
		//Attempt to get file from local tarball cache if one is configured.
		if(d->getWorld()->haveTarballCache()) {
			PackageCmd pc("dl", "wget");
			std::string url = string_format("%s/%s",
							d->getWorld()->
							tarballCache().c_str(),
							fname.c_str());
			pc.addArg(url);
			pc.addArg("-O" + fullname);
			localCacheHit = pc.Run(this->P);
		}
		//If we didn't get the file from the local cache, look upstream.
		if(!localCacheHit) {
			PackageCmd pc("dl", "wget");
			pc.addArg(this->fetch_uri.c_str());
			pc.addArg("-O" + fullname);
			if(!pc.Run(this->P))
				throw CustomException("Failed to fetch file");
		}
		if(decompress) {
			// We want to run a command on this file
			std::string cmd = std::string();
			char *filename = strdup(fname.c_str());
			char *ext = strrchr(filename, '.');
			if(ext == NULL) {
				log(P->getName().c_str(),
				    "Could not guess decompression based on extension: %s\n",
				    fname.c_str());
			}

			if(strcmp(ext, ".bz2") == 0) {
				cmd = string_format("bunzip2 -d dl/%s", fullname.c_str());
			} else if(strcmp(ext, ".gz") == 0) {
				cmd = string_format("gunzip -d dl/%s", fullname.c_str());
			}
			std::system(cmd.c_str());
			free(filename);
		}
	}

	bool ret = true;

	if(this->hash.length() != 0) {
		std::string fpath = string_format("%s/dl/%s",
						  d->getWorld()->getWorkingDir().c_str(),
						  this->final_name().c_str());
		std::string hash = hash_file(fpath);

		if(strcmp(this->hash.c_str(), hash.c_str()) != 0) {
			log(this->P,
			    "Hash mismatched for %s\n(committed to %s, providing %s)",
			    this->final_name().c_str(), this->hash.c_str(), hash.c_str());
			ret = false;
		}
	}

	return ret;
}


std::string DownloadFetch::HASH()
{
	/* Check if the package contains pre-computed hashes */
	std::string hash = P->getFileHash(this->final_name());
	/* Otherwise fetch and calculate the hash */
	if(hash.empty()) {
		log(P, "No hash for %s in package/%s/Digest", this->final_name().c_str(),
		    P->getName().c_str());
		throw CustomException("Missing hash " + P->getName());
	}
	this->hash = hash;
	return this->hash;
}

bool LinkFetch::fetch(BuildDir * d)
{
	PackageCmd pc(d->getPath(), "ln");
	pc.addArg("-sf");
	std::string l = P->relative_fetch_path(this->fetch_uri);
	pc.addArg(l.c_str());
	pc.addArg(".");
	if(!pc.Run(this->P)) {
		// An error occured, try remove the file, then relink
		PackageCmd rmpc(d->getPath(), "rm");
		rmpc.addArg("-fr");
		char *l_copy = strdup(l.c_str());
		char *l2 = strrchr(l_copy, '/');
		if(l2[1] == '\0') {
			l2[0] = '\0';
			l2 = strrchr(l_copy, '/');
		}
		l2++;
		rmpc.addArg(l2);
		free(l_copy);
		if(!rmpc.Run(this->P))
			throw
			    CustomException
			    ("Failed to ln (symbolically), could not remove target first");
		if(!pc.Run(this->P))
			throw
			    CustomException
			    ("Failed to ln (symbolically), even after removing target first");
	}
	return true;
}

std::string LinkFetch::HASH()
{
	return std::string("");
}

std::string LinkFetch::relative_path()
{
	auto position = this->fetch_uri.rfind("/");
	auto path = std::string("");

	if(position != std::string::npos) {
		path = this->fetch_uri.substr(position + 1);
	} else {
		path = this->fetch_uri;
	}

	return path;
}

bool CopyFetch::fetch(BuildDir * d)
{
	PackageCmd pc(d->getPath(), "cp");
	pc.addArg("-dpRuf");
	std::string l = P->absolute_fetch_path(this->fetch_uri);
	pc.addArg(l.c_str());
	pc.addArg(".");
	if(!pc.Run(this->P))
		throw CustomException("Failed to copy (recursively)");
	log(P, "Copied data in, considering code updated");
	P->setCodeUpdated();
	return true;
}

std::string CopyFetch::HASH()
{
	return std::string("");
}

std::string CopyFetch::relative_path()
{
	auto position = this->fetch_uri.rfind("/");
	auto path = std::string("");

	if(position != std::string::npos) {
		path = this->fetch_uri.substr(position + 1);
	} else {
		path = this->fetch_uri;
	}

	return path;
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
