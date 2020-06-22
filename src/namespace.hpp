/******************************************************************************
 Copyright 2020 Allied Telesis Labs Ltd. All rights reserved.

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

#ifndef NAMESPACE_HPP_
#define NAMESPACE_HPP_

#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>

namespace buildsys
{
	class Package;

	//! A namespace
	class NameSpace
	{
	private:
		const std::string name;
		const std::string staging_dir;
		const std::string install_dir;
		std::list<std::unique_ptr<Package>> packages;
		mutable std::mutex lock;

	public:
		explicit NameSpace(std::string _name);
		const std::string &getName() const;
		void for_each_package(const std::function<void(Package &)> &func) const;
		Package *findPackage(const std::string &_name);
		void addPackage(std::unique_ptr<Package> p);
		const std::string &getStagingDir() const;
		const std::string &getInstallDir() const;
		static void for_each(const std::function<void(const NameSpace &)> &func);
		static void printNameSpaces();
		static NameSpace *findNameSpace(const std::string &name);
		static void deleteAll();
	};
} // namespace buildsys

#endif // NAMESPACE_HPP_
