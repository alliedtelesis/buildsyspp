/******************************************************************************
 Copyright 2016 Allied Telesis Labs Ltd. All rights reserved.

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

#include "include/buildsys.h"

#include <openssl/evp.h>

void buildsys::hash_setup()
{
	OpenSSL_add_all_digests();
}

void buildsys::hash_shutdown()
{
	EVP_cleanup();
}

std::string buildsys::hash_file(const std::string &fname)
{
	EVP_MD_CTX *mdctx;
	const EVP_MD *md;
	const int BUFLEN = 4096;
	char buff[BUFLEN];
	unsigned char md_value[EVP_MAX_MD_SIZE];
	unsigned int md_len;

	md = EVP_get_digestbyname("sha256");

	mdctx = EVP_MD_CTX_create();
	EVP_DigestInit_ex(mdctx, md, nullptr);

	FILE *f = fopen(fname.c_str(), "rb");
	if(!f) {
		fprintf(stderr, "Failed opening: %s\n", fname.c_str());
		return std::string("");
	}
	while(!feof(f)) {
		size_t red = fread(buff, 1, BUFLEN, f);
		EVP_DigestUpdate(mdctx, buff, red);
	}
	fclose(f);

	EVP_DigestFinal_ex(mdctx, md_value, &md_len);
	EVP_MD_CTX_destroy(mdctx);

	std::stringstream ss;

	for(unsigned int i = 0; i < md_len; i++) {
		ss << std::hex << std::setfill('0') << std::setw(2)
		   << static_cast<int>(md_value[i]);
	}

	return ss.str();
}
