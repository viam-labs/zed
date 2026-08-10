#!/bin/bash
# set -e: exit with errors if anything fails
#     -u: it's an error to use an undefined variable
#     -x: print out every command before it runs
#     -o pipefail: if something in the middle of a pipeline fails, the whole thing fails
#

set -euxo pipefail


OS=$(uname -s | tr '[:upper:]' '[:lower:]')

if [[ ${OS} == "linux" ]]; then
    sudo apt-get update
    sudo apt-get install -y wget gpg lsb-release

    # Add Kitware repository for up-to-date CMake if on Ubuntu
    # This is required for the publish step to work
    if lsb_release -is | grep -q "Ubuntu"; then
        sudo wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | sudo gpg --dearmor - | sudo tee /usr/share/keyrings/kitware-archive-keyring.gpg >/dev/null
        sudo echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/kitware.list >/dev/null
        sudo apt-get update
    fi

    sudo apt-get install -y \
        python3 \
        python3-venv \
        python3-pip \
        cmake \
        cmake-data \
        autoconf \
        automake \
        build-essential \
        ca-certificates \
        curl \
        doxygen \
        g++ \
        git \
        gdb \
        gnupg \
        less \
        libssl-dev \
        libudev-dev \
        ninja-build \
        pkg-config \
        software-properties-common

    ARCH=$(uname -m)

    # CUDA: Jetson ships it via JetPack. x86_64 hosts (CI runners) install cuda-toolkit-12-6
    # plus NVIDIA driver user-space libs (libcuda, libnvcuvid, libnvidia-encode) so ZED SDK
    # can link without a physical GPU.
    if [[ "${ARCH}" == "x86_64" ]] && ! command -v nvcc >/dev/null 2>&1; then
        wget -qO /tmp/cuda-keyring.deb \
            https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-keyring_1.1-1_all.deb
        sudo dpkg -i /tmp/cuda-keyring.deb
        sudo apt-get update
        sudo apt-get install -y --no-install-recommends \
            cuda-toolkit-12-6 \
            libnvidia-compute-570 \
            libnvidia-decode-570 \
            libnvidia-encode-570
        rm /tmp/cuda-keyring.deb
    fi

    # ZED SDK 5.0. Pinned to L4T36.4 on Jetson, Ubuntu22 + CUDA 12 on x86_64.
    if [ ! -d /usr/local/zed ]; then
        if [[ "${ARCH}" == "aarch64" ]]; then
            ZED_URL="https://download.stereolabs.com/zedsdk/5.0/l4t36.4/jetsons"
            ZED_RUN="/tmp/ZED_SDK_Tegra_L4T36.4_v5.0.run"
        else
            ZED_URL="https://download.stereolabs.com/zedsdk/5.0/cu12/ubuntu22"
            ZED_RUN="/tmp/ZED_SDK_Ubuntu22_v5.0.run"
        fi
        wget --max-redirect=5 -O "${ZED_RUN}" "${ZED_URL}"
        # Guard: Stereolabs' web tier sometimes serves the product HTML page when
        # a version+platform combo is missing. A real installer is ~80MB.
        if [ "$(stat -c%s "${ZED_RUN}")" -lt 10000000 ]; then
            echo "ERROR: ZED SDK download too small ($(stat -c%s "${ZED_RUN}") bytes) — likely got HTML" >&2
            head -c 200 "${ZED_RUN}" >&2
            exit 1
        fi
        chmod +x "${ZED_RUN}"
        "${ZED_RUN}" -- silent skip_tools skip_python
        rm "${ZED_RUN}"
    fi
elif [[ ${OS} == "darwin" ]]; then
    if ! command -v brew >/dev/null 2>&1; then
        echo "Homebrew not found. Please install it first: https://brew.sh/"
        exit 1
    fi
    # On macOS, these are typically enough for the build
    brew install cmake pkg-config libusb ninja python
fi

# Check python3 availability
if ! command -v python3 >/dev/null 2>&1; then
  echo "python3 not found in PATH. Aborting." >&2
  exit 1
fi

# Check venv module
if ! python3 -m venv --help >/dev/null 2>&1; then
  echo "python3 venv module not available. Try: sudo apt-get install --reinstall python3-venv python3-full python3-pip" >&2
  exit 1
fi


if [ ! -f "./venv/bin/activate" ]; then
  echo 'creating and sourcing virtual env'
  if ! python3 -m venv venv; then
    echo "Failed to create venv. Trying with full path to python3."
    if ! /usr/bin/python3 -m venv venv; then
      echo "Failed to create venv with /usr/bin/python3. Aborting." >&2
      exit 1
    fi
  fi
  source ./venv/bin/activate
else
  echo 'sourcing virtual env'
  source ./venv/bin/activate
fi


# Set up conan
if [ ! -f "./venv/bin/conan" ]; then
  echo 'installing conan'
  . ./venv/bin/activate
  pip install --upgrade pip
  pip install conan
fi

conan profile detect || echo "Conan is already installed"
conan remote add viamconan https://viam.jfrog.io/artifactory/api/conan/viamconan --index 0 || echo "Viam conan remote already exists"
