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

#ifndef DIR_BUILDDIR_HPP_
#define DIR_BUILDDIR_HPP_

#include "../interface/builddir.hpp"
#include <string>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace buildsys
{
	/**
	 * A directory for building a package in. Each package has one build
	 * directory which is used to run all the commands in.
	 */
	class BuildDir
	{
	private:
		std::string path;
		std::string rpath;
		std::string staging;
		std::string new_path;
		std::string new_staging;
		std::string new_install;
		std::string work_build;
		std::string work_src;

	public:
		BuildDir() = default;

		/** Create a build directory
		 *  @param pwd - The current working directory
		 *  @param gname - The namespace name of the package
		 *  @param pname - The package name
		 */
		BuildDir(const std::string &pwd, const std::string &gname,
		         const std::string &pname);
		//! Return the full path to this directory
		const std::string &getPath() const
		{
			return this->path;
		};
		//! Get the short version (relative only) of the working directory
		const std::string &getShortPath() const
		{
			return this->rpath;
		};
		//! Return the full path to the staging directory
		const std::string &getStaging() const
		{
			return this->staging;
		};
		//! Return the full path to the new directory
		const std::string &getNewPath() const
		{
			return this->new_path;
		};
		//! Return the full path to the new staging directory
		const std::string &getNewStaging() const
		{
			return this->new_staging;
		};
		//! Return the full path to the new install directory
		const std::string &getNewInstall() const
		{
			return this->new_install;
		};
		//! Remove all of the work directory contents
		void clean() const;
		//! Remove all of the staging directory contents
		void cleanStaging() const;

		void lua_table(lua_State *L);
	};
} // namespace buildsys

#endif // DIR_BUILDDIR_HPP_
