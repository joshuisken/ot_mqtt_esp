.PHONY: build upload libs ota

SKETCH ?= .

PORT ?= /dev/ttyUSB0

BOARD ?= esp8266:esp8266:d1_mini_clone
CLIENT ?= mqtt_ot
VERSION ?= 0.6

DEF = --build-property compiler.cpp.extra_flags="-Wall -DCLIENT_NAME=\"$(CLIENT)\" -DVERSION=\"$(VERSION)\""

all: build upload monitor

libs:
	arduino-cli lib install EspMQTTClient OpenTherm\ Library DS18B20 OneWire

upgrade-libs:
	arduino-cli lib update-index
	arduino-cli lib upgrade

secrets:  # Note: create your secrets.h and keep them in secrets.hh, outside source control
	cmp -s secrets.hh secrets.h || cp secrets.hh secrets.h

build: secrets
	arduino-cli compile $(DEF) --export-binaries --fqbn $(BOARD) $(SKETCH)

upload:
	arduino-cli upload -p $(PORT) --fqbn $(BOARD) $(SKETCH)

PWD := $(shell awk 'BEGIN{FS="\""}/BROKER_PASSWORD/{print $$2}' secrets.h)
BIN := $(wildcard build/$(subst :,.,$(BOARD))/*.bin)

ota: build uploadw

uploadw:
	espota --debug --ip=$(CLIENT) --file=$(BIN) --auth=$(PWD)

monitor:
	cu -l $(PORT) -s 115200  # Close with ~. 
