#!/usr/bin/env bash
set -Eeuo pipefail

umask 077

readonly SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
readonly VENV_DIR="${DEPLOY_VENV_DIR:-${SCRIPT_DIR}/.venv}"
readonly DEPS_DIR="${DEPLOY_DEPS_DIR:-${SCRIPT_DIR}/.deps}"
readonly BUILD_ROOT="${DEPLOY_BUILD_DIR:-${SCRIPT_DIR}/build/deploy}"
readonly OUT_ROOT="${DEPLOY_OUT_DIR:-${SCRIPT_DIR}/out}"
readonly REQUIREMENTS="${SCRIPT_DIR}/tools/requirements-deploy.txt"
readonly DFU_ID="0483:df11"
readonly FLASH_BASE="0x08000000"

COMMAND="build"
FLASH_TARGET=""
RELEASE_ID=""
RELEASE_DIR=""
KEY_FILE=""
INITIAL_COUNTER="1"
H7_SERIAL=""
G431_SERIAL=""
PROTECTION="OFF"
ARM_PREFIX=""

log() {
    printf '[deploy] %s\n' "$*"
}

die() {
    printf '[deploy] ERROR: %s\n' "$*" >&2
    exit 1
}

usage() {
    cat <<'EOF'
Usage:
  ./deploy.sh build [options]
  ./deploy.sh flash h7|g431 [options]
  ./deploy.sh all --h7-serial SERIAL --g431-serial SERIAL [options]
  ./deploy.sh deps

Commands:
  build       Create the venv, fetch dependencies, test, build, and provision.
  flash       Flash one provisioned image from out/latest or --release.
  all         Build and provision, then flash both serial-selected DFU devices.
  deps        Create the venv and fetch/verify pinned dependencies only.

Options:
  --release-id NAME    Name for a new directory below out/.
  --release PATH       Existing release directory used by flash.
  --keys PATH          Existing keys.json passed unchanged to provision.py.
  --counter VALUE      Initial counter passed unchanged to provision.py.
  --development        Build without the RDP/WRP boot guard (default).
  --production         Require configured RDP1 and WRP option bytes at boot.
  --h7-serial SERIAL   ROM-DFU serial assigned to the H743.
  --g431-serial SERIAL ROM-DFU serial assigned to the G431.
  -h, --help           Show this help.

Environment overrides:
  ARM_TOOLCHAIN_PREFIX, DEPLOY_VENV_DIR, DEPLOY_DEPS_DIR,
  DEPLOY_BUILD_DIR, DEPLOY_OUT_DIR
EOF
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || die "Required command '$1' was not found."
}

