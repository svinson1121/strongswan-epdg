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

#include "vectorcore_eap_proxy_plugin.h"
#include "vectorcore_eap_proxy.h"

#include <daemon.h>

typedef struct private_vectorcore_eap_proxy_plugin_t private_vectorcore_eap_proxy_plugin_t;

struct private_vectorcore_eap_proxy_plugin_t {
	vectorcore_eap_proxy_plugin_t public;
};

METHOD(plugin_t, get_name, char*,
	private_vectorcore_eap_proxy_plugin_t *this)
{
	return "vectorcore-eap-proxy";
}

METHOD(plugin_t, get_features, int,
	private_vectorcore_eap_proxy_plugin_t *this, plugin_feature_t *features[])
{
	static plugin_feature_t f[] = {
		PLUGIN_CALLBACK(eap_method_register, vectorcore_eap_proxy_create),
			PLUGIN_PROVIDE(EAP_SERVER, EAP_AKA),
	};
	*features = f;
	return countof(f);
}

METHOD(plugin_t, destroy, void,
	private_vectorcore_eap_proxy_plugin_t *this)
{
	free(this);
}

plugin_t *vectorcore_eap_proxy_plugin_create()
{
	private_vectorcore_eap_proxy_plugin_t *this;

	INIT(this,
		.public = {
			.plugin = {
				.get_name = _get_name,
				.get_features = _get_features,
				.destroy = _destroy,
			},
		},
	);

	return &this->public.plugin;
}
