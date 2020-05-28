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

static void build_thread(World *w, Package *p)
{
	p->log("Build Thread");
	p->log(boost::format{"Building (%1% others running)"} % (w->threadsRunning() - 1));

	try {
		if(p->build()) {
			w->packageFinished(p);
		} else {
			w->setFailed();
			p->log("Building failed");
		}
	} catch(std::exception &e) {
		p->log(e.what());
		throw;
	}
	w->threadEnded();

	p->log(boost::format{"Finished (%1% others running)"} % w->threadsRunning());
}

static void process_package(Package *p, PackageQueue *pq)
{
	try {
		if(!p->process()) {
			p->log("Processing failed");
		}
	} catch(std::exception &e) {
		p->log(e.what());
		throw;
	}

	for(auto &depend : p->getDepends()) {
		Package *dp = depend.getPackage();
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
	Logger err_logger("BuildSys");

	// Strip the directory from the base package name
	std::string pname = filename.substr(filename.rfind('/') + 1);
	// Strip the '.lua' from end of the filename for the namespace name
	std::string nsname = pname.substr(0, pname.find(".lua"));

	NameSpace *ns = NameSpace::findNameSpace(nsname);

	std::unique_ptr<Package> p = std::make_unique<Package>(ns, pname, filename, filename);
	Package *base_package = p.get();
	ns->addPackage(std::move(p));

	process_packages(base_package);

	this->graph.fill();
	this->topo_graph.fill();

	std::unordered_set<Package *> cycled_packages = this->graph.get_cycled_packages();
	if(!cycled_packages.empty()) {
		err_logger.log("Dependency Loop Detected");
		err_logger.log("Cycled Packages:");
		for(auto &package : cycled_packages) {
			err_logger.log(boost::format{"    %1%,%2%"} % package->getNS()->getName() %
			               package->getName());
		}
		return false;
	}

	if(this->areParseOnly()) {
		// We are done, no building required
		return true;
	}

	this->topo_graph.topological();
	while(!this->isFailed() && !base_package->isBuilt()) {
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
			std::thread thr(build_thread, this, toBuild);
			thr.detach();
		} else {
			this->cond.wait(lk);
		}
	}
	if(this->areKeepGoing()) {
		std::unique_lock<std::mutex> lk(this->cond_lock);
		while(!base_package->isBuilt() && this->threads_running > 0) {
			this->cond.wait(lk);
			lk.unlock();
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
