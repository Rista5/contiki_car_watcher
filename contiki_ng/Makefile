CONTIKI_PROJECT = car-watcher
all: $(CONTIKI_PROJECT)

CONTIKI = ../..

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/mqtt

-include $(CONTIKI)/Makefile.identify-target


MODULES_REL += arch/platform/$(TARGET)
MODULES_REL += ./sensors

PROJECT_SOURCEFILES += sensor.c
PROJECT_SOURCEFILES += job.c


PLATFORMS_ONLY = cc26x0-cc13x0 cc2538dk openmote zoul native simplelink cooja

include $(CONTIKI)/Makefile.include
