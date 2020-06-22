/******************************************************************************
 Copyright 2015 Allied Telesis Labs Ltd. All rights reserved.

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

#include "namespace.hpp"
#include "include/buildsys.h"
#include <utility>

using namespace buildsys;

static std::list<NameSpace> namespaces;
static std::mutex namespaces_lock;

/**
 * Construct the NameSpace object.
 *
 * @param _name - The name of the namespace.
 */
NameSpace::NameSpace(std::string _name)
    : name(std::move(_name)), staging_dir("output/" + this->name + "/staging"),
      install_dir("output/" + this->name + "/install")
{
}

/**
 * Get the name of the NameSpace.
 *
 * @returns The name.
 */
const std::string &NameSpace::getName() const
{
	return this->name;
}

/**
 * Call a function for each Package in the NameSpace.
 *
 * @param func - The function to call with each Package.
 */
void NameSpace::for_each_package(const std::function<void(Package &)> &func) const
{
	std::unique_lock<std::mutex> lk(this->lock);
	for(const auto &package : this->packages) {
		func(*package);
	}
}

/**
 * Call a function for each NameSpace that exists.
 *
 * @param func - The function to call with each NameSpace.
 */
void NameSpace::for_each(const std::function<void(const NameSpace &)> &func)
{
	std::unique_lock<std::mutex> lk(namespaces_lock);
	for(const auto &ns : namespaces) {
		func(ns);
	}
}

/**
 * Print the names of all the created NameSpaces to stdout.
 */
void NameSpace::printNameSpaces()
{
	std::cout << std::endl << "----BEGIN NAMESPACES----" << std::endl;
	NameSpace::for_each(
	    [](const NameSpace &ns) { std::cout << ns.getName() << std::endl; });
	std::cout << "----END NAMESPACES----" << std::endl;
}

/**
 * Find (or create) a namespace
 *
 * @param name - The name of the namespace to find.
 *
 * @returns The pointer to the namespace.
 */
NameSpace *NameSpace::findNameSpace(const std::string &name)
{
	std::unique_lock<std::mutex> lk(namespaces_lock);
	for(auto &ns : namespaces) {
		if(ns.getName() == name) {
			return &ns;
		}
	}

	namespaces.emplace_back(name);
	return &namespaces.back();
}

/**
 * Get the path to the staging directory for this NameSpace.
 *
 * @returns The staging directory path.
 */
const std::string &NameSpace::getStagingDir() const
{
	return this->staging_dir;
}

/**
 * Get the path to the install directory for this NameSpace.
 *
 * @returns The install directory path.
 */
const std::string &NameSpace::getInstallDir() const
{
	return this->install_dir;
}

/**
 * Lookup the package by name. If it doesn't exist then it will
 * be created.
 *
 * @param _name - The name of the package to lookup.
 *
 * @returns A pointer to the package.
 */
Package *NameSpace::findPackage(const std::string &_name)
{
	std::unique_lock<std::mutex> lk(this->lock);

	for(const auto &package : this->packages) {
		if(package->getName() == _name) {
			return package.get();
		}
	}

	// Package not found, create it
	std::unique_ptr<Package> p = std::make_unique<Package>(this, _name);
	Package *ret = p.get();
	this->packages.push_back(std::move(p));

	return ret;
}

/**
 * Add a package to this NameSpace.
 *
 * @param p - The package to add.
 */
void NameSpace::addPackage(std::unique_ptr<Package> p)
{
	std::unique_lock<std::mutex> lk(this->lock);
	this->packages.push_back(std::move(p));
}

/**
 * Delete all namespaces.
 */
void NameSpace::deleteAll()
{
	std::unique_lock<std::mutex> lk(namespaces_lock);
	namespaces.clear();
}
