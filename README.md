# VectorCore strongSwan ePDG Component

This repository contains a modified strongSwan codebase used by VectorCore ePDG
for SWu/IKEv2/IPsec handling.

The VectorCore standards-mode runtime path uses the `vectorcore-eap-proxy`
strongSwan plugin.

In this mode, strongSwan does **not** act as the EAP-AKA/EAP-AKA' server.
Instead, strongSwan proxies raw EAP messages to `vectorcore-epdg` over a local
Unix domain socket. `vectorcore-epdg` then relays those EAP messages to
VectorCore AAA over SWm Diameter-EAP using `EAP-Payload`.

Target architecture:

```text
[UE]
  |
  | SWu: IKEv2 / EAP-AKA or EAP-AKA'
  v
[strongSwan + vectorcore-eap-proxy]
  |
  | local IPC: /run/vectorcore/epdg-eap.sock
  | raw EAP proxy messages
  v
[VectorCore ePDG]
  |
  | SWm Diameter-EAP / EAP-Payload
  v
[VectorCore AAA]
  |
  | SWx
  v
[VectorCore HSS]
```

VectorCore AAA remains the 3GPP EAP-AKA/EAP-AKA' server. The AAA must not be
changed to expose private strongSwan-specific AKA vectors over SWm.

## Legacy osmo_epdg plugin

The old `osmo_epdg` strongSwan plugin is retained only as legacy/reference code.

It is **not** the VectorCore standards-mode runtime path.

The old model was:

```text
strongSwan eap_aka_server
  -> requests AKA vectors over GSUP
  -> builds EAP-AKA challenge locally
  -> validates AT_MAC locally
  -> compares RES/XRES locally
```

That model is not used for VectorCore standards-mode SWm Diameter-EAP operation.

Do not enable `osmo_epdg` as a silent fallback.

## Build dependencies

Install the normal strongSwan build dependencies plus any dependencies required
by this source tree.

Example baseline package:

```bash
apt install libosmocore-dev
```

Additional build dependencies may be required depending on the host distribution
and strongSwan build options.

## Build strongSwan with VectorCore EAP proxy

From the strongSwan source directory:

```bash
./autogen.sh
```

Configure for VectorCore standards mode:

```bash
./configure \
  --enable-systemd \
  --enable-save-keys \
  --enable-p-cscf \
  --enable-vectorcore-eap-proxy \
  --with-systemdsystemunitdir
```

Then build:

```bash
make
```

Or to build only libcharon/plugin components during development:

```bash
make -C src/libcharon
make -C src/libcharon/plugins/vectorcore_eap_proxy
```

## Important build note

For VectorCore standards mode, do **not** configure the old local EAP-AKA/vector
path:

```text
--enable-osmo-epdg
--enable-eap-aka
--enable-eap-aka-3gpp
--enable-eap-aka-3gpp2
```

Those belong to the older strongSwan-local EAP-AKA validation model and are not
the target VectorCore SWm Diameter-EAP proxy path.

## Plugin configuration

The VectorCore EAP proxy plugin uses a local Unix domain socket:

```text
/run/vectorcore/epdg-eap.sock
```

strongSwan option:

```text
charon.plugins.vectorcore-eap-proxy.socket = /run/vectorcore/epdg-eap.sock
```

This socket is served by `vectorcore-epdg`.

## VectorCore ePDG configuration

`vectorcore-epdg` must enable standards-mode authentication and expose the EAP
proxy socket:

```erlang
{auth_mode, swm_diameter_eap},
{eap_proxy_socket, "/run/vectorcore/epdg-eap.sock"},
```

The ePDG side is responsible for:

```text
- accepting raw EAP messages from strongSwan over local IPC
- mapping AUTH_START / AUTH_CONTINUE to UE FSM events
- sending SWm DER with EAP-Payload to VectorCore AAA
- receiving SWm DEA from AAA
- returning EAP continue/success/failure to strongSwan
```

## AAA behavior

VectorCore AAA must remain standards-oriented.

The AAA is the EAP-AKA/EAP-AKA' server.

SWm must use Diameter-EAP DER/DEA with `EAP-Payload`.

AAA must not expose private `RAND/AUTN/XRES/CK/IK` tuples as the normal SWm
contract.

## MSK / keying material requirement

The `vectorcore-eap-proxy` plugin requires the EAP keying material needed by
strongSwan when authentication succeeds.

The current implementation expects a 64-byte MSK from the ePDG/AAA path on EAP
success.

If VectorCore AAA does not include `EAP-Master-Session-Key` or equivalent
standards-correct keying material in the SWm success response, IKE_AUTH will
fail with a missing-MSK condition by design.

Do not bypass this check.

## Current validation status

Recovered implementation notes indicate:

```text
- vectorcore-eap-proxy plugin was added
- vectorcore-epdg gained an EAP proxy IPC server
- vectorcore-epdg supervisor starts the IPC server
- ePDG FSM gained proxy start/continue handling
- SWm DEA handling can carry EAP-Master-Session-Key if present
- key material is redacted from logs
- strongSwan configured with --enable-vectorcore-eap-proxy
- osmo_epdg is not selected in the active standards-mode configure
- VectorCore AAA src/include/config/rebar.config matched the clean baseline
```

Build checks reported:

```text
rebar3 compile
rebar3 eunit
make -C src/libcharon
```

`rebar3 eunit` completed, but with zero tests.

## Funding / attribution

This codebase may contain code derived from strongSwan and Osmocom osmo-epdg
work. Preserve all original copyright and license notices.

Where VectorCore code is derived from Osmocom/strongSwan code, keep attribution
and clearly mark VectorCore modifications.

