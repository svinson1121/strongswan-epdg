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

#include "vectorcore_eap_proxy_ipc.h"

#include <daemon.h>

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

typedef struct {
	uint8_t *ptr;
	size_t len;
	size_t cap;
} buf_t;

static bool reserve(buf_t *buf, size_t add)
{
	size_t cap;
	uint8_t *ptr;

	if (buf->len + add <= buf->cap)
	{
		return TRUE;
	}
	cap = buf->cap ?: 256;
	while (cap < buf->len + add)
	{
		cap *= 2;
	}
	ptr = realloc(buf->ptr, cap);
	if (!ptr)
	{
		return FALSE;
	}
	buf->ptr = ptr;
	buf->cap = cap;
	return TRUE;
}

static bool put_byte(buf_t *buf, uint8_t v)
{
	if (!reserve(buf, 1))
	{
		return FALSE;
	}
	buf->ptr[buf->len++] = v;
	return TRUE;
}

static bool put_varint(buf_t *buf, uint64_t v)
{
	while (v >= 0x80)
	{
		if (!put_byte(buf, (v & 0x7f) | 0x80))
		{
			return FALSE;
		}
		v >>= 7;
	}
	return put_byte(buf, v);
}

static bool put_bytes(buf_t *buf, uint32_t field, chunk_t data)
{
	if (!data.ptr || !data.len)
	{
		return TRUE;
	}
	return put_varint(buf, (field << 3) | 2) &&
		   put_varint(buf, data.len) &&
		   reserve(buf, data.len) &&
		   (memcpy(buf->ptr + buf->len, data.ptr, data.len), buf->len += data.len, TRUE);
}

static bool put_string(buf_t *buf, uint32_t field, char *str)
{
	if (!str || !*str)
	{
		return TRUE;
	}
	return put_bytes(buf, field, chunk_create(str, strlen(str)));
}

static bool put_uint(buf_t *buf, uint32_t field, uint64_t v)
{
	if (!v)
	{
		return TRUE;
	}
	return put_varint(buf, field << 3) && put_varint(buf, v);
}

static bool encode_message(vectorcore_eap_proxy_message_t *msg, chunk_t *out)
{
	buf_t buf = {};

	if (!put_uint(&buf, 1, msg->version) ||
		!put_uint(&buf, 2, msg->type) ||
		!put_string(&buf, 3, msg->session_id) ||
		!put_string(&buf, 4, msg->imsi) ||
		!put_string(&buf, 5, msg->nai) ||
		!put_string(&buf, 6, msg->apn) ||
		!put_uint(&buf, 7, msg->ike_spi_i) ||
		!put_uint(&buf, 8, msg->ike_spi_r) ||
		!put_bytes(&buf, 13, msg->eap_payload) ||
		!put_uint(&buf, 14, msg->status) ||
		!put_bytes(&buf, 15, msg->msk) ||
		!put_bytes(&buf, 16, msg->emsk) ||
		!put_uint(&buf, 17, msg->key_lifetime_seconds) ||
		!put_string(&buf, 18, msg->error_code) ||
		!put_string(&buf, 19, msg->error_message))
	{
		free(buf.ptr);
		return FALSE;
	}
	*out = chunk_create(buf.ptr, buf.len);
	return TRUE;
}

static bool get_varint(chunk_t *in, uint64_t *v)
{
	uint64_t val = 0;
	uint8_t shift = 0, b;

	while (in->len && shift < 64)
	{
		b = *in->ptr++;
		in->len--;
		val |= (uint64_t)(b & 0x7f) << shift;
		if (!(b & 0x80))
		{
			*v = val;
			return TRUE;
		}
		shift += 7;
	}
	return FALSE;
}

static bool take_len(chunk_t *in, chunk_t *out)
{
	uint64_t len;

	if (!get_varint(in, &len) || len > in->len)
	{
		return FALSE;
	}
	*out = chunk_create(in->ptr, len);
	in->ptr += len;
	in->len -= len;
	return TRUE;
}

static char *clone_string(chunk_t data)
{
	char *str = malloc(data.len + 1);

	if (!str)
	{
		return NULL;
	}
	memcpy(str, data.ptr, data.len);
	str[data.len] = '\0';
	return str;
}

