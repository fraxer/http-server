/*
The MIT License
Copyright (c) 2012 Comfirm <http://www.comfirm.se/>
https://github.com/amail/firm-dkim
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef _FIRM_DKIM_H_
#define _FIRM_DKIM_H_

#include <openssl/rand.h>
#include <openssl/rsa.h>
#include <openssl/engine.h>
#include <openssl/sha.h>

#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <openssl/buffer.h>
#include <openssl/err.h>

namespace server {

struct stringpair{
	char *key;
	char *value;
};

stringpair **relaxed_header_canon(stringpair **headers, int headerc);
char *relaxed_body_canon(char *body);
char *relaxed_body_canon_line(char *line);

char *rtrim_lines(char *str);
char *rtrim(char *str);
char *ltrim(char *str);
char *wrap(char *str, int len);
int rsa_read_pem(RSA **rsa, char *buff, int len);
char *base64_encode(const unsigned char *input, int length);

char *dkim_create(stringpair **headers, int headerc, char *body, char *pkey, char *domain, char *selector, int v);

} // namespace

#endif