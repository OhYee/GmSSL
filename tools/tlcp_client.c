﻿/*
 * Copyright (c) 2021 - 2021 The GmSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the GmSSL Project.
 *    (http://gmssl.org/)"
 *
 * 4. The name "GmSSL Project" must not be used to endorse or promote
 *    products derived from this software without prior written
 *    permission. For written permission, please contact
 *    guanzhi1980@gmail.com.
 *
 * 5. Products derived from this software may not be called "GmSSL"
 *    nor may "GmSSL" appear in their names without prior written
 *    permission of the GmSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the GmSSL Project
 *    (http://gmssl.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE GmSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE GmSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <gmssl/tls.h>
#include <gmssl/error.h>


static const char *http_get =
	"GET / HTTP/1.1\r\n"
	"Hostname: aaa\r\n"
	"\r\n\r\n";


// 虽然服务器可以用双证书，但是客户端只能使用一个证书，也就是签名证书
static const char *options = "-host str [-port num] [-cacert file] [-cert file -key file [-pass str]]";

int tlcp_client_main(int argc, char *argv[])
{
	int ret = -1;
	char *prog = argv[0];
	char *host = NULL;
	int port = 443;
	char *pass = NULL;
	TLS_CONNECT conn;
	char buf[100] = {0};
	size_t len = sizeof(buf);

	char *file;

	FILE *cacertfp = NULL;
	FILE *certfp = NULL;
	FILE *keyfp = NULL;
	SM2_KEY sign_key;

	if (argc < 2) {
		fprintf(stderr, "usage: %s %s\n", prog, options);
		return 1;
	}

	argc--;
	argv++;
	while (argc >= 1) {
		if (!strcmp(*argv, "-help")) {
			printf("usage: %s %s\n", prog, options);
			return 0;
		} else if (!strcmp(*argv, "-host")) {
			if (--argc < 1) goto bad;
			host = *(++argv);
		} else if (!strcmp(*argv, "-port")) {
			if (--argc < 1) goto bad;
			port = atoi(*(++argv));
		} else if (!strcmp(*argv, "-cacert")) {
			if (--argc < 1) goto bad;
			file = *(++argv);
			if (!(cacertfp = fopen(file, "r"))) {
				error_print();
				return -1;
			}
		} else if (!strcmp(*argv, "-cert")) {
			if (--argc < 1) goto bad;
			file = *(++argv);
			if (!(certfp = fopen(file, "r"))) {
				error_print();
				return -1;
			}
		} else if (!strcmp(*argv, "-key")) {
			if (--argc < 1) goto bad;
			file = *(++argv);
			if (!(keyfp = fopen(file, "r"))) {
				error_print();
				return -1;
			}
		} else if (!strcmp(*argv, "-pass")) {
			if (--argc < 1) goto bad;
			pass = *(++argv);
		} else {
			fprintf(stderr, "%s: invalid option '%s'\n", prog, *argv);
			return 1;
bad:
			fprintf(stderr, "%s: option '%s' argument required\n", prog, *argv);
			return 0;
		}
		argc--;
		argv++;
	}

	if (!host) {
		error_print();
		return -1;
	}

	if (certfp) {
		if (!keyfp) {
			error_print();
			return -1;
		}
		if (!pass) {
			pass = getpass("Password : ");
		}
		if (sm2_private_key_info_decrypt_from_pem(&sign_key, pass, keyfp) != 1) {
			error_print();
			return -1;
		}
	}

	memset(&conn, 0, sizeof(conn));

	if (tlcp_connect(&conn, host, port, cacertfp, certfp, &sign_key) != 1) {
		error_print();
		return -1;
	}


	// 这个client 发收了一个消息就结束了
	if (tls_send(&conn, (uint8_t *)"12345\n", 6) != 1) {
		error_print();
		return -1;
	}

	for (;;) {
		memset(buf, 0, sizeof(buf));
		len = sizeof(buf);
		if (tls_recv(&conn, (uint8_t *)buf, &len) != 1) {
			error_print();
			return -1;
		}
		if (len > 0) {
			printf("%s\n", buf);
			break;
		}
	}

	return 0;
}
