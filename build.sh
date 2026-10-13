#!/usr/bin/env bash

# Build audiocapturemgr in a native Linux environment.
#
# Run build_dependencies.sh first. It must stage dependencies under:
#
#   dependencies/native/include/media-utils/audioCapture/rmfAudioCapture.h
#   dependencies/native/include/rdk/iarmbus/libIARM.h
#   dependencies/native/include/rdk/iarmbus/libIBus.h
#   dependencies/native/include/safec_lib.h
#   dependencies/native/lib/librmfAudioCapture.a
#   dependencies/native/lib/libIARMBus.a
#
# Usage:
#   ./build_dependencies.sh
#   ./build.sh
#
# Optional:
#   ./build.sh --clean
#   ./build.sh --rebuild
#   ./build.sh --enable-testapp
#   ./build.sh --jobs 4

set -Eeuo pipefail

readonly SCRIPT_NAME="$(basename "$0")"
readonly ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

DEPS_PREFIX="${DEPS_PREFIX:-${ROOT_DIR}/dependencies/native}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-native}"
INSTALL_PREFIX="${INSTALL_PREFIX:-${BUILD_DIR}/install}"

CLEAN_BUILD=false
CLEAN_ONLY=false
ENABLE_TESTAPP=false
JOBS=""

log() {
    printf '[%s] %s\n' "$SCRIPT_NAME" "$*"
}

fail() {
    printf '[%s] ERROR: %s\n' "$SCRIPT_NAME" "$*" >&2
    exit 1
}

usage() {
    cat <<EOF
Usage: $SCRIPT_NAME [options]

Build audiocapturemgr in a native Linux environment using dependencies staged
by build_dependencies.sh.

Options:
  --clean             Remove the native build directory and exit.
  --rebuild           Remove the native build directory, then build.
  --enable-testapp    Build the repository test applications.
  --jobs NUMBER       Number of parallel make jobs.
  -h, --help          Display this help message.

Environment variables:
  DEPS_PREFIX         Dependency installation prefix.
                      Default: $DEPS_PREFIX

  BUILD_DIR           Out-of-tree build directory.
                      Default: $BUILD_DIR

  INSTALL_PREFIX      Component installation directory.
                      Default: $INSTALL_PREFIX

  CC                  Native C compiler.
                      Default: gcc

  CXX                 Native C++ compiler.
                      Default: g++

Examples:
  ./build_dependencies.sh
  ./build.sh
  ./build.sh --rebuild --jobs 4
  ./build.sh --enable-testapp
EOF
}

parse_arguments() {
    while (($# > 0)); do
        case "$1" in
            --clean)
                CLEAN_BUILD=true
                CLEAN_ONLY=true
                shift
                ;;
            --rebuild)
                CLEAN_BUILD=true
                CLEAN_ONLY=false
                shift
                ;;
            --enable-testapp)
                ENABLE_TESTAPP=true
                shift
                ;;
            --jobs)
                (($# >= 2)) || fail "--jobs requires a numeric argument."

                case "$2" in
                    ''|*[!0-9]*)
                        fail "--jobs requires a positive integer."
                        ;;
                    0)
                        fail "--jobs must be greater than zero."
                        ;;
                    *)
                        JOBS="$2"
                        ;;
                esac

                shift 2
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                usage >&2
                fail "Unknown option: $1"
                ;;
        esac
    done
}

require_command() {
    local command_name="$1"

    command -v "$command_name" >/dev/null 2>&1 || {
        fail "Required command '${command_name}' was not found."
    }
}

require_file() {
    local file="$1"
    local description="$2"

    [[ -f "$file" ]] || {
        fail "${description} was not found at ${file}."
    }
}

parallel_jobs() {
    if [[ -n "$JOBS" ]]; then
        printf '%s\n' "$JOBS"
    elif command -v nproc >/dev/null 2>&1; then
        nproc
    else
        printf '1\n'
    fi
}

verify_build_tools() {
    local command_name

    for command_name in \
        autoreconf \
        gcc \
        g++ \
        libtoolize \
        make \
        pkg-config; do
        require_command "$command_name"
    done
}

native_pkg_cflags() {
    PKG_CONFIG_SYSROOT_DIR= \
        pkg-config --cflags glib-2.0 gthread-2.0 dbus-1
}

native_pkg_libs() {
    PKG_CONFIG_SYSROOT_DIR= \
        pkg-config --libs glib-2.0 gthread-2.0 dbus-1
}

