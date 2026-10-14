#!/usr/bin/env bash

# Prepare a native build environment for audiocapturemgr and build its
# source-level RDK dependencies into ./dependencies.
#
# Dependency repositories:
#   https://github.com/rdkcentral/media_utils
#   https://github.com/rdkcentral/rdk-halif-rmf_audio_capture
#   https://github.com/rdkcentral/iarmbus
#
# The resulting native dependency layout is:
#   dependencies/native/include/media-utils/audioCapture/rmfAudioCapture.h
#   dependencies/native/include/rdk/iarmbus/*.h
#   dependencies/native/include/safec_lib.h
#   dependencies/native/lib/librmfAudioCapture.a
#   dependencies/native/lib/libIARMBus.a

set -Eeuo pipefail

readonly SCRIPT_NAME="$(basename "$0")"
readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
readonly DEPS_DIR="${ROOT_DIR}/dependencies"
readonly PREFIX="${DEPS_DIR}/native"

readonly MEDIA_UTILS_REPOSITORY="https://github.com/rdkcentral/media_utils.git"
readonly RMF_AUDIO_CAPTURE_HAL_REPOSITORY="https://github.com/rdkcentral/rdk-halif-rmf_audio_capture.git"
readonly IARMBUS_REPOSITORY="https://github.com/rdkcentral/iarmbus.git"

readonly MEDIA_UTILS_REF="${MEDIA_UTILS_REF:-develop}"
readonly RMF_AUDIO_CAPTURE_HAL_REF="${RMF_AUDIO_CAPTURE_HAL_REF:-main}"
readonly IARMBUS_REF="${IARMBUS_REF:-develop}"

readonly APT_PACKAGES=(
    autoconf
    automake
    build-essential
    git
    libdbus-1-dev
    libglib2.0-dev
    libtool
    make
    pkg-config
)

log() {
    printf '[%s] %s\n' "$SCRIPT_NAME" "$*"
}

fail() {
    printf '[%s] ERROR: %s\n' "$SCRIPT_NAME" "$*" >&2
    exit 1
}

usage() {
    cat <<EOF
Usage: $SCRIPT_NAME [--help]

Install native build tools, clone the RDK dependency sources, and build their
static libraries under:

  $PREFIX

Optional environment variables:

  MEDIA_UTILS_REF
      Git branch, tag, or commit for media_utils.
      Default: $MEDIA_UTILS_REF

  RMF_AUDIO_CAPTURE_HAL_REF
      Git branch, tag, or commit for rdk-halif-rmf_audio_capture.
      Default: $RMF_AUDIO_CAPTURE_HAL_REF

  IARMBUS_REF
      Git branch, tag, or commit for iarmbus.
      Default: $IARMBUS_REF

After this script completes, build audiocapturemgr with:

  export RDK_FSROOT_PATH="$PREFIX"
  export PKG_CONFIG_SYSROOT_DIR="$PREFIX"
  autoreconf --install --force
  ./configure
  make
EOF
}

case "${1:-}" in
    "")
        ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        usage >&2
        exit 2
        ;;
esac

install_host_dependencies() {
    command -v apt-get >/dev/null 2>&1 || {
        fail "Only Debian/Ubuntu hosts using apt-get are currently supported."
    }

    local apt_prefix=()

    if [[ "$(id -u)" -ne 0 ]]; then
        command -v sudo >/dev/null 2>&1 || {
            fail "sudo is required to install host packages as a non-root user."
        }

        apt_prefix=(sudo)
    fi

    log "Installing native build prerequisites."

    "${apt_prefix[@]}" apt-get update

    "${apt_prefix[@]}" apt-get install \
        --no-install-recommends \
        --yes \
        "${APT_PACKAGES[@]}"
}

verify_host_dependencies() {
    local package

    for package in glib-2.0 gthread-2.0 dbus-1; do
        if ! PKG_CONFIG_SYSROOT_DIR= pkg-config --exists "$package"; then
            fail "The native pkg-config package '${package}' was not found."
        fi
    done

    log "Verified native GLib, GThread, and D-Bus development packages."
}

checkout_source() {
    local repository="$1"
    local reference="$2"
    local destination="$3"

    if [[ -d "${destination}/.git" ]]; then
        log "Updating $(basename "$destination") at ${reference}."

        git -C "$destination" fetch \
            --depth 1 \
            origin \
            "$reference"

        git -C "$destination" checkout --detach FETCH_HEAD
    else
        log "Cloning $(basename "$destination") at ${reference}."

        git clone \
            --depth 1 \
            --branch "$reference" \
            "$repository" \
            "$destination"
    fi
}

bootstrap_autotools() {
    local source_dir="$1"

    (
        cd "$source_dir"

        if [[ ! -x ./configure ]]; then
            log "Generating Autotools files in $(basename "$source_dir")."
            autoreconf --install --force
        fi
    )
}

parallel_jobs() {
    if command -v nproc >/dev/null 2>&1; then
        nproc
    else
        printf '1\n'
    fi
}

install_rmf_audio_capture_header() {
    local source_dir="${DEPS_DIR}/rdk-halif-rmf_audio_capture"
    local header="${source_dir}/include/rmfAudioCapture.h"

    checkout_source \
        "$RMF_AUDIO_CAPTURE_HAL_REPOSITORY" \
        "$RMF_AUDIO_CAPTURE_HAL_REF" \
        "$source_dir"

    [[ -f "$header" ]] || {
        fail "RMF Audio Capture header was not found at ${header}."
    }

    install -D -m 0644 \
        "$header" \
        "${PREFIX}/include/media-utils/audioCapture/rmfAudioCapture.h"

    log "Installed rmfAudioCapture.h."
}

