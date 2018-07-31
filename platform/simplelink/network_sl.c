/*
 * Copyright Swan Solutions Inc. All Rights Reserved.
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

#include <network_interface.h>
#include <aws_iot_error.h>

#include "simplelink.h"

#define SOCKET_TIMEOUT_VAL 5000		// Timeout period while waiting for non-blocking socket APIs
#define SOCKET_POLL_INTERVAL 100		// Polling interval for non-blocking socket APIs

extern uint32_t NetWiFi_isConnected(void);

IoT_Error_t iot_tls_init(Network *pNetwork, char *pRootCALocation,
        char *pDeviceCertLocation, char *pDevicePrivateKeyLocation,
        char *pDestinationURL, uint16_t DestinationPort, uint32_t timeout_ms,
        bool ServerVerificationFlag)
{
    if (pNetwork == NULL) {
        return NULL_VALUE_ERROR;
    }

    // Init TLS parameters
    pNetwork->tlsConnectParams.DestinationPort = DestinationPort;
    pNetwork->tlsConnectParams.pDestinationURL = pDestinationURL;
    pNetwork->tlsConnectParams.pDeviceCertLocation = pDeviceCertLocation;
    pNetwork->tlsConnectParams.pDevicePrivateKeyLocation = pDevicePrivateKeyLocation;
    pNetwork->tlsConnectParams.pRootCALocation = pRootCALocation;
    pNetwork->tlsConnectParams.timeout_ms = timeout_ms;
    pNetwork->tlsConnectParams.ServerVerificationFlag = ServerVerificationFlag;

    pNetwork->connect = iot_tls_connect;
    pNetwork->read = iot_tls_read;
    pNetwork->write = iot_tls_write;
    pNetwork->disconnect = iot_tls_disconnect;
    pNetwork->isConnected = iot_tls_is_connected;
    pNetwork->destroy = iot_tls_destroy;

    pNetwork->tlsDataParams.ssock = NULL;

    return SUCCESS;
}

IoT_Error_t iot_tls_connect(Network *pNetwork, TLSConnectParams *TLSParams)
{
    IoT_Error_t rc = SUCCESS;
    unsigned long ip;
    int skt = 0;
    SlSockAddrIn_t address;
    TLSConnectParams *tlsParams;
    long lRetVal = -1;
    long lNonBlocking = 1;
    long timeout = SOCKET_TIMEOUT_VAL;

    if (pNetwork == NULL) {
        return (NULL_VALUE_ERROR);
    }

    // Use TLS params in Network struct
    tlsParams = &pNetwork->tlsConnectParams;

    // Open a secure socket
    skt = sl_Socket(SL_AF_INET,SL_SOCK_STREAM, SL_SEC_SOCKET);
    if (skt < 0) return NETWORK_ERR_NET_SOCKET_FAILED;

    // Configure socket to be non-blocking
    lRetVal = sl_SetSockOpt(skt,
    						SL_SOL_SOCKET,
    						SL_SO_NONBLOCKING,
    						&lNonBlocking,
                            sizeof(lNonBlocking));
    if(lRetVal < 0) {
        rc = NETWORK_SSL_INIT_ERROR;
        goto QUIT;
    }

    // Configure the socket with CA certificate - for server verification
    lRetVal = sl_SetSockOpt(skt,
    						SL_SOL_SOCKET,
    						SL_SO_SECURE_FILES_CA_FILE_NAME,
    						tlsParams->pRootCALocation,
    						strlen(tlsParams->pRootCALocation));
    if(lRetVal < 0) {
    	rc = NETWORK_SSL_INIT_ERROR;
        goto QUIT;
    }

    // Configure the socket with client certificate
    lRetVal = sl_SetSockOpt(skt,
    						SL_SOL_SOCKET,
    						SL_SO_SECURE_FILES_CERTIFICATE_FILE_NAME,
    						tlsParams->pDeviceCertLocation,
    						strlen(tlsParams->pDeviceCertLocation));
    if(lRetVal < 0) {
    	rc = NETWORK_SSL_INIT_ERROR;
        goto QUIT;
    }

    // Configure the socket with private key
    lRetVal = sl_SetSockOpt(skt,
    						SL_SOL_SOCKET,
    						SL_SO_SECURE_FILES_PRIVATE_KEY_FILE_NAME,
    						tlsParams->pDevicePrivateKeyLocation,
    						strlen(tlsParams->pDevicePrivateKeyLocation));
    if(lRetVal < 0) {
    	rc = NETWORK_SSL_INIT_ERROR;
        goto QUIT;
    }

    // Securely verify domain name
    lRetVal = sl_SetSockOpt(skt,
    						SL_SOL_SOCKET,
    						SO_SECURE_DOMAIN_NAME_VERIFICATION,
    						tlsParams->pDestinationURL,
    						strlen(tlsParams->pDestinationURL));
    if(lRetVal < 0) {
    	rc = NETWORK_ERR_NET_UNKNOWN_HOST;
        goto QUIT;
    }

    // Get host IP
    lRetVal = sl_NetAppDnsGetHostByName((signed char *)tlsParams->pDestinationURL,
    									strlen(tlsParams->pDestinationURL),
    									(unsigned long*)&ip,
    									SL_AF_INET);
    if(lRetVal < 0) {
    	rc = NETWORK_ERR_NET_UNKNOWN_HOST;
        goto QUIT;
    }

    // Configure host address
    address.sin_family = SL_AF_INET;
    address.sin_port = sl_Htons(tlsParams->DestinationPort);
    address.sin_addr.s_addr = sl_Htonl(ip);

    // Connect to server
    while(1) {
		lRetVal = sl_Connect(skt, (SlSockAddr_t *)&address, sizeof(address));
		if(lRetVal != SL_EALREADY) break;

		// Wait a bit
		osi_Sleep(SOCKET_POLL_INTERVAL);

		// Check for timeout while sending
		timeout -= SOCKET_POLL_INTERVAL;
		if(timeout <= 0) break;
    }
	if(lRetVal < 0) {
		rc = NETWORK_ERR_NET_CONNECT_FAILED;
		goto QUIT;
	}

QUIT:
	if(rc == SUCCESS) {
		// Store successful socket connection
		pNetwork->tlsDataParams.ssock = skt;
	}
	else {
		// Free socket memory if socket was opened
		if (skt >= 0) sl_Close(skt);

		// Clear socket handle from network parameters
		pNetwork->tlsDataParams.ssock = NULL;
	}

	// Return
	return rc;
}

IoT_Error_t iot_tls_is_connected(Network *pNetwork)
{
    return ((IoT_Error_t)NetWiFi_isConnected());
}

IoT_Error_t iot_tls_write(Network *pNetwork, unsigned char *pMsg, size_t len,
            Timer *timer, size_t *numbytes)
{
    int ssock = NULL;
    int bytes = 0;

    if (pNetwork == NULL || pMsg == NULL ||
            pNetwork->tlsDataParams.ssock == NULL || numbytes == NULL) {
        return NULL_VALUE_ERROR;
    }

    ssock = pNetwork->tlsDataParams.ssock;

    int timeout = SOCKET_TIMEOUT_VAL;
    while(1) {
    	// Try to send data over socket
		bytes = sl_Send(ssock, pMsg, len, 0);
		if(bytes != SL_EAGAIN) break;

		// Wait a bit
		osi_Sleep(SOCKET_POLL_INTERVAL);

		// Check for timeout while sending
		timeout -= SOCKET_POLL_INTERVAL;
		if(timeout <= 0) {
			*numbytes = 0;
			return NETWORK_SSL_WRITE_ERROR;
		}
    }
	if (bytes > 0) {
		*numbytes = (size_t)bytes;
		return SUCCESS;
	}

    return NETWORK_SSL_WRITE_ERROR;
}

IoT_Error_t iot_tls_read(Network *pNetwork, unsigned char *pMsg, size_t len,
        Timer *timer, size_t *numbytes)
{
    int bytes = 0;
    SlTimeval_t tv;
    int ssock = NULL;
    uint32_t timeout;

    if (pNetwork == NULL || pMsg == NULL ||
            pNetwork->tlsDataParams.ssock == NULL || timer == NULL ||
            numbytes == NULL) {
        return (NULL_VALUE_ERROR);
    }

    ssock = pNetwork->tlsDataParams.ssock;

    timeout = left_ms(timer);
    if (timeout == 0) {
        /* sock timeout of 0 == block forever; just read + return if expired */
        timeout = 1;
    }

    tv.tv_sec = 0;
    tv.tv_usec = timeout * 1000;
    if (sl_SetSockOpt(ssock, SL_SOL_SOCKET, SL_SO_RCVTIMEO, (char *)&tv, sizeof(tv)) == 0) {
        bytes = sl_Recv(ssock, pMsg, len, 0);
        if (bytes > 0) {
            *numbytes = (size_t)bytes;
            return SUCCESS;
        }
        else if (bytes == SL_EAGAIN) {
            // nothing to read in the socket buffer
            return NETWORK_SSL_NOTHING_TO_READ;
        }
    }
    return NETWORK_SSL_READ_ERROR;
}

IoT_Error_t iot_tls_disconnect(Network *pNetwork)
{
    int ssock = NULL;

    if (pNetwork == NULL || pNetwork->tlsDataParams.ssock == NULL) {
        return (NULL_VALUE_ERROR);
    }

    ssock = pNetwork->tlsDataParams.ssock;
    sl_Close(ssock);

    return SUCCESS;
}

IoT_Error_t iot_tls_destroy(Network *pNetwork)
{
    if (pNetwork == NULL) {
        return (NULL_VALUE_ERROR);
    }

    pNetwork->connect = NULL;
    pNetwork->read = NULL;
    pNetwork->write = NULL;
    pNetwork->disconnect = NULL;
    pNetwork->isConnected = NULL;
    pNetwork->destroy = NULL;

    return SUCCESS;
}
