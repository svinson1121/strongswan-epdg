/*
 * Copyright (C) 2026 VectorCore Inc.
 *
 * SPDX-License-Identifier: GPL-2.0+
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef VECTORCORE_EAP_PROXY_IPC_H_
#define VECTORCORE_EAP_PROXY_IPC_H_

#include <library.h>

#define VECTORCORE_EAP_PROXY_DEFAULT_SOCKET "/run/vectorcore/epdg-eap.sock"
#define VECTORCORE_EAP_PROXY_VERSION 1

typedef enum {
	VECTORCORE_EAP_PROXY_AUTH_START = 1,
	VECTORCORE_EAP_PROXY_AUTH_CONTINUE = 2,
	VECTORCORE_EAP_PROXY_AUTH_RESULT = 3,
	VECTORCORE_EAP_PROXY_CANCEL = 4,
} vectorcore_eap_proxy_msg_type_t;

typedef enum {
	VECTORCORE_EAP_PROXY_STATUS_CONTINUE = 1,
	VECTORCORE_EAP_PROXY_STATUS_SUCCESS = 2,
	VECTORCORE_EAP_PROXY_STATUS_FAILURE = 3,
	VECTORCORE_EAP_PROXY_STATUS_ERROR = 4,
} vectorcore_eap_proxy_status_t;

typedef struct vectorcore_eap_proxy_message_t vectorcore_eap_proxy_message_t;

struct vectorcore_eap_proxy_message_t {
	uint32_t version;
	vectorcore_eap_proxy_msg_type_t type;
	char *session_id;
	char *imsi;
	char *nai;
	char *apn;
	uint64_t ike_spi_i;
	uint64_t ike_spi_r;
	chunk_t eap_payload;
	vectorcore_eap_proxy_status_t status;
	chunk_t msk;
	chunk_t emsk;
	uint32_t key_lifetime_seconds;
	char *error_code;
	char *error_message;
};

void vectorcore_eap_proxy_message_clear(vectorcore_eap_proxy_message_t *msg);

bool vectorcore_eap_proxy_exchange(char *socket_path,
								   vectorcore_eap_proxy_message_t *request,
								   vectorcore_eap_proxy_message_t *response,
								   uint32_t timeout_ms);

#endif /** VECTORCORE_EAP_PROXY_IPC_H_ @}*/
