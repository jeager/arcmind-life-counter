FQBN := esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,USBMode=hwcdc,CDCOnBoot=cdc,FlashMode=qio,PartitionScheme=huge_app
SKETCH := arcmind
OUTPUT_ARG := $(if $(OUTPUT_DIR),--output-dir "$(OUTPUT_DIR)",)

PORT := $(shell \
	if [ -e /dev/cu.usbmodem101 ]; then echo /dev/cu.usbmodem101; \
	elif [ -e /dev/cu.usbmodem1101 ]; then echo /dev/cu.usbmodem1101; \
	fi)

.PHONY: build upload register

build:
	arduino-cli compile --fqbn "$(FQBN)" $(OUTPUT_ARG) $(SKETCH)

upload: _require_port
	arduino-cli upload --fqbn "$(FQBN)" -p $(PORT) $(SKETCH)

register: _require_port
	node scripts/register-device.mjs --port $(PORT) $(if $(NOTES),--notes "$(NOTES)",)

_require_port:
	@test -n "$(PORT)" || { echo "Error: device not found (tried /dev/cu.usbmodem101 and /dev/cu.usbmodem1101)"; exit 1; }
	@echo "Using port: $(PORT)"