verify_dependencies() {
    require_file \
        "${DEPS_PREFIX}/include/media-utils/audioCapture/rmfAudioCapture.h" \
        "RMF Audio Capture header"

    require_file \
        "${DEPS_PREFIX}/include/rdk/iarmbus/libIARM.h" \
        "IARM core header"

    require_file \
        "${DEPS_PREFIX}/include/rdk/iarmbus/libIBus.h" \
        "IARM Bus header"

    require_file \
        "${DEPS_PREFIX}/include/safec_lib.h" \
        "Safe C compatibility header"

    require_file \
        "${DEPS_PREFIX}/lib/librmfAudioCapture.a" \
        "RMF Audio Capture static library"

    require_file \
        "${DEPS_PREFIX}/lib/libIARMBus.a" \
        "IARM Bus static library"

    local package
    local pkg_libs

    for package in glib-2.0 gthread-2.0 dbus-1; do
        if ! PKG_CONFIG_SYSROOT_DIR= pkg-config --exists "$package"; then
            fail "Native pkg-config package '${package}' was not found."
        fi
    done

    pkg_libs="$(native_pkg_libs)"

    [[ "$pkg_libs" == *"-lglib-2.0"* ]] || {
        fail "pkg-config did not return -lglib-2.0."
    }

    [[ "$pkg_libs" == *"-ldbus-1"* ]] || {
        fail "pkg-config did not return -ldbus-1."
    }

    log "Verified staged RDK and native system dependencies."
    log "IARM transitive linker flags: ${pkg_libs}"
}

clean_build() {
    if [[ -d "$BUILD_DIR" ]]; then
        log "Removing native build directory: ${BUILD_DIR}"
        rm -rf "$BUILD_DIR"
    else
        log "Native build directory does not exist: ${BUILD_DIR}"
    fi
}

bootstrap_source_tree() {
    [[ -f "${ROOT_DIR}/configure.ac" ]] || {
        fail "configure.ac was not found in ${ROOT_DIR}."
    }

    log "Generating Autotools files."

    (
        cd "$ROOT_DIR"
        autoreconf --install --force
    )
}

configure_build() {
    local configure_options=(
        "--prefix=${INSTALL_PREFIX}"
    )
    local pkg_cflags
    local pkg_libs

    if [[ "$ENABLE_TESTAPP" == true ]]; then
        configure_options+=(--enable-testapp)
    fi

    pkg_cflags="$(native_pkg_cflags)"
    pkg_libs="$(native_pkg_libs)"

    mkdir -p "$BUILD_DIR"

    log "Configuring the native build in ${BUILD_DIR}."

    (
        cd "$BUILD_DIR"

        export CC="${CC:-gcc}"
        export CXX="${CXX:-g++}"

        # This is a native dependency prefix, not a cross-compilation sysroot.
        export RDK_FSROOT_PATH="$DEPS_PREFIX"
        export PKG_CONFIG_SYSROOT_DIR=

        export CPPFLAGS="${CPPFLAGS:-} \
-I${DEPS_PREFIX}/include \
-I${DEPS_PREFIX}/include/media-utils/audioCapture \
-I${DEPS_PREFIX}/include/rdk/iarmbus \
${pkg_cflags}"

        export CFLAGS="${CFLAGS:-} -fPIC"
        export CXXFLAGS="${CXXFLAGS:-} -fPIC"

        # The project Makefile uses RDK_FSROOT_PATH for /usr/lib locations,
        # while dependencies are staged directly under ${DEPS_PREFIX}/lib.
        export LDFLAGS="${LDFLAGS:-} \
-L${DEPS_PREFIX}/lib \
-L${DEPS_PREFIX}/usr/lib \
-L${DEPS_PREFIX}/usr/local/lib"

        # configure substitutes LIBS into the generated Makefiles. These
        # libraries must appear after libIARMBus.a for static linking.
        export LIBS="${LIBS:-} \
-Wl,--no-as-needed \
${pkg_libs} \
-lpthread \
-Wl,--as-needed"

        "${ROOT_DIR}/configure" "${configure_options[@]}"
    )
}

compile_component() {
    local job_count

    job_count="$(parallel_jobs)"

    log "Building audiocapturemgr with ${job_count} parallel job(s)."

    (
        cd "$BUILD_DIR"

        # LIBS was captured by configure and is now present in the generated
        # Makefiles. Do not replace it here, or the link-order fix may be lost.
        make -j"$job_count"
    )
}

install_component() {
    log "Installing audiocapturemgr under ${INSTALL_PREFIX}."

    (
        cd "$BUILD_DIR"
        make install
    )
}

print_summary() {
    log "Native build completed successfully."
    log "Install prefix: ${INSTALL_PREFIX}"
    log "Executable location: ${INSTALL_PREFIX}/bin/audiocapturemgr"
    log "Library directory: ${INSTALL_PREFIX}/lib"
    log ""
    log "Runtime requirements:"
    log "  - A running IARM Bus service."
    log "  - A functional RMF Audio Capture provider."
    log ""
    log "Note: the media_utils implementation may be a stub and is primarily"
    log "suitable for compilation and integration testing without hardware."
}

main() {
    parse_arguments "$@"
    verify_build_tools

    if [[ "$CLEAN_BUILD" == true ]]; then
        clean_build
    fi

    if [[ "$CLEAN_ONLY" == true ]]; then
        log "Clean completed."
        exit 0
    fi

    verify_dependencies
    bootstrap_source_tree
    configure_build
    compile_component
    install_component
    print_summary
}

main "$@"
