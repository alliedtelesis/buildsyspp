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

#include "logger.hpp"

using namespace buildsys;

/**
 * Construct a Logger object. Log messages will be output to std::cout
 * with no prefix.
 */
Logger::Logger()
{
	this->output = &std::cout;
	this->output_supports_colour = (isatty(fileno(stdout)) == 1);
}

/**
 * Construct a Logger object. Log messages will be output to std::cout
 * with the given prefix.
 *
 * @param _prefix - The prefix to prepend to log messages.
 */
Logger::Logger(const std::string &_prefix)
{
	this->prefix = _prefix + ": ";
	this->output = &std::cout;
	this->output_supports_colour = (isatty(fileno(stdout)) == 1);
}

/**
 * Construct a Logger object. Log messages will be output to the given
 * file path with the given prefix.
 *
 * @param _prefix - The prefix to prepend to log messages.
 * @param file_path - The file to output the log messages to.
 */
Logger::Logger(const std::string &_prefix, const std::string &file_path)
{
	this->prefix = _prefix + ": ";
	this->file_output = std::make_unique<std::ofstream>(file_path);
	this->output = this->file_output.get();
}

/**
 * Output a log message.
 *
 * @param str - The string to output.
 */
void Logger::log(const std::string &str)
{
	auto msg = boost::format{"%1%%2%"} % this->prefix % str;
	*this->output << msg << std::endl;
}

/**
 * Output a log message.
 *
 * @param str - The boost::format object containing the string to output.
 */
void Logger::log(const boost::format &str)
{
	this->log(str.str());
}

/**
 * Returns whether the logging supports colour output.
 *
 * @returns true if supported, false otherwise.
 */
bool Logger::supports_colour_output() const
{
	return this->output_supports_colour;
}
