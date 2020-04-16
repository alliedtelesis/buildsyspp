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

#ifndef LOGGER_HPP_
#define LOGGER_HPP_

#include <boost/format.hpp>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace buildsys
{
	class Logger
	{
	private:
		std::string prefix;
		std::unique_ptr<std::ofstream> file_output;
		std::ostream *output{nullptr};
		bool output_supports_colour{false};

		const char *get_colour(const std::string &str);

	public:
		// VT100 terminal color escape sequences
		static constexpr const char *COLOUR_NORMAL{""};
		static constexpr const char *COLOUR_RESET{"\033[m"};
		static constexpr const char *COLOUR_BOLD_RED{"\033[1;31m"};
		static constexpr const char *COLOUR_BOLD_BLUE{"\033[1;34m"};

		Logger();
		explicit Logger(const std::string &_prefix);
		Logger(const std::string &_prefix, const std::string &file_path);
		void log(const std::string &str);
		void log(const boost::format &str);
		void program_output(const std::string &str);
		void force_colour_output(bool set);
	};
} // namespace buildsys

#endif // LOGGER_HPP_