parse_args() {
    if (($#)); then
        case "$1" in
            build|all|deps)
                COMMAND="$1"
                shift
                ;;
            flash)
                COMMAND="flash"
                shift
                (($#)) || die "flash requires target 'h7' or 'g431'."
                FLASH_TARGET="$1"
                shift
                [[ "${FLASH_TARGET}" == "h7" || "${FLASH_TARGET}" == "g431" ]] ||
                    die "Unknown flash target '${FLASH_TARGET}'; use h7 or g431."
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                die "Unknown command '$1'. Run ./deploy.sh --help."
                ;;
        esac
    fi

    while (($#)); do
        case "$1" in
            --release-id)
                (($# >= 2)) || die "--release-id requires a value."
                RELEASE_ID="$2"
                shift 2
                ;;
            --release)
                (($# >= 2)) || die "--release requires a path."
                RELEASE_DIR="$2"
                shift 2
                ;;
            --keys)
                (($# >= 2)) || die "--keys requires a path."
                KEY_FILE="$2"
                shift 2
                ;;
            --counter)
                (($# >= 2)) || die "--counter requires a value."
                INITIAL_COUNTER="$2"
                shift 2
                ;;
            --development)
                PROTECTION="OFF"
                shift
                ;;
            --production)
                PROTECTION="ON"
                shift
                ;;
            --h7-serial)
                (($# >= 2)) || die "--h7-serial requires a value."
                H7_SERIAL="$2"
                shift 2
                ;;
            --g431-serial)
                (($# >= 2)) || die "--g431-serial requires a value."
                G431_SERIAL="$2"
                shift 2
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                die "Unknown option '$1'. Run ./deploy.sh --help."
                ;;
        esac
    done

    [[ "${COMMAND}" == "flash" || -z "${RELEASE_DIR}" ]] ||
        die "--release is valid only with the flash command."
    [[ "${COMMAND}" == "build" || "${COMMAND}" == "all" || -z "${KEY_FILE}" ]] ||
        die "--keys is valid only with build or all."
    [[ "${COMMAND}" == "build" || "${COMMAND}" == "all" || "${INITIAL_COUNTER}" == "1" ]] ||
        die "--counter is valid only with build or all."
}

setup_python() {
    require_command python3
    [[ -f "${REQUIREMENTS}" ]] || die "Missing ${REQUIREMENTS}."

    if [[ ! -x "${VENV_DIR}/bin/python" ]]; then
        log "Creating Python virtual environment at ${VENV_DIR}"
        python3 -m venv "${VENV_DIR}" ||
            die "Could not create the venv. Install the Python venv package for your OS."
    fi

    log "Installing pinned Python requirements"
    "${VENV_DIR}/bin/python" -m pip install \
        --disable-pip-version-check --no-cache-dir --no-deps --require-hashes \
        -r "${REQUIREMENTS}"
}

verify_dependencies() {
    log "Verifying dependency revisions and clean working trees"
    "${VENV_DIR}/bin/python" - "${SCRIPT_DIR}" "${DEPS_DIR}" <<'PY'
import json
from pathlib import Path
import subprocess
import sys

root = Path(sys.argv[1])
deps = Path(sys.argv[2])
declared = json.loads((root / "tools/dependencies.json").read_text())
for name, (_, expected) in declared.items():
    checkout = deps / name
    try:
        actual = subprocess.check_output(
            ["git", "-C", str(checkout), "rev-parse", "HEAD"], text=True
        ).strip()
        dirty = subprocess.check_output(
            ["git", "-C", str(checkout), "status", "--porcelain"], text=True
        )
    except subprocess.CalledProcessError as error:
        raise SystemExit(f"Cannot inspect dependency {checkout}: {error}")
    if actual != expected:
        raise SystemExit(f"Dependency {name} is at {actual}, expected {expected}")
    if dirty:
        raise SystemExit(f"Dependency {name} contains local modifications")
PY
}

fetch_dependencies() {
    require_command git
    setup_python
    local expected_tinycrypt actual_tinycrypt dirty_tinycrypt
    expected_tinycrypt="$(git -C "${SCRIPT_DIR}" ls-tree HEAD third_party/tinycrypt | awk '{print $3}')"
    actual_tinycrypt="$(git -C "${SCRIPT_DIR}/third_party/tinycrypt" rev-parse HEAD 2>/dev/null || true)"
    if [[ "${actual_tinycrypt}" != "${expected_tinycrypt}" ]]; then
        if [[ -n "${actual_tinycrypt}" ]] &&
           [[ -n "$(git -C "${SCRIPT_DIR}/third_party/tinycrypt" status --porcelain)" ]]; then
            die "TinyCrypt contains local modifications and cannot be updated."
        fi
        log "Initializing the pinned TinyCrypt submodule"
        git -C "${SCRIPT_DIR}" submodule update --init --recursive
        actual_tinycrypt="$(git -C "${SCRIPT_DIR}/third_party/tinycrypt" rev-parse HEAD)"
    fi
    dirty_tinycrypt="$(git -C "${SCRIPT_DIR}/third_party/tinycrypt" status --porcelain)"
    [[ -n "${expected_tinycrypt}" && "${actual_tinycrypt}" == "${expected_tinycrypt}" ]] ||
        die "TinyCrypt is not at the revision recorded by the project."
    [[ -z "${dirty_tinycrypt}" ]] || die "TinyCrypt contains local modifications."
    log "Fetching pinned STM32 HAL and CMSIS dependencies"
    "${VENV_DIR}/bin/python" "${SCRIPT_DIR}/tools/fetch_deps.py" \
        --directory "${DEPS_DIR}"
    verify_dependencies
}

detect_arm_toolchain() {
    local candidate=""

    if [[ -n "${ARM_TOOLCHAIN_PREFIX:-}" ]]; then
        if command -v "${ARM_TOOLCHAIN_PREFIX}-gcc" >/dev/null 2>&1; then
            candidate="$(command -v "${ARM_TOOLCHAIN_PREFIX}-gcc")"
        else
            candidate="${ARM_TOOLCHAIN_PREFIX}-gcc"
        fi
    elif command -v arm-none-eabi-gcc >/dev/null 2>&1; then
        candidate="$(command -v arm-none-eabi-gcc)"
    elif [[ -x /opt/arm-gnu/bin/arm-none-eabi-gcc ]]; then
        candidate="/opt/arm-gnu/bin/arm-none-eabi-gcc"
    fi

    [[ -n "${candidate}" && -x "${candidate}" ]] ||
        die "arm-none-eabi-gcc was not found. Install Arm GNU Toolchain or set ARM_TOOLCHAIN_PREFIX."
    ARM_PREFIX="${candidate%-gcc}"
    [[ -x "${ARM_PREFIX}-objcopy" ]] || die "Missing ${ARM_PREFIX}-objcopy."
    log "Using $(${candidate} --version | sed -n '1p')"
}

clean_build_directory() {
    local directory="$1"
    case "${directory}" in
        "${BUILD_ROOT}"/*) ;;
        *) die "Refusing to clean unexpected build path: ${directory}" ;;
    esac
    [[ "${directory}" != "${BUILD_ROOT}" ]] || die "Refusing to clean build root."
    rm -rf -- "${directory}"
    mkdir -p -- "${directory}"
}

run_native_tests() {
    local directory="${BUILD_ROOT}/posix-release"
    clean_build_directory "${directory}"
    log "Building and testing the POSIX validation instance"
    cmake -S "${SCRIPT_DIR}" -B "${directory}" -G Ninja \
        -DINSTANCE=posix -DCMAKE_BUILD_TYPE=Release
    cmake --build "${directory}"
    ctest --test-dir "${directory}" --output-on-failure
}

build_instance() {
    local instance="$1"
    local directory="$2"
    clean_build_directory "${directory}"
    log "Building ${instance}"
    cmake -S "${SCRIPT_DIR}" -B "${directory}" -G Ninja \
        -DINSTANCE="${instance}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DARM_TOOLCHAIN_PREFIX="${ARM_PREFIX}" \
        -DAUTH_DEPS_ROOT="${DEPS_DIR}" \
        -DAUTH_REQUIRE_PROTECTION="${PROTECTION}"
    grep -Fqx "AUTH_REQUIRE_PROTECTION:BOOL=${PROTECTION}" "${directory}/CMakeCache.txt" ||
        die "${instance} protection profile does not match the requested build."
    cmake --build "${directory}" --target firmware
    [[ -s "${directory}/firmware.bin" && -s "${directory}/firmware.hex" ]] ||
        die "${instance} did not produce firmware.bin and firmware.hex."
}

make_release_id() {
    if [[ -z "${RELEASE_ID}" ]]; then
        RELEASE_ID="$(date -u +'%Y%m%dT%H%M%SZ')-$$"
    fi
    [[ "${RELEASE_ID}" =~ ^[A-Za-z0-9._-]+$ ]] ||
        die "Release IDs may contain only letters, numbers, dot, underscore, and hyphen."
}

provision_release() {
    local h7_build="$1"
    local g431_build="$2"
    local -a args

    make_release_id
    mkdir -p -- "${OUT_ROOT}"
    chmod 700 "${OUT_ROOT}"
    RELEASE_DIR="${OUT_ROOT}/${RELEASE_ID}"
    [[ ! -e "${RELEASE_DIR}" ]] || die "Release already exists: ${RELEASE_DIR}"

    args=(
        --h7 "${h7_build}/firmware.bin"
        --g431 "${g431_build}/firmware.bin"
        --out "${RELEASE_DIR}"
        --counter "${INITIAL_COUNTER}"
    )
    if [[ -n "${KEY_FILE}" ]]; then
        [[ -f "${KEY_FILE}" ]] || die "Key file does not exist: ${KEY_FILE}"
        args+=(--keys "${KEY_FILE}")
    fi

    log "Provisioning the final binaries with the existing Python workflow"
    "${VENV_DIR}/bin/python" "${SCRIPT_DIR}/tools/provision.py" "${args[@]}"

    install -m 600 "${h7_build}/firmware.bin" "${RELEASE_DIR}/h743-firmware.bin"
    install -m 600 "${h7_build}/firmware.hex" "${RELEASE_DIR}/h743-firmware.hex"
    install -m 600 "${g431_build}/firmware.bin" "${RELEASE_DIR}/g431-firmware.bin"
    install -m 600 "${g431_build}/firmware.hex" "${RELEASE_DIR}/g431-firmware.hex"

    {
        printf 'release_id=%s\n' "${RELEASE_ID}"
        printf 'protection_guard=%s\n' "${PROTECTION}"
        printf 'source_commit=%s\n' "$(git -C "${SCRIPT_DIR}" rev-parse HEAD)"
        if [[ -n "$(git -C "${SCRIPT_DIR}" status --porcelain)" ]]; then
            printf 'source_tree=dirty\n'
        else
            printf 'source_tree=clean\n'
        fi
        printf 'arm_gcc=%s\n' "$("${ARM_PREFIX}-gcc" --version | sed -n '1p')"
        printf 'cmake=%s\n' "$(cmake --version | sed -n '1p')"
        (
            cd -- "${RELEASE_DIR}"
            sha256sum h743-firmware.bin g431-firmware.bin h743.hex g431.hex
        )
    } >"${RELEASE_DIR}/build-info.txt"

    chmod 700 "${RELEASE_DIR}"
    find "${RELEASE_DIR}" -type f -exec chmod 600 {} +

    if [[ -e "${OUT_ROOT}/latest" && ! -L "${OUT_ROOT}/latest" ]]; then
        die "${OUT_ROOT}/latest exists and is not a symbolic link."
    fi
    ln -sfn -- "${RELEASE_ID}" "${OUT_ROOT}/latest"
    log "Provisioned release: ${RELEASE_DIR}"
    log "The provisioned HEX/BIN files and keys are private (mode 600)."
}

build_and_provision() {
    require_command cmake
    require_command ninja
    require_command ctest
    require_command stat
    require_command find
    require_command sha256sum
    detect_arm_toolchain
    fetch_dependencies
    run_native_tests

    local h7_build="${BUILD_ROOT}/stm32h743vit6-release"
    local g431_build="${BUILD_ROOT}/stm32g431cbu6-release"
    build_instance stm32h743vit6 "${h7_build}"
    build_instance stm32g431cbu6 "${g431_build}"
    provision_release "${h7_build}" "${g431_build}"
}

resolve_release() {
    if [[ -z "${RELEASE_DIR}" ]]; then
        [[ -L "${OUT_ROOT}/latest" ]] ||
            die "No out/latest release exists. Run './deploy.sh build' first or pass --release."
        RELEASE_DIR="$(readlink -f -- "${OUT_ROOT}/latest")"
    else
        RELEASE_DIR="$(readlink -f -- "${RELEASE_DIR}")"
    fi
    [[ -d "${RELEASE_DIR}" ]] || die "Release directory does not exist: ${RELEASE_DIR}"
}

check_dfu_device() {
    local serial="$1"
    local listing matching paths

    if ! listing="$(dfu-util -l 2>&1)"; then
        die "dfu-util could not access USB. Check USB permissions/udev rules and reconnect the device. Details: ${listing}"
    fi
    matching="$(printf '%s\n' "${listing}" | grep -i "\[${DFU_ID}\]" | grep 'alt=0' || true)"
    [[ -n "${matching}" ]] ||
        die "No STM32 ROM-DFU device (${DFU_ID}, alt 0) is connected. Enter system-memory DFU mode and reconnect USB."
    if [[ -n "${serial}" ]]; then
        printf '%s\n' "${matching}" | grep -Fq "serial=\"${serial}\"" ||
            die "No STM32 ROM-DFU device with serial '${serial}' is connected."
    else
        paths="$(printf '%s\n' "${matching}" | sed -n 's/.*path="\([^"]*\)".*/\1/p' | sort -u)"
        [[ "$(printf '%s\n' "${paths}" | sed '/^$/d' | wc -l)" -eq 1 ]] ||
            die "Multiple STM32 DFU devices are connected. Select one with the matching --h7-serial or --g431-serial option."
    fi
    printf '%s\n' "${matching}" | grep -qi 'internal flash.*0x08000000' ||
        die "DFU alt 0 does not report internal flash at ${FLASH_BASE}; refusing to flash."
}

flash_one() {
    local target="$1"
    local serial image suffix i
    local -a selector addresses images

    require_command dfu-util
    resolve_release
    case "${target}" in
        h7)
            serial="${H7_SERIAL}"
            addresses=(0x08000000 0x081A0000 0x081C0000 0x081E0000)
            images=(h743-firmware.bin h743-provision.bin h743-journal-a.bin h743-journal-b.bin)
            ;;
        g431)
            serial="${G431_SERIAL}"
            addresses=(0x08000000 0x0801F800 0x0801C000 0x0801C800)
            images=(g431-firmware.bin g431-provision.bin g431-journal-a.bin g431-journal-b.bin)
            ;;
        *) die "Internal error: invalid flash target ${target}." ;;
    esac
    for image in "${images[@]}"; do
        [[ -r "${RELEASE_DIR}/${image}" && -s "${RELEASE_DIR}/${image}" ]] ||
            die "Missing provisioned segment: ${RELEASE_DIR}/${image}"
    done
    check_dfu_device "${serial}"

    selector=(-d "${DFU_ID}" -a 0)
    [[ -z "${serial}" ]] || selector+=(-S "${serial}")
    for ((i=0;i<${#images[@]};++i)); do
        suffix=""
        if ((i+1==${#images[@]})); then suffix=":leave"; fi
        log "Flashing ${target} segment ${images[i]} at ${addresses[i]}"
        dfu-util "${selector[@]}" -s "${addresses[i]}${suffix}" -D "${RELEASE_DIR}/${images[i]}" ||
            die "Flashing ${target} failed. The device may be disconnected, protected, or not in ROM DFU mode."
    done
    log "Flashed ${target} successfully"
}

main() {
    parse_args "$@"
    cd -- "${SCRIPT_DIR}"

    case "${COMMAND}" in
        deps)
            fetch_dependencies
            ;;
        build)
            build_and_provision
            ;;
        flash)
            flash_one "${FLASH_TARGET}"
            ;;
        all)
            [[ -n "${H7_SERIAL}" && -n "${G431_SERIAL}" ]] ||
                die "The all command requires --h7-serial and --g431-serial so it cannot swap the two devices."
            [[ "${H7_SERIAL}" != "${G431_SERIAL}" ]] ||
                die "H743 and G431 DFU serial numbers must be different."
            build_and_provision
            flash_one h7
            flash_one g431
            ;;
    esac
}

main "$@"
