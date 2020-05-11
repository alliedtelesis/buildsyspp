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

#include "lua.hpp"
#include "exceptions.hpp"

using namespace buildsys;

Lua::Lua()
{
	this->state = luaL_newstate();
	if(this->state == nullptr) {
		throw CustomException("Lua Error");
	}
	luaL_openlibs(this->state);
}

Lua::~Lua()
{
	lua_close(state);
	this->state = nullptr;
}

/**
 * Load and execute a lua file in this instance
 *
 * @param filename - The name of the lua file to load and run
 */
void Lua::processFile(const std::string &filename)
{
	if(luaL_dofile(state, filename.c_str()) != 0) {
		throw CustomException(lua_tostring(state, -1));
	}
}

/**
 * Register a function in this lua instance
 *
 * @param name - The name of the function
 * @param fn - The function to call
 */
void Lua::registerFunc(const std::string &name, lua_CFunction fn)
{
	lua_register(state, name.c_str(), fn);
}