build_media_utils() {
    local source_dir="${DEPS_DIR}/media_utils"
    local library="${source_dir}/src/audioCapture/.libs/librmfAudioCapture.a"
    local hal_include_dir="${DEPS_DIR}/rdk-halif-rmf_audio_capture/include"

    checkout_source \
        "$MEDIA_UTILS_REPOSITORY" \
        "$MEDIA_UTILS_REF" \
        "$source_dir"

    [[ -f "${hal_include_dir}/rmfAudioCapture.h" ]] || {
        fail "The RMF Audio Capture HAL header must be installed before building media_utils."
    }

    bootstrap_autotools "$source_dir"

    (
        cd "$source_dir"

        export CPPFLAGS="${CPPFLAGS:-} -I${hal_include_dir}"
        export CFLAGS="${CFLAGS:-} -I${hal_include_dir}"

        ./configure \
            --prefix="$PREFIX" \
            --enable-static \
            --disable-shared

        make -j"$(parallel_jobs)"
    )

    [[ -f "$library" ]] || {
        fail "media_utils did not produce librmfAudioCapture.a."
    }

    install -D -m 0644 \
        "$library" \
        "${PREFIX}/lib/librmfAudioCapture.a"

    log "Installed librmfAudioCapture.a."
}

build_iarmbus() {
    local source_dir="${DEPS_DIR}/iarmbus"
    local library="${source_dir}/core/.libs/libIARMBus.a"
    local include_dir="${source_dir}/core/include"
    local safec_stub="${source_dir}/stubs/safec_lib.h"
    local host_pkg_cflags
    local host_pkg_libs

    checkout_source \
        "$IARMBUS_REPOSITORY" \
        "$IARMBUS_REF" \
        "$source_dir"

    [[ -f "$safec_stub" ]] || {
        fail "iarmbus did not provide stubs/safec_lib.h."
    }

    # GLib and D-Bus are native host dependencies. Clear
    # PKG_CONFIG_SYSROOT_DIR so pkg-config returns host paths instead of
    # incorrectly prefixing them with the RDK staging directory.
    host_pkg_cflags="$(
        PKG_CONFIG_SYSROOT_DIR= \
            pkg-config --cflags glib-2.0 gthread-2.0 dbus-1
    )"

    host_pkg_libs="$(
        PKG_CONFIG_SYSROOT_DIR= \
            pkg-config --libs glib-2.0 gthread-2.0 dbus-1
    )"

    bootstrap_autotools "$source_dir"

    (
        cd "$source_dir"

        # iarmbus includes safec_lib.h from its stubs directory. It also
        # includes GLib and D-Bus headers directly.
        export CPPFLAGS="${CPPFLAGS:-} -I${source_dir}/stubs ${host_pkg_cflags}"
        export CFLAGS="${CFLAGS:-} -I${source_dir}/stubs ${host_pkg_cflags}"
        export CXXFLAGS="${CXXFLAGS:-} -I${source_dir}/stubs ${host_pkg_cflags}"
        export LIBS="${LIBS:-} ${host_pkg_libs}"

        # Do not apply an RDK sysroot while configuring this native library.
        PKG_CONFIG_SYSROOT_DIR= ./configure \
            --prefix="$PREFIX" \
            --enable-static \
            --disable-shared

        make clean
        make -j"$(parallel_jobs)"
    )

    [[ -f "${include_dir}/libIARM.h" ]] || {
        fail "iarmbus did not provide core/include/libIARM.h."
    }

    [[ -f "${include_dir}/libIBus.h" ]] || {
        fail "iarmbus did not provide core/include/libIBus.h."
    }

    [[ -f "$library" ]] || {
        fail "iarmbus did not produce libIARMBus.a."
    }

    # Install all public IARM headers. audiocapturemgr directly needs
    # libIARM.h and libIBus.h, while those headers reference related public
    # IARM declarations.
    mkdir -p "${PREFIX}/include/rdk/iarmbus"

    find "$include_dir" \
        -maxdepth 1 \
        -type f \
        -name '*.h' \
        -exec install -m 0644 {} "${PREFIX}/include/rdk/iarmbus/" \;

    # audiocapturemgr includes safec_lib.h directly. Use the iarmbus stub
    # when an external Safe C implementation is not available.
    install -D -m 0644 \
        "$safec_stub" \
        "${PREFIX}/include/safec_lib.h"

    install -D -m 0644 \
        "$library" \
        "${PREFIX}/lib/libIARMBus.a"

    log "Installed IARM Bus headers, safec_lib.h, and libIARMBus.a."
}

main() {
    install_host_dependencies
    verify_host_dependencies

    mkdir -p \
        "$DEPS_DIR" \
        "${PREFIX}/include" \
        "${PREFIX}/lib"

    # The HAL header must be available before media_utils is compiled.
    install_rmf_audio_capture_header
    build_media_utils
    build_iarmbus

    log "Native dependencies are available under:"
    log "  ${PREFIX}"
    log ""
    log "Configure audiocapturemgr with:"
    log "  export RDK_FSROOT_PATH=${PREFIX}"
    log "  export PKG_CONFIG_SYSROOT_DIR=${PREFIX}"
    log "  autoreconf --install --force"
    log "  ./configure"
    log "  make"
}

main "$@"
