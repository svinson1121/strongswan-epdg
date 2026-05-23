/*
 * Copyright (C) 2026 VectorCore Inc.
 *
 * SPDX-License-Identifier: GPL-2.0+
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef VECTORCORE_EAP_PROXY_H_
#define VECTORCORE_EAP_PROXY_H_

typedef struct vectorcore_eap_proxy_t vectorcore_eap_proxy_t;

#include <sa/eap/eap_method.h>

struct vectorcore_eap_proxy_t {
	eap_method_t eap_method;
};

vectorcore_eap_proxy_t *vectorcore_eap_proxy_create(identification_t *server,
													identification_t *peer);

#endif /** VECTORCORE_EAP_PROXY_H_ @}*/