static bool decode_message(chunk_t data, vectorcore_eap_proxy_message_t *msg)
{
	uint64_t key, value;
	chunk_t field;
	uint32_t nr, wire;

	while (data.len)
	{
		if (!get_varint(&data, &key))
		{
			return FALSE;
		}
		nr = key >> 3;
		wire = key & 0x7;
		switch (wire)
		{
			case 0:
				if (!get_varint(&data, &value))
				{
					return FALSE;
				}
				switch (nr)
				{
					case 1: msg->version = value; break;
					case 2: msg->type = value; break;
					case 7: msg->ike_spi_i = value; break;
					case 8: msg->ike_spi_r = value; break;
					case 14: msg->status = value; break;
					case 17: msg->key_lifetime_seconds = value; break;
					default: break;
				}
				break;
			case 2:
				if (!take_len(&data, &field))
				{
					return FALSE;
				}
				switch (nr)
				{
					case 3: msg->session_id = clone_string(field); break;
					case 4: msg->imsi = clone_string(field); break;
					case 5: msg->nai = clone_string(field); break;
					case 6: msg->apn = clone_string(field); break;
					case 13: msg->eap_payload = chunk_clone(field); break;
					case 15: msg->msk = chunk_clone(field); break;
					case 16: msg->emsk = chunk_clone(field); break;
					case 18: msg->error_code = clone_string(field); break;
					case 19: msg->error_message = clone_string(field); break;
					default: break;
				}
				break;
			default:
				return FALSE;
		}
	}
	return TRUE;
}

void vectorcore_eap_proxy_message_clear(vectorcore_eap_proxy_message_t *msg)
{
	free(msg->session_id);
	free(msg->imsi);
	free(msg->nai);
	free(msg->apn);
	chunk_clear(&msg->eap_payload);
	chunk_clear(&msg->msk);
	chunk_clear(&msg->emsk);
	free(msg->error_code);
	free(msg->error_message);
	memset(msg, 0, sizeof(*msg));
}

static bool wait_fd(int fd, short events, uint32_t timeout_ms)
{
	struct pollfd pfd = { .fd = fd, .events = events };

	return poll(&pfd, 1, timeout_ms) == 1 && (pfd.revents & events);
}

static bool write_all(int fd, void *ptr, size_t len, uint32_t timeout_ms)
{
	uint8_t *pos = ptr;
	ssize_t ret;

	while (len)
	{
		if (!wait_fd(fd, POLLOUT, timeout_ms))
		{
			return FALSE;
		}
		ret = write(fd, pos, len);
		if (ret <= 0)
		{
			return FALSE;
		}
		pos += ret;
		len -= ret;
	}
	return TRUE;
}

static bool read_all(int fd, void *ptr, size_t len, uint32_t timeout_ms)
{
	uint8_t *pos = ptr;
	ssize_t ret;

	while (len)
	{
		if (!wait_fd(fd, POLLIN, timeout_ms))
		{
			return FALSE;
		}
		ret = read(fd, pos, len);
		if (ret <= 0)
		{
			return FALSE;
		}
		pos += ret;
		len -= ret;
	}
	return TRUE;
}

bool vectorcore_eap_proxy_exchange(char *socket_path,
								   vectorcore_eap_proxy_message_t *request,
								   vectorcore_eap_proxy_message_t *response,
								   uint32_t timeout_ms)
{
	struct sockaddr_un addr = { .sun_family = AF_UNIX };
	uint32_t len_be, len;
	chunk_t encoded = chunk_empty, received = chunk_empty;
	int fd;
	bool ok = FALSE;

	fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (fd < 0)
	{
		return FALSE;
	}
	if (strlen(socket_path) >= sizeof(addr.sun_path))
	{
		close(fd);
		return FALSE;
	}
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
	if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
	{
		DBG1(DBG_IKE, "VectorCore EAP proxy IPC connect failed: %s", strerror(errno));
		close(fd);
		return FALSE;
	}
	if (!encode_message(request, &encoded) || encoded.len > UINT32_MAX)
	{
		goto out;
	}
	len_be = htonl(encoded.len);
	if (!write_all(fd, &len_be, sizeof(len_be), timeout_ms) ||
		!write_all(fd, encoded.ptr, encoded.len, timeout_ms) ||
		!read_all(fd, &len_be, sizeof(len_be), timeout_ms))
	{
		goto out;
	}
	len = ntohl(len_be);
	if (!len || len > 1024 * 1024)
	{
		goto out;
	}
	received = chunk_alloc(len);
	if (!read_all(fd, received.ptr, received.len, timeout_ms))
	{
		goto out;
	}
	ok = decode_message(received, response);
out:
	free(encoded.ptr);
	chunk_free(&received);
	close(fd);
	return ok;
}
