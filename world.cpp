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
	return this->overlays->begin();
}

string_list::iterator World::overlaysEnd()
{
	return this->overlays->end();
}

NameSpace *World::findNameSpace(std::string name)
{
	std::list<NameSpace *>::iterator iter = this->nameSpacesStart();
	std::list<NameSpace *>::iterator iterEnd = this->nameSpacesEnd();
	for(; iter != iterEnd; iter++) {
		if((*iter)->getName().compare(name) == 0)
			return (*iter);
	}

	NameSpace *ns = new NameSpace(this, name);
	this->namespaces->push_back(ns);
	return ns;
}

bool World::setFeature(std::string key, std::string value, bool override)
{
	// look for this key
	if(features->find(key) != features->end()) {
		if(override) {
			// only over-write the value if we are explicitly told to
			(*features)[key] = value;
		}
		return true;
	}
	features->insert(std::pair<std::string, std::string>(key, value));
	return true;
}

bool World::setFeature(std::string kv)
{
	auto pos = kv.find('=');
	if(pos == std::string::npos) {
		error("Features must be described as feature=value\n");
		return false;
	}

	std::string key = kv.substr(0, pos);
	std::string value = kv.substr(pos + 1);

	this->setFeature(key, value, true);
	return true;
}

std::string World::getFeature(const std::string &key)
{
	if(features->find(key) != features->end()) {
		return (*features)[key];
	}
	throw NoKeyException();
}

void World::printFeatureValues()
{
	std::cout << std::endl << "----BEGIN FEATURE VALUES----" << std::endl;
	for(key_value::iterator it = this->features->begin(); it != this->features->end();
	    ++it) {
		std::cout << it->first << "\t" << it->second << std::endl;
	}
	std::cout << "----END FEATURE VALUES----" << std::endl;
}

static void build_thread(Package *p)
{
	World *w = p->getNS()->getWorld();

	log(p, "Build Thread");
	log((p->getNS()->getName() + "," + p->getName()).c_str(),
	    "Building (%i others running)", w->threadsRunning() - 1);

	try {
		if(!p->build()) {
			w->setFailed();
			log((p->getNS()->getName() + "," + p->getName()).c_str(), "Building failed");
		}
	} catch(Exception &E) {
		error(E.error_msg().c_str());
		throw E;
	}
	w->threadEnded();

	log((p->getNS()->getName() + "," + p->getName()).c_str(),
	    "Finished (%i others running)", w->threadsRunning());
}

static void process_package(Package *p, PackageQueue *pq)
{
	try {
		if(!p->process()) {
			log(p, "Processing failed");
		}
	} catch(Exception &E) {
		log(p, E.error_msg().c_str());
		throw E;
	}

	auto iter = p->dependsStart();
	auto end = p->dependsEnd();

	for(; iter != end; iter++) {
		Package *dp = (*iter)->getPackage();
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
		if(toProcess != NULL) {
			pq.start();

			std::thread thr(process_package, toProcess, &pq);
			thr.detach();
		}
		pq.wait();
	}
}

bool World::basePackage(const std::string &filename)
{
	// Strip the directory from the base package name
	char *filename_copy = strdup(filename.c_str());
	const char *pname = basename(filename_copy);

	// Strip the '.lua' from end of the filename for the namespace name
	char *nsname = strdup(pname);
	int p_len = strlen(pname);
	if(nsname[p_len - 4] == '.') {
		nsname[p_len - 4] = '\0';
	}

	this->p =
	    new Package(this->findNameSpace(nsname), pname, filename_copy, filename_copy, "");
	free(nsname);
	free(filename_copy);
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
	this->graph = new Internal_Graph(this);
	this->topo_graph = new Internal_Graph(this);

	this->topo_graph->topological();
	while(!this->isFailed() && !this->p->isBuilt()) {
		std::unique_lock<std::mutex> lk(this->cond_lock);
		Package *toBuild = NULL;
		if(this->threads_limit == 0 || this->threads_running < this->threads_limit) {
			toBuild = this->topo_graph->topoNext();
		}
		if(toBuild != NULL) {
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

bool World::isForced(std::string name)
{
	string_list::iterator fIt = this->forcedDeps->begin();
	string_list::iterator fEnd = this->forcedDeps->end();

	for(; fIt != fEnd; fIt++) {
		if((*fIt).compare(name) == 0)
			return true;
	}
	return false;
}

bool World::populateForcedList(PackageCmd *pc)
{
	string_list::iterator fIt = this->forcedDeps->begin();
	string_list::iterator fEnd = this->forcedDeps->end();

	for(; fIt != fEnd; fIt++) {
		pc->addArg(*fIt);
	}
	return false;
}

bool World::packageFinished(Package *_p)
{
	std::unique_lock<std::mutex> lk(this->cond_lock);
	this->topo_graph->deleteNode(_p);
	this->topo_graph->topological();
	this->cond.notify_all();
	return true;
}

bool World::isIgnoredFeature(std::string feature)
{
	string_list::iterator iter = this->ignoredFeatures->begin();
	string_list::iterator iterEnd = this->ignoredFeatures->end();
	for(; iter != iterEnd; iter++) {
		if((*iter).compare(feature) == 0)
			return true;
	}
	return false;
}

DLObject *World::_findDLObject(std::string fname)
{
	std::list<DLObject *>::iterator iter = this->dlobjects->begin();
	std::list<DLObject *>::iterator iterEnd = this->dlobjects->end();
	for(; iter != iterEnd; iter++) {
		if((*iter)->fileName().compare(fname) == 0)
			return (*iter);
	}

	DLObject *dlo = new DLObject(fname);
	this->dlobjects->push_back(dlo);
	return dlo;
}

World::~World()
{
	delete this->features;
	delete this->forcedDeps;
	this->p = NULL;
	while(!this->namespaces->empty()) {
		NameSpace *ns = this->namespaces->front();
		this->namespaces->pop_front();
		delete ns;
	}
	delete this->namespaces;
	while(!this->dlobjects->empty()) {
		DLObject *dlo = this->dlobjects->front();
		this->dlobjects->pop_front();
		delete dlo;
	}
	delete this->dlobjects;
	delete this->overlays;
	delete this->graph;
	delete this->topo_graph;
	delete this->ignoredFeatures;
}
