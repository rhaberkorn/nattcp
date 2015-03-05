#include <string.h>
#include <stdio.h>

#include <tropicssl/net.h>
#include <tropicssl/ssl.h>
#include <tropicssl/havege.h>
#include <tropicssl/certs.h>
#include <tropicssl/x509.h>

#define pserror(RET, FMT, ...) do {						\
	fprintf(stderr, "Error: " FMT ": TropicSSL code -%#x\n", ##__VA_ARGS__,	\
		-(RET));							\
	fflush(stderr);								\
} while (0)

static ssl_context ssl;
static ssl_session ssn;

static const char *my_dhm_P = 
	"E4004C1F94182000103D883A448B3F80"
	"2CE4B44A83301270002C20D0321CFD00"
	"11CCEF784C26A400F43DFB901BCA7538"
	"F2C6B176001CF5A0FD16D2C48B1D0C1C"
	"F6AC8E1DA6BCC3B4E1F96B0564965300"
	"FFA1D0B601EB2800F489AA512C4B248C"
	"01F76949A60BB7F00A40B1EAB64BDD48"
	"E8A700D60B7F1200FA8E77B0A979DABF";

static const char *my_dhm_G = "4";

int
ctl_init_ssl_client(int fd, const char *cert_file, const char *key_file, const char *key_pwd,
		    const char *ca_file, const char *server_cn)
{
	havege_state hs;

	x509_cert cert;
	x509_cert cachain;
	x509_cert *pca;
	rsa_context rsa;

	int ret = 0;

	memset(&ssl, 0, sizeof(ssl_context));
	memset(&ssn, 0, sizeof(ssl_session));
	memset(&cert, 0, sizeof(x509_cert));
	memset(&cachain, 0, sizeof(x509_cert));
	memset(&rsa, 0, sizeof(rsa_context));

	ret = x509parse_crtfile(&cert, (char *)cert_file);
	if (ret != 0) {
		pserror(ret, "Parsing client certificate chain \"%s\"", cert_file);
		goto abort;
	}
	pca = cert.next;

	ret = x509parse_keyfile(&rsa, (char *)key_file, (char *)key_pwd);
	if (ret != 0) {
		pserror(ret, "Parsing client private key \"%s\"", key_file);
		goto abort;
	}

	if (ca_file) {
		ret = x509parse_crtfile(&cachain, (char *)ca_file);
		if (ret != 0) {
			pserror(ret, "Parsing CA certificate chain \"%s\"", ca_file);
			goto abort;
		}
		pca = &cachain;
	}
	if (pca == NULL) {
		fprintf(stderr, "Error: No CA certificate chain available\n");
		fflush(stderr);
		goto abort;
	}

	havege_init(&hs);

	ret = ssl_init(&ssl);
	if (ret != 0) {
		pserror(ret, "SSL client initialization");
		goto abort;
	}

	ssl_set_endpoint(&ssl, SSL_IS_CLIENT);
	ssl_set_authmode(&ssl, SSL_VERIFY_REQUIRED);

	ssl_set_rng(&ssl, havege_rand, &hs);
	ssl_set_bio(&ssl, net_recv, &fd, net_send, &fd);
	ssl_set_session(&ssl, 1, 600, &ssn);

	ssl_set_ciphers(&ssl, ssl_default_ciphers);

	ssl_set_own_cert(&ssl, &cert, &rsa);
	ssl_set_ca_chain(&ssl, pca, (char *)server_cn);

	while ((ret = ssl_handshake(&ssl)) != 0) {
		if (ret != TROPICSSL_ERR_NET_TRY_AGAIN) {
			pserror(ret, "SSL client handshake");
			goto abort;
		}
	}

	ssl_flush_output(&ssl);

abort:
	x509_free(&cachain);
	x509_free(&cert);
	rsa_free(&rsa);
	ssl_free(&ssl);

	return ret;
}

int
ctl_init_ssl_server(int fd, const char *cert_file, const char *key_file, const char *key_pwd,
		    const char *ca_file)
{
	havege_state hs;

	x509_cert cert;
	x509_cert cachain;
	x509_cert *pca;
	rsa_context rsa;

	int ret = 0;

	memset(&ssl, 0, sizeof(ssl_context));
	memset(&ssn, 0, sizeof(ssl_session));
	memset(&cert, 0, sizeof(x509_cert));
	memset(&cachain, 0, sizeof(x509_cert));
	memset(&rsa, 0, sizeof(rsa_context));

	ret = x509parse_crtfile(&cert, (char *)cert_file);
	if (ret != 0) {
		pserror(ret, "Parsing server certificate chain \"%s\"", cert_file);
		goto abort;
	}
	pca = cert.next;

	ret = x509parse_keyfile(&rsa, (char *)key_file, (char *)key_pwd);
	if (ret != 0) {
		pserror(ret, "Parsing server private key \"%s\"", key_file);
		goto abort;
	}

	if (ca_file) {
		ret = x509parse_crtfile(&cachain, (char *)ca_file);
		if (ret != 0) {
			pserror(ret, "Parsing CA certificate chain \"%s\"", ca_file);
			goto abort;
		}
		pca = &cachain;
	}
	if (pca == NULL) {
		fprintf(stderr, "Error: No CA certificate chain available\n");
		fflush(stderr);
		goto abort;
	}

	havege_init(&hs);

	ret = ssl_init(&ssl);
	if (ret != 0) {
		pserror(ret, "SSL server initialization");
		goto abort;
	}

	ssl_set_endpoint(&ssl, SSL_IS_SERVER);
	ssl_set_authmode(&ssl, SSL_VERIFY_REQUIRED);

	ssl_set_rng(&ssl, havege_rand, &hs);
	ssl_set_bio(&ssl, net_recv, &fd, net_send, &fd);
	ssl_set_session(&ssl, 1, 0, &ssn);

	ssl_set_ciphers(&ssl, ssl_default_ciphers);

	ssl_set_own_cert(&ssl, &cert, &rsa);
	ssl_set_ca_chain(&ssl, pca, NULL);

	ssl_set_dh_param(&ssl, (char *)my_dhm_P, (char *)my_dhm_G);

	while ((ret = ssl_handshake(&ssl)) != 0) {
		if (ret != TROPICSSL_ERR_NET_TRY_AGAIN) {
			pserror(ret, "SSL server handshake");
			goto abort;
		}
	}

	ssl_flush_output(&ssl);

abort:
	x509_free(&cachain);
	x509_free(&cert);
	rsa_free(&rsa);
	ssl_free(&ssl);

	return ret;
}

