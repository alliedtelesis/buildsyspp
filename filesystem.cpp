/******************************************************************************
 Copyright 2019 Allied Telesis Labs Ltd. All rights reserved.

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

/**
 * The functionality in this file should be removed and replaced with the filesystem
 * functionality in the standard library once a sufficient version of the compiler is
 * available.
 */

#include <buildsys.h>
#include <sys/stat.h>

namespace buildsys
{
	namespace filesystem
	{
		void create_directories(const std::string &path)
		{
			if(!exists(path)) {
				auto cmd = string_format("mkdir -p %s", path.c_str());
				auto ret = std::system(cmd.c_str());
				if(ret != 0) {
					throw CustomException("Failed to create directories");
				}
			}
		}
		void remove_all(const std::string &path)
		{
			auto cmd = string_format("rm -fr %s", path.c_str());
			auto ret = std::system(cmd.c_str());
			if(ret != 0) {
				throw CustomException("Failed to remove directories");
			}
		}
		bool exists(const std::string &path)
		{
			struct stat buf;
			return (stat(path.c_str(), &buf) == 0);
		}
		bool is_directory(const std::string &path)
		{
			struct stat buf;
			if(stat(path.c_str(), &buf) == 0 && S_ISDIR(buf.st_mode)) {
				return true;
			}
			return false;
		}
	}
}
