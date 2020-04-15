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

#include "logger.hpp"

using namespace buildsys;

// VT100 terminal color escape sequences
#define COLOR_NORMAL ""
#define COLOR_RESET "\033[m"
#define COLOR_BOLD "\033[1m"
#define COLOR_RED "\033[31m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_BLUE "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN "\033[36m"
#define COLOR_BOLD_RED "\033[1;31m"
#define COLOR_BOLD_GREEN "\033[1;32m"
#define COLOR_BOLD_YELLOW "\033[1;33m"
#define COLOR_BOLD_BLUE "\033[1;34m"
#define COLOR_BOLD_MAGENTA "\033[1;35m"
#define COLOR_BOLD_CYAN "\033[1;36m"
#define COLOR_BG_RED "\033[41m"
#define COLOR_BG_GREEN "\033[42m"
#define COLOR_BG_YELLOW "\033[43m"
#define COLOR_BG_BLUE "\033[44m"
#define COLOR_BG_MAGENTA "\033[45m"
#define COLOR_BG_CYAN "\033[46m"

Logger::Logger()
{
	this->output = &std::cout;
}

Logger::Logger(std::string _prefix) : prefix(std::move(_prefix))
{
	this->output = &std::cout;
}

Logger::Logger(std::string _prefix, const std::string &file_path)
    : prefix(std::move(_prefix))
{
	this->file_output = std::make_unique<std::ofstream>(file_path);
	this->output = this->file_output.get();
}

void Logger::log(const std::string &str)
{
	auto msg = boost::format{"%1%: %2%"} % this->prefix % str;
	*this->output << msg << std::endl;
}

void Logger::log(const boost::format &str)
{
	this->log(str.str());
}

static inline const char *get_color(const std::string &mesg)
{
	if(mesg.find("error:") != std::string::npos) {
		return COLOR_BOLD_RED;
	}
	if(mesg.find("warning:") != std::string::npos) {
		return COLOR_BOLD_BLUE;
	}

	return nullptr;
}

void Logger::program_output(const std::string &mesg)
{
	static int isATTY = isatty(fileno(stdout));
	const char *color;
	boost::format msg;
	bool output_is_cout = (this->output == &std::cout);

	if(output_is_cout && (isATTY != 0) && ((color = get_color(mesg)) != nullptr)) {
		msg = boost::format{"%1%: %2%%3%%4%"} % this->prefix % color % mesg % COLOR_RESET;
	} else {
		msg = boost::format{"%1%: %2%"} % this->prefix % mesg;
	}
	*this->output << msg << std::endl;
}
