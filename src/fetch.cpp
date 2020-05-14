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

#include "include/buildsys.h"

std::string DownloadFetch::tarball_cache;
std::list<DLObject> DownloadFetch::dlobjects;
std::mutex DownloadFetch::dlobjects_lock;

/**
 *  Set the location of the local tarball cache
 *
 *  @param cache - The location to set.
 */
void DownloadFetch::setTarballCache(std::string cache)
{
	tarball_cache = std::move(cache);
}

//! Find (or create) a DLObject for a given full file name
const DLObject *DownloadFetch::findDLObject(const std::string &fname)
{
	std::unique_lock<std::mutex> lk(this->dlobjects_lock);
	auto iter = this->dlobjects.begin();
	auto iterEnd = this->dlobjects.end();
	for(; iter != iterEnd; iter++) {
		if((*iter).fileName() == fname) {
			return &(*iter);
		}
	}

	this->dlobjects.emplace_back(fname);
	return &this->dlobjects.back();
}

/* This is the full name of the file to be downloaded */
std::string DownloadFetch::full_name()
{
	auto fname = std::string("");

	if(this->filename.empty()) {
		auto position = this->fetch_uri.rfind('/');
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
	auto fullname = this->full_name();
	auto ret = fullname;

	if(this->decompress) {
		auto position = fullname.rfind('.');
		if(position != std::string::npos) {
			ret = fullname.substr(0, position);
		}
	}

	return ret;
}

bool DownloadFetch::fetch(BuildDir *) // NOLINT
{
	filesystem::create_directories("dl");

	std::string fullname = this->full_name();
	std::string fname = this->final_name();

	/* Hold a lock while we download this file
	 * Also checks for conflicting hashes for the same file
	 */
	const DLObject *dlobj = this->findDLObject(fullname);
	if(dlobj == nullptr) {
		this->P->log("Failed to get the DLObject for " + fullname);
		return false;
	}

	std::unique_lock<std::mutex> lk(dlobj->getLock());

	if(this->hash.length() != 0) {
		if(dlobj->HASH().length() != 0) {
			if(dlobj->HASH() != this->hash) {
				this->P->log(
				    boost::format{
				        "Another package has already downloaded %1% with hash %2% (but "
				        "we need %3%)"} %
				    fullname % dlobj->HASH() % this->hash);
				return false;
			}
		}
	}

	std::string _fpath = this->P->getPwd() + "/dl/" + fname;
	if(!filesystem::exists(_fpath)) {
		bool localCacheHit = false;
		// Attempt to get file from local tarball cache if one is configured.
		if(!this->tarball_cache.empty()) {
			filesystem::remove("dl/" + fullname + ".tmp");
			PackageCmd pc("dl", "wget");
			std::string url = this->tarball_cache + "/" + fname;
			pc.addArg(url);
			pc.addArg("-O" + fullname + ".tmp");
			localCacheHit = pc.Run(this->P->getLogger());
			if(localCacheHit) {
				filesystem::rename("dl/" + fullname + ".tmp", "dl/" + fullname);
			}
		}
		// If we didn't get the file from the local cache, look upstream.
		if(!localCacheHit) {
			filesystem::remove("dl/" + fullname + ".tmp");
			PackageCmd pc("dl", "wget");
			pc.addArg(this->fetch_uri);
			pc.addArg("-O" + fullname + ".tmp");
			if(!pc.Run(this->P->getLogger())) {
				throw CustomException("Failed to fetch file");
			}
			filesystem::rename("dl/" + fullname + ".tmp", "dl/" + fullname);
		}
		if(decompress) {
			// We want to run a command on this file
			std::string cmd;

			size_t ext_pos = fname.rfind('.');
			if(ext_pos == std::string::npos) {
				P->log("Could not guess decompression based on extension: " + fname);
			} else {
				std::string ext = fname.substr(ext_pos + 1);
				if(ext == ".bz2") {
					cmd = "bunzip2 -d dl/" + fullname;
				} else if(ext == ".gz") {
					cmd = "gunzip -d dl/" + fullname;
				}
				std::system(cmd.c_str());
			}
		}
	}

	bool ret = true;

	if(this->hash.length() != 0) {
		auto fpath = boost::format{"%1%/dl/%2%"} % this->P->getPwd() % this->final_name();
		std::string _hash = hash_file(fpath.str());

		if(this->hash != _hash) {
			this->P->log(boost::format{
			                 "Hash mismatched for %1%\n(committed to %2%, providing %3%)"} %
			             this->final_name() % this->hash % _hash);
			ret = false;
		}
	}

	return ret;
}

std::string DownloadFetch::HASH()
{
	/* Check if the package contains pre-computed hashes */
	std::string _hash = P->getFileHash(this->final_name());
	/* Otherwise fetch and calculate the hash */
	if(_hash.empty()) {
		P->log(boost::format{"No hash for %1% in package/%2%/Digest"} % this->final_name() %
		       P->getName());
		throw CustomException("Missing hash " + P->getName());
	}
	this->hash = _hash;
	return this->hash;
}

bool LinkFetch::fetch(BuildDir *d)
{
	PackageCmd pc(d->getPath(), "ln");
	pc.addArg("-sf");
	std::string l = P->relative_fetch_path(this->fetch_uri);
	pc.addArg(l);
	pc.addArg(".");
	if(!pc.Run(this->P->getLogger())) {
		// An error occured, try remove the file, then relink
		PackageCmd rmpc(d->getPath(), "rm");
		rmpc.addArg("-fr");

		// Remove any trailing slashes
		std::string l2 = l;
		l2.erase(l2.find_last_not_of('/') + 1);
		// Strip the directory from the file name
		l2 = l2.substr(l2.rfind('/') + 1);
		rmpc.addArg(l2);
		if(!rmpc.Run(this->P->getLogger())) {
			throw CustomException(
			    "Failed to ln (symbolically), could not remove target first");
		}
		if(!pc.Run(this->P->getLogger())) {
			throw CustomException(
			    "Failed to ln (symbolically), even after removing target first");
		}
	}
	return true;
}

std::string LinkFetch::HASH()
{
	return std::string("");
}

std::string LinkFetch::relative_path()
{
	auto position = this->fetch_uri.rfind('/');
	auto path = std::string("");

	if(position != std::string::npos) {
		path = this->fetch_uri.substr(position + 1);
	} else {
		path = this->fetch_uri;
	}

	return path;
}

bool CopyFetch::fetch(BuildDir *d)
{
	PackageCmd pc(d->getPath(), "cp");
	pc.addArg("-dpRuf");
	std::string l = P->absolute_fetch_path(this->fetch_uri);
	pc.addArg(l);
	pc.addArg(".");
	if(!pc.Run(this->P->getLogger())) {
		throw CustomException("Failed to copy (recursively)");
	}
	P->log("Copied data in, considering code updated");
	P->setCodeUpdated();
	return true;
}

std::string CopyFetch::HASH()
{
	return std::string("");
}

std::string CopyFetch::relative_path()
{
	auto position = this->fetch_uri.rfind('/');
	auto path = std::string("");

	if(position != std::string::npos) {
		path = this->fetch_uri.substr(position + 1);
	} else {
		path = this->fetch_uri;
	}

	return path;
}

void Fetch::add(std::unique_ptr<FetchUnit> fu)
{
	this->FUs.push_back(std::move(fu));
}
