/*
 * Copyright (C) 2026 VectorCore Inc.
 *
 * Portions of IMSI/APN extraction are derived from the Osmocom osmo_epdg
 * strongSwan plugin by sysmocom - s.f.m.c. GmbH.
 *
 * SPDX-License-Identifier: GPL-2.0+
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "vectorcore_eap_proxy.h"
#include "vectorcore_eap_proxy_ipc.h"

#include <daemon.h>
#include <encoding/payloads/eap_payload.h>
#include <sa/ike_sa_id.h>
#include <utils/debug.h>

#include <errno.h>
#include <ctype.h>
#include <inttypes.h>

#define VECTORCORE_EAP_PROXY_TIMEOUT_MS 10000
#define VECTORCORE_EAP_PROXY_APN_MAXLEN 256

typedef struct private_vectorcore_eap_proxy_t private_vectorcore_eap_proxy_t;

struct private_vectorcore_eap_proxy_t {
	vectorcore_eap_proxy_t public;
	identification_t *peer;
	auth_cfg_t *auth;
	char *socket_path;
	char *session_id;
	uint8_t identifier;
	chunk_t msk;
	chunk_t emsk;
	eap_type_t type;
	uint32_t vendor;
};

static char *format_session_id(uint64_t spi_i, uint64_t spi_r, char *fallback)
{
	char buf[48];

	if (!spi_i && !spi_r)
	{
		return fallback ? strdup(fallback) : NULL;
	}
	snprintf(buf, sizeof(buf), "%016" PRIx64 "-%016" PRIx64, spi_i, spi_r);
	return strdup(buf);
}

static char *chunk_to_cstr(chunk_t chunk)
{
	char *str = malloc(chunk.len + 1);

	if (!str)
	{
		return NULL;
	}
	memcpy(str, chunk.ptr, chunk.len);
	str[chunk.len] = '\0';
	return str;
}

static char *peer_nai(private_vectorcore_eap_proxy_t *this)
{
	return chunk_to_cstr(this->peer->get_encoding(this->peer));
}

static char *peer_imsi(private_vectorcore_eap_proxy_t *this)
{
	chunk_t nai = this->peer->get_encoding(this->peer);
	char *imsi;
	size_t len = min(15, nai.len > 0 ? nai.len - 1 : 0);

	if (nai.len < 16 || nai.ptr[0] != '0')
	{
		return NULL;
	}
	imsi = malloc(len + 1);
	if (!imsi)
	{
		return NULL;
	}
	memcpy(imsi, nai.ptr + 1, len);
	imsi[len] = '\0';
	for (size_t i = 0; i < len; i++)
	{
		if (!isdigit((unsigned char)imsi[i]))
		{
			free(imsi);
			return NULL;
		}
	}
	return imsi;
}

static char *ike_apn()
{
	ike_sa_t *ike_sa;
	identification_t *id;
	chunk_t enc;

	ike_sa = charon->bus->get_sa(charon->bus);
	if (!ike_sa)
	{
		return NULL;
	}
	id = ike_sa->get_my_id(ike_sa);
	if (!id)
	{
		return NULL;
	}
	enc = id->get_encoding(id);
	if (!enc.len || enc.len >= VECTORCORE_EAP_PROXY_APN_MAXLEN)
	{
		return NULL;
	}
	return chunk_to_cstr(enc);
}

static char *configured_socket_path()
{
	return strdup(lib->settings->get_str(lib->settings,
					"%s.plugins.vectorcore-eap-proxy.socket",
					VECTORCORE_EAP_PROXY_DEFAULT_SOCKET, lib->ns));
}

static void populate_request(private_vectorcore_eap_proxy_t *this,
							 vectorcore_eap_proxy_message_t *request,
							 vectorcore_eap_proxy_msg_type_t type,
							 chunk_t eap_payload)
{
	ike_sa_t *ike_sa;

	request->version = VECTORCORE_EAP_PROXY_VERSION;
	request->type = type;
	request->nai = peer_nai(this);
	request->imsi = peer_imsi(this);
	request->apn = ike_apn();
	request->eap_payload = eap_payload;
	ike_sa = charon->bus->get_sa(charon->bus);
	if (ike_sa)
	{
		ike_sa_id_t *id = ike_sa->get_id(ike_sa);
		if (id)
		{
			request->ike_spi_i = id->get_initiator_spi(id);
			request->ike_spi_r = id->get_responder_spi(id);
		}
	}
	request->session_id = format_session_id(request->ike_spi_i,
											request->ike_spi_r,
											this->session_id);
}

static status_t handle_response(private_vectorcore_eap_proxy_t *this,
								vectorcore_eap_proxy_message_t *response,
								eap_payload_t **out)
{
	switch (response->status)
	{
		case VECTORCORE_EAP_PROXY_STATUS_CONTINUE:
			if (!response->eap_payload.ptr || !response->eap_payload.len)
			{
				DBG1(DBG_IKE, "VectorCore EAP proxy continue without EAP payload");
				return FAILED;
			}
			*out = eap_payload_create_data(response->eap_payload);
			this->type = (*out)->get_type(*out, &this->vendor);
			return NEED_MORE;
		case VECTORCORE_EAP_PROXY_STATUS_SUCCESS:
			if (!response->msk.ptr || response->msk.len != 64)
			{
				DBG1(DBG_IKE, "VectorCore EAP proxy success without 64-byte MSK");
				return FAILED;
			}
			chunk_clear(&this->msk);
			chunk_clear(&this->emsk);
			this->msk = chunk_clone(response->msk);
			if (response->emsk.ptr && response->emsk.len)
			{
				this->emsk = chunk_clone(response->emsk);
			}
			this->auth->add(this->auth, AUTH_RULE_EAP_IDENTITY,
							this->peer->clone(this->peer));
			return SUCCESS;
		case VECTORCORE_EAP_PROXY_STATUS_FAILURE:
			DBG1(DBG_IKE, "VectorCore EAP proxy authentication failed");
			return FAILED;
		case VECTORCORE_EAP_PROXY_STATUS_ERROR:
		default:
			DBG1(DBG_IKE, "VectorCore EAP proxy error: %s",
				 response->error_code ?: "unknown");
			return FAILED;
	}
}

static status_t exchange(private_vectorcore_eap_proxy_t *this,
						 vectorcore_eap_proxy_msg_type_t type,
						 chunk_t eap_payload, eap_payload_t **out)
{
	vectorcore_eap_proxy_message_t request = {}, response = {};
	status_t status = FAILED;

	populate_request(this, &request, type, eap_payload);
	if (!request.imsi)
	{
		DBG1(DBG_IKE, "VectorCore EAP proxy could not derive IMSI from peer identity");
		goto out;
	}
	if (!vectorcore_eap_proxy_exchange(this->socket_path, &request, &response,
									   VECTORCORE_EAP_PROXY_TIMEOUT_MS))
	{
		DBG1(DBG_IKE, "VectorCore EAP proxy IPC exchange failed");
		goto out;
	}
	status = handle_response(this, &response, out);
out:
	free(request.session_id);
	free(request.imsi);
	free(request.nai);
	free(request.apn);
	vectorcore_eap_proxy_message_clear(&response);
	return status;
}

METHOD(eap_method_t, initiate, status_t,
	private_vectorcore_eap_proxy_t *this, eap_payload_t **out)
{
	return exchange(this, VECTORCORE_EAP_PROXY_AUTH_START, chunk_empty, out);
}

METHOD(eap_method_t, process, status_t,
	private_vectorcore_eap_proxy_t *this, eap_payload_t *in, eap_payload_t **out)
{
	return exchange(this, VECTORCORE_EAP_PROXY_AUTH_CONTINUE, in->get_data(in), out);
}

METHOD(eap_method_t, get_type, eap_type_t,
	private_vectorcore_eap_proxy_t *this, uint32_t *vendor)
{
	*vendor = this->vendor;
	return this->type;
}

METHOD(eap_method_t, get_auth, auth_cfg_t*,
	private_vectorcore_eap_proxy_t *this)
{
	return this->auth;
}

METHOD(eap_method_t, get_msk, status_t,
	private_vectorcore_eap_proxy_t *this, chunk_t *msk)
{
	if (this->msk.ptr)
	{
		*msk = this->msk;
		return SUCCESS;
	}
	return FAILED;
}

METHOD(eap_method_t, get_identifier, uint8_t,
	private_vectorcore_eap_proxy_t *this)
{
	return this->identifier;
}

METHOD(eap_method_t, set_identifier, void,
	private_vectorcore_eap_proxy_t *this, uint8_t identifier)
{
	this->identifier = identifier;
}

METHOD(eap_method_t, is_mutual, bool,
	private_vectorcore_eap_proxy_t *this)
{
	return TRUE;
}

METHOD(eap_method_t, destroy, void,
	private_vectorcore_eap_proxy_t *this)
{
	free(this->socket_path);
	free(this->session_id);
	chunk_clear(&this->msk);
	chunk_clear(&this->emsk);
	this->peer->destroy(this->peer);
	this->auth->destroy(this->auth);
	free(this);
}

vectorcore_eap_proxy_t *vectorcore_eap_proxy_create(identification_t *server,
													identification_t *peer)
{
	private_vectorcore_eap_proxy_t *this;

	INIT(this,
		.public = {
			.eap_method = {
				.initiate = _initiate,
				.process = _process,
				.get_type = _get_type,
				.is_mutual = _is_mutual,
				.get_msk = _get_msk,
				.get_auth = _get_auth,
				.get_identifier = _get_identifier,
				.set_identifier = _set_identifier,
				.destroy = _destroy,
			},
		},
		.peer = peer->clone(peer),
		.auth = auth_cfg_create(),
		.socket_path = configured_socket_path(),
		.session_id = chunk_to_cstr(peer->get_encoding(peer)),
		.type = EAP_AKA,
	);

	return &this->public;
}
