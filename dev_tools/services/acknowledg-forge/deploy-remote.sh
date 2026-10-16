#!/usr/bin/bash

# Part of the EDG Compiler Project, under the Apache License v2.0 with LLVM
# Exceptions.
# See https://edgcpp.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
# Point an acknowledg-forge Quadlet service at a new image and restart it.
#
# Installed by the host's administrator as a root-owned helper (e.g.
# /usr/local/libexec/acknowledg-forge-deploy) that the deploy user may run
# with sudo, and nothing else as root:
#
#   sudo /usr/local/libexec/acknowledg-forge-deploy \
#     <service> <image@sha256:...> <acknowledg-api-url> <acknowledg-ui-url>
#
# The URLs are the public AcknowlEDG endpoints, e.g. wss://api.example and
# https://ui.example (the same values the web client is built with).
#
# The service is a rootless Podman user service of SERVICE_USER, whose unit
# the administrator owns,
# /etc/containers/systemd/users/<uid>/<service>.container (see README.md),
# along with its credentials. This script only manages a drop-in next to it
# that sets the image (pinned by digest, from IMAGE_REPOSITORY only) and the
# EDG_ACKNOWLEDG_{API,UI}_{HOST,PORT} the URLs translate to, and restarts the
# service only when that drop-in changes. Since it runs as root on a caller's
# behalf, every argument is strictly validated.

set -euo pipefail

# The only repository images may come from.
IMAGE_REPOSITORY='ghcr.io/edgcpp/acknowledg-forge'
# The account the service runs as.
SERVICE_USER='acknowledg-forge'

if [[ $# -ne 4 ]]; then
  echo "usage: $0 <service> <image@sha256:...> <acknowledg-api-url>" \
       '<acknowledg-ui-url>' >&2
  exit 2
fi

service=$1
image=$2
api_url=$3
ui_url=$4

fail() {
  echo "error: $*" >&2
  exit 1
}

[[ ${EUID} -eq 0 ]] || fail 'must run as root (via sudo)'
[[ "${service}" =~ ^acknowledg-forge-[a-z0-9-]+$ ]] \
  || fail 'the service must be named acknowledg-forge-<name>'
[[ "${image}" =~ ^${IMAGE_REPOSITORY//./\\.}@sha256:[0-9a-f]{64}$ ]] \
  || fail "the image must be ${IMAGE_REPOSITORY}, pinned by digest"

# Split ${3} (a URL) into url_host and url_port. ${1} and ${2} are the plain
# and TLS schemes it may use (e.g. ws and wss). The service picks TLS by port
# alone (443), so only URLs it can honor exactly are accepted: TLS on 443 and
# plain on any other port, never a silent downgrade.
parse_url() {
  local plain=$1 tls=$2 url=$3 label=$4
  local pattern="^(${plain}|${tls})://"
  pattern+='([A-Za-z0-9]([A-Za-z0-9.-]*[A-Za-z0-9])?)(:([0-9]{1,5}))?/?$'
  [[ "${url}" =~ ${pattern} ]] \
    || fail "the AcknowlEDG ${label} must be ${plain}:// or ${tls}://" \
            'followed by a host name and optional port, with no path'
  local scheme=${BASH_REMATCH[1]}
  url_host=${BASH_REMATCH[2]}
  url_port=${BASH_REMATCH[5]}
  if [[ -n "${url_port}" ]] &&
     (( 10#${url_port} < 1 || 10#${url_port} > 65535 )); then
    fail "the AcknowlEDG ${label} has an invalid port"
  fi
  url_port=${url_port:+$((10#${url_port}))}
  if [[ "${scheme}" == "${tls}" ]]; then
    url_port=${url_port:-443}
    [[ ${url_port} -eq 443 ]] \
      || fail "the AcknowlEDG ${label} can only use ${tls}:// on port 443"
  else
    url_port=${url_port:-80}
    [[ ${url_port} -ne 443 ]] \
      || fail "the AcknowlEDG ${label} must use ${tls}:// on port 443"
  fi
}
parse_url ws wss "${api_url}" 'API URL'
api_host=${url_host}
api_port=${url_port}
parse_url http https "${ui_url}" 'UI URL'
ui_host=${url_host}
ui_port=${url_port}

uid="$(id -u "${SERVICE_USER}")" || fail "no ${SERVICE_USER} account"
if [[ "$(loginctl show-user "${SERVICE_USER}" --property=Linger --value)" \
      != yes ]]; then
  fail "lingering is off for ${SERVICE_USER}" \
       "(loginctl enable-linger ${SERVICE_USER})"
fi
unit_dir="/etc/containers/systemd/users/${uid}"
[[ -f "${unit_dir}/${service}.container" ]] \
  || fail "${unit_dir}/${service}.container is missing; set the service" \
          'up first (see README.md)'

# Run a command as the service's account, in its user session.
as_service_user() {
  runuser -u "${SERVICE_USER}" -- \
    env XDG_RUNTIME_DIR="/run/user/${uid}" \
    sh -c 'cd && exec "$@"' sh "$@"
}

user_systemctl() {
  systemctl --user --machine="${SERVICE_USER}@" "$@"
}

# The drop-in stays root-owned: the account can't change what it runs.
drop_in_dir="${unit_dir}/${service}.container.d"
drop_in="${drop_in_dir}/50-ci-deploy.conf"
content="\
# Managed by the acknowledg-forge-deploy workflow; changes are overwritten.
[Container]
Image=${image}
Environment=EDG_ACKNOWLEDG_API_HOST=${api_host}
Environment=EDG_ACKNOWLEDG_API_PORT=${api_port}
Environment=EDG_ACKNOWLEDG_UI_HOST=${ui_host}
Environment=EDG_ACKNOWLEDG_UI_PORT=${ui_port}
"
changed=0
if [[ ! -f "${drop_in}" ]] || ! cmp -s "${drop_in}" <(printf '%s' "${content}")
then
  install -d -m 0755 "${drop_in_dir}"
  printf '%s' "${content}" > "${drop_in}.tmp"
  chmod 0644 "${drop_in}.tmp"
  mv "${drop_in}.tmp" "${drop_in}"
  changed=1
fi

echo "Pulling ${image##*@}."
as_service_user podman pull --quiet "${image}" > /dev/null

user_systemctl daemon-reload
if [[ ${changed} -eq 1 ]]; then
  echo "Restarting ${service}."
  user_systemctl restart "${service}.service"
elif ! user_systemctl is-active --quiet "${service}.service"; then
  echo "Starting ${service}."
  user_systemctl start "${service}.service"
else
  echo "Unchanged; ${service} keeps running."
fi

# Give it a moment to fail on startup (bad configuration, missing access).
sleep 5
if ! user_systemctl is-active --quiet "${service}.service"; then
  echo "error: ${service} is not active" >&2
  journalctl "_UID=${uid}" --user-unit "${service}.service" --lines 30 \
    --no-pager >&2 || true
  exit 1
fi

as_service_user podman image prune --force > /dev/null
echo "Deployed ${service} (${image##*@})."
