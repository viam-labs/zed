OUTPUT_NAME = viam-camera-zed
BIN := build-conan/build/RelWithDebInfo/viam-camera-zed

UNAME_S := $(shell uname -s)
UNAME_M := $(shell uname -m)

ifeq ($(UNAME_S),Linux)
    export DEFAULT_PKG_CONFIG_PATH := /usr/lib/$(UNAME_M)-linux-gnu/pkgconfig:/usr/share/pkgconfig
else ifeq ($(UNAME_S),Darwin)
    ifeq ($(UNAME_M),arm64)
        export DEFAULT_PKG_CONFIG_PATH := /opt/homebrew/lib/pkgconfig:/usr/local/lib/pkgconfig
    else
        export DEFAULT_PKG_CONFIG_PATH := /usr/local/lib/pkgconfig
    endif
endif

ifeq ($(PKG_CONFIG_PATH),)
    export PKG_CONFIG_PATH := $(DEFAULT_PKG_CONFIG_PATH)
else
    export PKG_CONFIG_PATH := $(PKG_CONFIG_PATH):$(DEFAULT_PKG_CONFIG_PATH)
endif

export CONAN_FLAGS := -s:a build_type=Release -s:a compiler.cppstd=17

.PHONY: build setup clean lint conan-pkg

default: module.tar.gz

clean:
	rm -rf build-conan build-native module.tar.gz venv

setup:
	bin/setup.sh

conan-pkg:
	test -f ./venv/bin/activate && . ./venv/bin/activate; \
	conan create . \
	$(CONAN_FLAGS) \
	--build=missing

module.tar.gz: conan-pkg meta.json
	test -f ./venv/bin/activate && . ./venv/bin/activate; \
	conan install --requires=viam-camera-zed/0.0.1 \
	$(CONAN_FLAGS) \
	--lockfile-partial \
	--deployer-package "&" \
	--envs-generation false

lint:
	./bin/lint.sh
