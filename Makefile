CMD := arduino-cli
CORE := esp32:esp32
FQBN := esp32:esp32:esp32
SRCDIR := src
INTERFACE := $(shell $(CMD) board list | grep -oE '/dev/tty(ACM|USB)[0-9]+' | head -1)

compile:
	$(CMD) compile -b $(FQBN) $(SRCDIR)

flash: compile
	$(CMD) upload -p $(INTERFACE) --fqbn $(FQBN) $(SRCDIR)

monitor:
	$(CMD) monitor -p $(INTERFACE) -c baudrate=115200

deps:
	$(CMD) lib install "tzapu/WiFiManager"
	$(CMD) lib install "bblanchon/ArduinoJson"

install:
	$(CMD) config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
	$(CMD) core install $(CORE)

list:
	$(CMD) board list
