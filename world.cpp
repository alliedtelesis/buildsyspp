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

string_list::iterator World::overlaysStart()
{
	return this->overlays.begin();
}

string_list::iterator World::overlaysEnd()
{
	return this->overlays.end();
}

NameSpace *World::findNameSpace(const std::string &name)
{
	auto iter = this->nameSpacesStart();
	auto iterEnd = this->nameSpacesEnd();
	for(; iter != iterEnd; iter++) {
		if((*iter).getName() == name) {
			return &(*iter);
		}
	}

	this->namespaces.emplace_back(this, name);
	return &this->namespaces.back();
}

static void build_thread(Package *p)
{
	World *w = p->getNS()->getWorld();

	log(p, "Build Thread");
	log(p->getNS()->getName() + "," + p->getName(),
	    boost::format{"Building (%1% others running)"} % (w->threadsRunning() - 1));

	try {
		if(!p->build()) {
			w->setFailed();
			log(p->getNS()->getName() + "," + p->getName(), "Building failed");
		}
	} catch(Exception &E) {
		error(E.error_msg());
		throw;
	}
	w->threadEnded();

	log(p->getNS()->getName() + "," + p->getName(),
	    boost::format{"Finished (%1% others running)"} % w->threadsRunning());
}

static void process_package(Package *p, PackageQueue *pq)
{
	try {
		if(!p->process()) {
			log(p, "Processing failed");
		}
	} catch(Exception &E) {
		log(p, E.error_msg());
		throw;
	}

	auto iter = p->dependsStart();
	auto end = p->dependsEnd();

	for(; iter != end; iter++) {
		Package *dp = (*iter).getPackage();
		if(dp->setProcessingQueued()) {
			pq->push(dp);
		}
	}

	pq->finish();
}

static void process_packages(Package *p)
{
	PackageQueue pq;
	pq.push(p);

	while(!pq.done()) {
		Package *toProcess = pq.pop();
		if(toProcess != nullptr) {
			pq.start();

			std::thread thr(process_package, toProcess, &pq);
			thr.detach();
		}
		pq.wait();
	}
}

bool World::basePackage(const std::string &filename)
{
	std::string filename_copy = filename;

	// Remove any trailing slashes
	filename_copy.erase(filename_copy.find_last_not_of('/') + 1);
	// Strip the directory from the base package name
	std::string pname = filename_copy.substr(filename_copy.rfind('/') + 1);
	// Strip the '.lua' from end of the filename for the namespace name
	std::string nsname = pname.substr(0, pname.find(".lua"));

	this->p =
	    new Package(this->findNameSpace(nsname), pname, filename_copy, filename_copy, "");
	this->p->setNS(this->p->getNS());

	process_packages(this->p);

	// Check for dependency loops
	if(!this->p->checkForDependencyLoops()) {
		error("Dependency Loop Detected");
		return false;
	}

	if(this->areParseOnly()) {
		// We are done, no building required
		return true;
	}

	this->graph.fill(this);
	this->topo_graph.fill(this);

	this->topo_graph.topological();
	while(!this->isFailed() && !this->p->isBuilt()) {
		std::unique_lock<std::mutex> lk(this->cond_lock);
		Package *toBuild = nullptr;
		if(this->threads_limit == 0 || this->threads_running < this->threads_limit) {
			toBuild = this->topo_graph.topoNext();
		}
		if(toBuild != nullptr) {
			// If this package is already building then skip it.
			if(toBuild->isBuilding()) {
				continue;
			}

			toBuild->setBuilding();
			this->threadStarted();
			std::thread thr(build_thread, toBuild);
			thr.detach();
		} else {
			this->cond.wait(lk);
		}
	}
	if(this->areKeepGoing()) {
		std::unique_lock<std::mutex> lk(this->cond_lock);
		while(!this->p->isBuilt() && this->threads_running > 0) {
			this->cond.wait(lk);
			lk.lock();
		}
	}
	return !this->failed;
}

bool World::packageFinished(Package *_p)
{
	std::unique_lock<std::mutex> lk(this->cond_lock);
	this->topo_graph.deleteNode(_p);
	this->topo_graph.topological();
	this->cond.notify_all();
	return true;
}

DLObject *World::_findDLObject(const std::string &fname)
{
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
