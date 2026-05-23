AM_CPPFLAGS = \
	-I$(top_srcdir)/src/libstrongswan \
	-I$(top_srcdir)/src/libcharon

AM_CFLAGS = \
	$(PLUGIN_CFLAGS)

if MONOLITHIC
noinst_LTLIBRARIES = libstrongswan-vectorcore-eap-proxy.la
else
plugin_LTLIBRARIES = libstrongswan-vectorcore-eap-proxy.la
endif

libstrongswan_vectorcore_eap_proxy_la_SOURCES = \
	vectorcore_eap_proxy_plugin.h vectorcore_eap_proxy_plugin.c \
	vectorcore_eap_proxy.h vectorcore_eap_proxy.c \
	vectorcore_eap_proxy_ipc.h vectorcore_eap_proxy_ipc.c

libstrongswan_vectorcore_eap_proxy_la_LDFLAGS = -module -avoid-version
