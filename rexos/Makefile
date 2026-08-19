# RexOS-TV top-level build orchestrator.
# Everything lands in ./out — RAM-resident artifacts pushed to the TV by
# ./boot/rexos-kexec. Nothing in this build touches the TV's storage.

OUT      := out
KERNEL   := kernel
INITRAM  := initramfs

# Point these at your local copies (defaults match rex's box).
KERNEL_SRC ?= $(HOME)/lgtv-toolkit/kernel-src/kernel/linux-4.4.3
MUSL_DIR    ?= $(CURDIR)/../kexec-poc/arm-linux-musleabihf-cross

.PHONY: all kernel initramfs kexec boot clean

all: kernel initramfs kexec

kernel:
	cd $(KERNEL) && OUT=$(CURDIR)/$(OUT) KERNEL_SRC=$(KERNEL_SRC) ./build.sh

initramfs:
	cd $(INITRAM) && OUT=$(CURDIR)/$(OUT) MUSL_DIR=$(MUSL_DIR) ./build.sh

kexec:
	@mkdir -p $(OUT)
	cp boot/kexec-arm-fixed $(OUT)/

boot: all
	./boot/rexos-kexec

clean:
	rm -rf $(OUT)/*
