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

#ifndef BUILDINFO_HPP_
#define BUILDINFO_HPP_

#include <memory>
#include <ostream>
#include <string>
#include <vector>

namespace buildsys
{
	/**
	 * A build description. Describes relevant steps taken to build a Package.
	 * This information is used to determine if a package needs rebuilding.
	 */
	class BuildDescription
	{
	private:
		std::vector<std::string> BUs;

	public:
		void add_feature_value(bool ignored, const std::string &feature,
		                       const std::string &value);
		void add_nil_feature_value(const std::string &feature);
		void add_package_file(const std::string &fname, const std::string &hash);
		void add_require_file(const std::string &fname, const std::string &hash);
		void add_output_info_file(const std::string &fname, const std::string &hash);
		void add_build_info_file(const std::string &fname, const std::string &hash);
		void add_extraction_info_file(const std::string &fname, const std::string &hash);
		void print(std::ostream &out) const;
	};
} // namespace buildsys

#endif // BUILDINFO_HPP_
