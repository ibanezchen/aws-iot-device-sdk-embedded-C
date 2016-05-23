/*
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#define _DBG AWS_HYPERC_DBG
#include <hcos/dbg.h>
#include <stdbool.h>
#include <string.h>

#include "aws_iot_error.h"
#include "aws_iot_log.h"
#include "network_interface.h"
#include "network_platform.h"
#include "cert_platform.h"
#include "timer_platform.h"

/* This is the value used for ssl read timeout */
#define IOT_SSL_READ_TIMEOUT 10

/*
 * This is a function to do further verification if needed on the cert received
 */

static IoT_Error_t _iot_tls_verify_cert(void *data, mbedtls_x509_crt *crt, int depth, uint32_t *flags) {
	char buf[1024];
	((void) data);

	dbg("\nVerify requested for (Depth %d):\n", depth);
	mbedtls_x509_crt_info(buf, sizeof(buf) - 1, "", crt);
	dbg("%s", buf);

	if ((*flags) == 0) {
		dbg("  This certificate has no flags\n");
	} else {
		dbg(buf, sizeof(buf), "  ! ", *flags); 
		dbg("%s\n", buf);
	}

	return SUCCESS;
}

void _iot_tls_set_connect_params(Network *pNetwork, char *pRootCALocation, char *pDeviceCertLocation,
								 char *pDevicePrivateKeyLocation, char *pDestinationURL,
								 uint16_t destinationPort, uint32_t timeout_ms, bool ServerVerificationFlag) {
	pNetwork->tlsConnectParams.DestinationPort = destinationPort;
	pNetwork->tlsConnectParams.pDestinationURL = pDestinationURL;
	pNetwork->tlsConnectParams.pDeviceCertLocation = pDeviceCertLocation;
	pNetwork->tlsConnectParams.pDevicePrivateKeyLocation = pDevicePrivateKeyLocation;
	pNetwork->tlsConnectParams.pRootCALocation = pRootCALocation;
	pNetwork->tlsConnectParams.timeout_ms = timeout_ms;
	pNetwork->tlsConnectParams.ServerVerificationFlag = ServerVerificationFlag;
}

IoT_Error_t iot_tls_init(Network *pNetwork, char *pRootCALocation, char *pDeviceCertLocation,
						 char *pDevicePrivateKeyLocation, char *pDestinationURL,
						 uint16_t destinationPort, uint32_t timeout_ms, bool ServerVerificationFlag) {
	_iot_tls_set_connect_params(pNetwork, pRootCALocation, pDeviceCertLocation, pDevicePrivateKeyLocation,
								pDestinationURL, destinationPort, timeout_ms, ServerVerificationFlag);

	pNetwork->connect = iot_tls_connect;
	pNetwork->read = iot_tls_read;
	pNetwork->write = iot_tls_write;
	pNetwork->disconnect = iot_tls_disconnect;
	pNetwork->isConnected = iot_tls_is_connected;
	pNetwork->destroy = iot_tls_destroy;

	return SUCCESS;
}

IoT_Error_t iot_tls_is_connected(Network *pNetwork) {
	/* Use this to add implementation which can check for physical layer disconnect */
	return NETWORK_PHYSICAL_LAYER_CONNECTED;
}

int _printf(const char *str, ...);

void my_debug( void *ctx, int level,
                      const char *file, int line,
                      const char *str )
{
	const char *p, *basename;
	for( p = basename = file; *p != '\0'; p++ )
		if( *p == '/' || *p == '\\' )
			basename = p + 1;
	_printf("%s:%04d: |%d| %s", basename, line, level, str );
}

