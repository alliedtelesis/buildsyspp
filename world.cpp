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
	std::list < NameSpace * >::iterator iter = this->nameSpacesStart();
	std::list < NameSpace * >::iterator iterEnd = this->nameSpacesEnd();
	for(; iter != iterEnd; iter++) {
		if((*iter)->getName().compare(name) == 0)
			return (*iter);
	}

	NameSpace *ns = new NameSpace(name);
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
	features->insert(std::pair < std::string, std::string > (key, value));
	return true;
}

bool World::setFeature(char *kv)
{
	char *eq = strchr(kv, '=');
	if(eq == NULL) {
		error("Features must be described as feature=value\n");
		return false;
	}
	char *temp = (char *) calloc(1, (eq - kv) + 1);
	if(temp == NULL)
		return false;
	strncpy(temp, kv, (eq - kv));
	std::string key(temp);
	free(temp);
	eq++;
	temp = strdup(eq);
	if(temp == NULL)
		return false;
	std::string value(temp);
	free(temp);
	this->setFeature(key, value, true);
	return true;
}


std::string World::getFeature(std::string key)
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

static pthread_mutex_t t_cond_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t t_cond = PTHREAD_COND_INITIALIZER;

static void *build_thread(void *t)
{
	Package *p = (Package *) t;

	log(p, "Build Thread");

	bool skip = false;

	if(p->isBuilding()) {
		skip = true;
	}
	if(!skip)
		p->setBuilding();
	pthread_mutex_lock(&t_cond_lock);
	pthread_cond_broadcast(&t_cond);
	pthread_mutex_unlock(&t_cond_lock);
	if(!skip) {
		if(!p->build())
			WORLD->setFailed();
	}
	WORLD->condTrigger();
	return NULL;
}

bool World::basePackage(char *filename)
{
	this->p = new Package(this->findNameSpace(filename), filename, filename, "");

	try {
		// Load all the lua files
		this->p->process();
		if(!this->areParseOnly()) {
			// Extract all the source code
			this->p->extract();
		}
	}
	catch(Exception & E) {
		error(E.error_msg().c_str());
		return false;
	}
	if(this->areParseOnly() || this->areExtractOnly()) {
		// We are done, no building required
		return true;
	}
	this->graph = new Internal_Graph();
	this->topo_graph = new Internal_Graph();

	this->topo_graph->topological();
	while(!this->isFailed() && !this->p->isBuilt()) {
		pthread_mutex_lock(&this->cond_lock);
		Package *toBuild = this->topo_graph->topoNext();
		if(toBuild != NULL) {
			pthread_t tid;
			pthread_mutex_unlock(&this->cond_lock);
			pthread_mutex_lock(&t_cond_lock);
			pthread_create(&tid, NULL, build_thread, toBuild);
			pthread_cond_wait(&t_cond, &t_cond_lock);
			pthread_mutex_unlock(&t_cond_lock);
		} else {
			pthread_cond_wait(&this->cond, &this->cond_lock);
			pthread_mutex_unlock(&this->cond_lock);
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

bool World::populateForcedList(PackageCmd * pc)
{
	string_list::iterator fIt = this->forcedDeps->begin();
	string_list::iterator fEnd = this->forcedDeps->end();

	for(; fIt != fEnd; fIt++) {
		pc->addArg(*fIt);
	}
	return false;
}

bool World::packageFinished(Package * p)
{
	pthread_mutex_lock(&this->cond_lock);
	this->topo_graph->deleteNode(p);
	this->topo_graph->topological();
	pthread_cond_broadcast(&this->cond);
	pthread_mutex_unlock(&this->cond_lock);
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

World::~World()
{
	delete this->features;
	delete this->forcedDeps;
	delete this->namespaces;
	delete this->overlays;
	delete this->graph;
	delete this->topo_graph;
	delete this->pwd;
	pthread_mutex_destroy(&this->cond_lock);
	pthread_cond_destroy(&this->cond);
}
