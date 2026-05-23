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

#ifndef VECTORCORE_EAP_PROXY_PLUGIN_H_
#define VECTORCORE_EAP_PROXY_PLUGIN_H_

#include <plugins/plugin.h>

typedef struct vectorcore_eap_proxy_plugin_t vectorcore_eap_proxy_plugin_t;

struct vectorcore_eap_proxy_plugin_t {
	plugin_t plugin;
};

plugin_t *vectorcore_eap_proxy_plugin_create();

#endif /** VECTORCORE_EAP_PROXY_PLUGIN_H_ @}*/