IoT_Error_t iot_tls_connect(Network *pNetwork, TLSConnectParams *params) {
	if(NULL == pNetwork) {
		return NULL_VALUE_ERROR;
	}

	if(NULL != params) {
		_iot_tls_set_connect_params(pNetwork, params->pRootCALocation, params->pDeviceCertLocation,
									params->pDevicePrivateKeyLocation, params->pDestinationURL,
									params->DestinationPort, params->timeout_ms, params->ServerVerificationFlag);
	}
	aws_key_t cert;
	int ret = 0;
	const char *pers = "aws_iot_tls_wrapper";
#ifdef IOT_DEBUG
	unsigned char buf[MBEDTLS_SSL_MAX_CONTENT_LEN + 1];
#endif
	TLSDataParams *tlsDataParams = &(pNetwork->tlsDataParams);

	mbedtls_net_init(&(tlsDataParams->server_fd));
	mbedtls_ssl_init(&(tlsDataParams->ssl));
	mbedtls_ssl_config_init(&(tlsDataParams->conf));
#if AWS_HYPERC_DBG > 1
	dbg("enable ssh debug\n");
	mbedtls_ssl_conf_dbg( &(tlsDataParams->conf), my_debug, 0 );
	mbedtls_debug_set_threshold(10);
#endif
	mbedtls_ctr_drbg_init(&(tlsDataParams->ctr_drbg));
	mbedtls_x509_crt_init(&(tlsDataParams->cacert));
	mbedtls_x509_crt_init(&(tlsDataParams->clicert));
	mbedtls_pk_init(&(tlsDataParams->pkey));

	dbg("\n  . Seeding the random number generator...");
	mbedtls_entropy_init(&(tlsDataParams->entropy));
	if ((ret = mbedtls_ctr_drbg_seed(&(tlsDataParams->ctr_drbg), mbedtls_entropy_func, &(tlsDataParams->entropy),
										(const unsigned char *) pers, strlen(pers))) != 0) {
		dbg(" failed\n  ! mbedtls_ctr_drbg_seed returned -0x%x\n", -ret);
		return NETWORK_MBEDTLS_ERR_CTR_DRBG_ENTROPY_SOURCE_FAILED;
	}

	dbg("  . Loading the CA root certificate ...");
	//ret = mbedtls_x509_crt_parse_file(&(tlsDataParams->cacert), pNetwork->tlsConnectParams.pRootCALocation);
	if( aws_key_init( &cert, AWS_CERT_ROOT) )
		ret = mbedtls_x509_crt_parse(&(tlsDataParams->cacert), cert.raw, cert.sz);
	else
		ret = -1;
	if (ret < 0) {
		dbg(" failed\n  !  mbedtls_x509_crt_parse returned -0x%x while parsing root cert\n\n", -ret);
		return NETWORK_X509_ROOT_CRT_PARSE_ERROR;
	}
	dbg(" ok (%d skipped)\n", ret);

	dbg("  . Loading the client cert. and key...");
	//ret = mbedtls_x509_crt_parse_file(&(tlsDataParams->clicert), pNetwork->tlsConnectParams.pDeviceCertLocation);
	if( aws_key_init( &cert, AWS_CERT_CLIENT) )
		ret = mbedtls_x509_crt_parse(&(tlsDataParams->clicert), cert.raw, cert.sz);
	else
		ret = -1;
	if (ret != 0) {
		dbg(" failed\n  !  mbedtls_x509_crt_parse returned -0x%x while parsing device cert\n\n", -ret);
		return NETWORK_X509_DEVICE_CRT_PARSE_ERROR;
	}

	//ret = mbedtls_pk_parse_keyfile(&(tlsDataParams->pkey), pNetwork->tlsConnectParams.pDevicePrivateKeyLocation, "");
	if( aws_key_init( &cert, AWS_PK) )
		ret = mbedtls_pk_parse_key( &(tlsDataParams->pkey), cert.raw, cert.sz,
                (const unsigned char *) "", strlen( "" ) );
	else
		ret = -1;
	if (ret != 0) {
		dbg(" failed\n  !  mbedtls_pk_parse_key returned -0x%x while parsing private key\n\n", -ret);
		dbg(" path : %s ", pNetwork->tlsConnectParams.pDevicePrivateKeyLocation);
		return NETWORK_PK_PRIVATE_KEY_PARSE_ERROR;
	} dbg(" ok\n");
	char portBuffer[6];
	snprintf(portBuffer, 6, "%d", pNetwork->tlsConnectParams.DestinationPort);
	dbg("  . Connecting to %s/%s...", pNetwork->tlsConnectParams.pDestinationURL, portBuffer);
	if ((ret = mbedtls_net_connect(&(tlsDataParams->server_fd), pNetwork->tlsConnectParams.pDestinationURL,
								   portBuffer, MBEDTLS_NET_PROTO_TCP)) != 0) {
		dbg(" failed\n  ! mbedtls_net_connect returned -0x%x\n\n", -ret);
		switch(ret) {
			case MBEDTLS_ERR_NET_SOCKET_FAILED:
				return NETWORK_ERR_NET_SOCKET_FAILED;
			case MBEDTLS_ERR_NET_UNKNOWN_HOST:
				return NETWORK_ERR_NET_UNKNOWN_HOST;
			case MBEDTLS_ERR_NET_CONNECT_FAILED:
			default:
				return NETWORK_ERR_NET_CONNECT_FAILED;
		};
	}

	ret = mbedtls_net_set_block(&(tlsDataParams->server_fd));
	if (ret != 0) {
		dbg(" failed\n  ! net_set_(non)block() returned -0x%x\n\n", -ret);
		return SSL_CONNECTION_ERROR;
	} dbg(" ok\n");

	dbg("  . Setting up the SSL/TLS structure...");
	if ((ret = mbedtls_ssl_config_defaults(&(tlsDataParams->conf), MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
			MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		dbg(" failed\n  ! mbedtls_ssl_config_defaults returned -0x%x\n\n", -ret);
		return SSL_CONNECTION_ERROR;
	}

	mbedtls_ssl_conf_verify(&(tlsDataParams->conf), (void*)_iot_tls_verify_cert, NULL);
	if (pNetwork->tlsConnectParams.ServerVerificationFlag == true) {
		mbedtls_ssl_conf_authmode(&(tlsDataParams->conf), MBEDTLS_SSL_VERIFY_REQUIRED);
	} else {
		mbedtls_ssl_conf_authmode(&(tlsDataParams->conf), MBEDTLS_SSL_VERIFY_OPTIONAL);
	}
	mbedtls_ssl_conf_rng(&(tlsDataParams->conf), mbedtls_ctr_drbg_random, &(tlsDataParams->ctr_drbg));

	mbedtls_ssl_conf_ca_chain(&(tlsDataParams->conf), &(tlsDataParams->cacert), NULL);
	if ((ret = mbedtls_ssl_conf_own_cert(&(tlsDataParams->conf), &(tlsDataParams->clicert), &(tlsDataParams->pkey))) != 0) {
		dbg(" failed\n  ! mbedtls_ssl_conf_own_cert returned %d\n\n", ret);
		return SSL_CONNECTION_ERROR;
	}

	mbedtls_ssl_conf_read_timeout(&(tlsDataParams->conf), pNetwork->tlsConnectParams.timeout_ms);

	if ((ret = mbedtls_ssl_setup(&(tlsDataParams->ssl), &(tlsDataParams->conf))) != 0) {
		dbg(" failed\n  ! mbedtls_ssl_setup returned -0x%x\n\n", -ret);
		return SSL_CONNECTION_ERROR;
	}
	if ((ret = mbedtls_ssl_set_hostname(&(tlsDataParams->ssl), pNetwork->tlsConnectParams.pDestinationURL)) != 0) {
		dbg(" failed\n  ! mbedtls_ssl_set_hostname returned %d\n\n", ret);
		return SSL_CONNECTION_ERROR;
	}
	dbg("\n\nSSL state connect : %d ", tlsDataParams->ssl.state);
	mbedtls_ssl_set_bio(&(tlsDataParams->ssl), &(tlsDataParams->server_fd), mbedtls_net_send, NULL, mbedtls_net_recv_timeout);
	dbg(" ok\n");

	dbg("\n\nSSL state connect : %d ", tlsDataParams->ssl.state);
	dbg("  . Performing the SSL/TLS handshake...");
	while ((ret = mbedtls_ssl_handshake(&(tlsDataParams->ssl))) != 0) {
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			dbg(" failed\n  ! mbedtls_ssl_handshake returned -0x%x\n", -ret);
			if (ret == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
				dbg("    Unable to verify the server's certificate. "
						"Either it is invalid,\n"
						"    or you didn't set ca_file or ca_path "
						"to an appropriate value.\n"
						"    Alternatively, you may want to use "
						"auth_mode=optional for testing purposes.\n");
			}
			return SSL_CONNECTION_ERROR;
		}
	}

	dbg(" ok\n    [ Protocol is %s ]\n    [ Ciphersuite is %s ]\n", mbedtls_ssl_get_version(&(tlsDataParams->ssl)), mbedtls_ssl_get_ciphersuite(&(tlsDataParams->ssl)));
	if ((ret = mbedtls_ssl_get_record_expansion(&(tlsDataParams->ssl))) >= 0) {
		dbg("    [ Record expansion is %d ]\n", ret);
	} else {
		dbg("    [ Record expansion is unknown (compression) ]\n");
	}

	dbg("  . Verifying peer X.509 certificate...");

	if(pNetwork->tlsConnectParams.ServerVerificationFlag == true) {
		if((tlsDataParams->flags = mbedtls_ssl_get_verify_result(&(tlsDataParams->ssl))) != 0) {
			char vrfy_buf[512];
			dbg(" failed\n");
			mbedtls_x509_crt_verify_info(vrfy_buf, sizeof(vrfy_buf), "  ! ", tlsDataParams->flags);
			dbg("%s\n", vrfy_buf);
			ret = SSL_CONNECTION_ERROR;
		} else {
			dbg(" ok\n");
			ret = SUCCESS;
		}
	} else {
		dbg(" Server Verification skipped\n");
		ret = SUCCESS;
	}

#ifdef IOT_DEBUG
	if (mbedtls_ssl_get_peer_cert(&(tlsDataParams->ssl)) != NULL) {
		dbg("  . Peer certificate information    ...\n");
		mbedtls_x509_crt_info((char *) buf, sizeof(buf) - 1, "      ", mbedtls_ssl_get_peer_cert(&(tlsDataParams->ssl)));
		dbg("%s\n", buf);
	}
#endif

	mbedtls_ssl_conf_read_timeout(&(tlsDataParams->conf), IOT_SSL_READ_TIMEOUT);

	return ret;
}

IoT_Error_t iot_tls_write(Network *pNetwork, unsigned char *pMsg, size_t len, Timer *timer, size_t *written_len) {
	size_t written_so_far;
	bool isErrorFlag = false;
	int frags, ret = 0;
	TLSDataParams *tlsDataParams = &(pNetwork->tlsDataParams);

	for(written_so_far = 0, frags = 0; written_so_far < len && !has_timer_expired(timer); written_so_far += ret, frags++) {
		while(!has_timer_expired(timer) && (ret = mbedtls_ssl_write(&(tlsDataParams->ssl), pMsg + written_so_far, len - written_so_far)) <= 0) {
			if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
				dbg(" failed\n  ! mbedtls_ssl_write returned -0x%x\n\n", -ret);
				/* All other negative return values indicate connection needs to be reset.
		 		* Will be caught in ping request so ignored here */
				isErrorFlag = true;
				break;
			}
		}
		if(isErrorFlag) {
			break;
		}
	}

	*written_len = written_so_far;

	if(isErrorFlag) {
		return NETWORK_SSL_WRITE_ERROR;
	} else if(has_timer_expired(timer) && written_so_far != len) {
		return NETWORK_SSL_WRITE_TIMEOUT_ERROR;
	}

	return SUCCESS;
}

