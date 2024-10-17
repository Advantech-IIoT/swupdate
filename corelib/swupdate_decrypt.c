/*
 * (C) Copyright 2016
 * Stefano Babic, DENX Software Engineering, sbabic@denx.de.
 *
 * SPDX-License-Identifier:     GPL-2.0-only
 *
 * Code mostly taken from openssl examples
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "swupdate.h"
#include "sslapi.h"
#include "util.h"

//to verify the signature file without origin file for compare and write down into file
int swupdate_RSA_decrypt_file(const char *public_key_path, const char *signature_file_path , char *output_path)
{
    // Load the public key
    FILE *public_key_file = fopen(public_key_path, "r");
    if (!public_key_file) {
        perror("Unable to open public key file");
        return -1;
    }
    RSA *rsa_public_key = PEM_read_RSA_PUBKEY(public_key_file, NULL, NULL, NULL);
    fclose(public_key_file);
    if (!rsa_public_key) {
        fprintf(stderr, "Error loading public key\n");
        return -1 ;
    }
    // Read the signature
    unsigned char *signature = malloc(RSA_size(rsa_public_key));
    FILE *signature_file = fopen(signature_file_path, "rb");
    fread(signature, 1, RSA_size(rsa_public_key), signature_file);
    fclose(signature_file);
    int input_length = 98; //32byte(key)+16bYte(IV)+1byte(whitespace)+1 ;  
    unsigned char *decrypted_data = malloc(RSA_size(rsa_public_key));

    // Decrypt the signature to get the original data
    int decrypted_length = RSA_public_decrypt(RSA_size(rsa_public_key), signature, decrypted_data, rsa_public_key, RSA_PKCS1_PADDING);

    if (decrypted_length == -1) {
        fprintf(stderr, "Error decrypting signature\n");
        if (signature) free(signature);
        if (decrypted_data) free(decrypted_data);
        return -1 ;
     }

    // Write the content to a file
    FILE *output_file = fopen(output_path, "w+b");
    fwrite(decrypted_data, 1, input_length, output_file);
    fclose(output_file);

    // Clean up
    if (signature) free(signature);
    if (decrypted_data) free(decrypted_data);
    if (rsa_public_key) RSA_free(rsa_public_key);
    return  0;
}

struct swupdate_digest *swupdate_DECRYPT_init(unsigned char *key, char keylen, unsigned char *iv)
{
	struct swupdate_digest *dgst;
	const EVP_CIPHER *cipher;
	int ret;

	if ((key == NULL) || (iv == NULL)) {
		ERROR("no key provided for decryption!");
		return NULL;
	}

	switch (keylen) {
	case AES_128_KEY_LEN:
		cipher = EVP_aes_128_cbc();
		break;
	case AES_192_KEY_LEN:
		cipher = EVP_aes_192_cbc();
		break;
	case AES_256_KEY_LEN:
		cipher = EVP_aes_256_cbc();
		break;
	default:
		return NULL;
	}

	dgst = calloc(1, sizeof(*dgst));
	if (!dgst) {
		return NULL;
	}

#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
	EVP_CIPHER_CTX_init(&dgst->ctxdec);
#else
	dgst->ctxdec = EVP_CIPHER_CTX_new();
	if (dgst->ctxdec == NULL) {
		ERROR("Cannot initialize cipher context.");
		free(dgst);
		return NULL;
	}
	if (EVP_CIPHER_CTX_reset(dgst->ctxdec) != 1) {
		ERROR("Cannot reset cipher context.");
		EVP_CIPHER_CTX_free(dgst->ctxdec);
		free(dgst);
		return NULL;
	}
#endif

	/*
	 * Check openSSL documentation for return errors
	 */
	ret = EVP_DecryptInit_ex(SSL_GET_CTXDEC(dgst), cipher, NULL, key, iv);
	if (ret != 1) {
		const char *reason = ERR_reason_error_string(ERR_peek_error());
		ERROR("Decrypt Engine not initialized, error 0x%lx, reason: %s", ERR_get_error(),
			reason != NULL ? reason : "unknown");
		free(dgst);
		return NULL;
	}

	return dgst;
}

int swupdate_DECRYPT_update(struct swupdate_digest *dgst, unsigned char *buf, 
				int *outlen, const unsigned char *cryptbuf, int inlen)
{
	if (EVP_DecryptUpdate(SSL_GET_CTXDEC(dgst), buf, outlen, cryptbuf, inlen) != 1) {
		const char *reason = ERR_reason_error_string(ERR_peek_error());
		ERROR("Update: Decryption error 0x%lx, reason: %s", ERR_get_error(),
			reason != NULL ? reason : "unknown");
		return -EFAULT;
	}

	return 0;
}

int swupdate_DECRYPT_final(struct swupdate_digest *dgst, unsigned char *buf,
				int *outlen)
{
	if (!dgst)
		return -EINVAL;

	if (EVP_DecryptFinal_ex(SSL_GET_CTXDEC(dgst), buf, outlen) != 1) {
		const char *reason = ERR_reason_error_string(ERR_peek_error());
		ERROR("Final: Decryption error 0x%lx, reason: %s", ERR_get_error(),
			reason != NULL ? reason : "unknown");
		return -EFAULT;
	}

	return 0;

}

void swupdate_DECRYPT_cleanup(struct swupdate_digest *dgst)
{
	if (dgst) {
#if OPENSSL_VERSION_NUMBER < 0x10100000L || defined(LIBRESSL_VERSION_NUMBER)
		EVP_CIPHER_CTX_cleanup(SSL_GET_CTXDEC(dgst));
#else
		EVP_CIPHER_CTX_free(SSL_GET_CTXDEC(dgst));
#endif
		free(dgst);
		dgst = NULL;
	}
}