IoT_Error_t iot_tls_read(Network *pNetwork, unsigned char *pMsg, size_t len, Timer *timer, size_t *read_len) {
	size_t rxLen = 0;
	bool isErrorFlag = false;
	bool isCompleteFlag = false;
	uint32_t timerLeftVal = left_ms(timer);
	TLSDataParams *tlsDataParams = &(pNetwork->tlsDataParams);
	int ret = 0;

	do {
		//mbedtls_ssl_conf_read_timeout(&(tlsDataParams->conf), timerLeftVal);
		ret = mbedtls_ssl_read(&(tlsDataParams->ssl), pMsg, len);
		if (ret >= 0) { /* 0 is for EOF */
			rxLen += ret;
		} else if (ret != MBEDTLS_ERR_SSL_WANT_READ) {
			isErrorFlag = true;
		}

		/* All other negative return values indicate connection needs to be reset.
		 * Will be caught in ping request so ignored here */

		if (rxLen >= len) {
			isCompleteFlag = true;
		}
		timerLeftVal = left_ms(timer);
	} while(!isErrorFlag && !isCompleteFlag && timerLeftVal > 0);

	*read_len = rxLen;

	if(0 == rxLen && isErrorFlag) {
		return NETWORK_SSL_NOTHING_TO_READ;
	} else if(has_timer_expired(timer) && !isCompleteFlag) {
		return NETWORK_SSL_READ_TIMEOUT_ERROR;
	}

	return SUCCESS;
}

IoT_Error_t iot_tls_disconnect(Network *pNetwork) {
	mbedtls_ssl_context *ssl = &(pNetwork->tlsDataParams.ssl);
	int ret = 0;
	do {
		ret = mbedtls_ssl_close_notify(ssl);
	} while (ret == MBEDTLS_ERR_SSL_WANT_WRITE);

	/* All other negative return values indicate connection needs to be reset.
	 * No further action required since this is disconnect call */

	return SUCCESS;
}

IoT_Error_t iot_tls_destroy(Network *pNetwork) {
	TLSDataParams *tlsDataParams = &(pNetwork->tlsDataParams);

	mbedtls_net_free(&(tlsDataParams->server_fd));

	mbedtls_x509_crt_free(&(tlsDataParams->clicert));
	mbedtls_x509_crt_free(&(tlsDataParams->cacert));
	mbedtls_pk_free(&(tlsDataParams->pkey));
	mbedtls_ssl_free(&(tlsDataParams->ssl));
	mbedtls_ssl_config_free(&(tlsDataParams->conf));
	mbedtls_ctr_drbg_free(&(tlsDataParams->ctr_drbg));
	mbedtls_entropy_free(&(tlsDataParams->entropy));

	return SUCCESS;
}
