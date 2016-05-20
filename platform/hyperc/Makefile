PREFIX ?= $(shell pwd)/../prefix/$(CROSS:%-=%)
NAME   :=aws-iot
TARGET :=arm-none-eabi
CROSS  :=$(TARGET)-
SOC    ?=soc-sim
CPU    :=arm
PLT:=hyperc
INCLUDE:=-Iinclude -I$(PREFIX)/include \
	-I$(PREFIX)/include/lwip \
	-I$(PREFIX)/include/lwip/ipv4 \
	-Isamples/linux/shadow_sample \
	-Iexternal_libs/jsmn \
	-Iplatform/$(PLT) \
	$(SOC_INC)
COPTS  ?=-march=armv7-a -mthumb -msoft-float
AARCH  :=$(shell echo $(COPTS) | sed -e 's/.*armv\([0-9]\).*/\1/g')
MOPTS  :=$(COPTS) \
          -DCFG_AARCH=$(AARCH) \
          -fno-builtin -fno-common \
          -ffunction-sections -fdata-sections
CONFIG := -DAWS_HYPERC_DBG=1 \
	#-D_ENABLE_THREAD_SUPPORT_
ASFLAGS:=$(MOPTS) $(CONFIG) -O2 -g -Wall -Werror -D __ASSEMBLY__
CFLAGS :=$(MOPTS) $(CONFIG) -O2 -g -Wall -Werror -fno-builtin
LSCRIPT:=ram.ld
LDFLAGS:=$(MOPTS) -g -nostartfiles -nodefaultlibs -L $(PREFIX)/lib -T$(LSCRIPT) dbg.o
MSCRIPT:=
LIB    :=lib$(NAME).a

ALL    :=lib _header
CLEAN  :=

VPATH  :=src external_libs/jsmn platform/$(PLT)
VOBJ   :=$(patsubst %.S,%.o, \
		$(patsubst %.c,%.o, \
		$(patsubst %.cpp, %.o, \
			$(notdir $(foreach DIR,$(VPATH),\
				$(wildcard $(DIR)/*.S)	\
				$(wildcard $(DIR)/*.c) 	\
				$(wildcard $(DIR)/*.cpp))))))
default:all

include $(PREFIX)/share/Makefile.rule

_header:
	mkdir -p $(PREFIX)/include/$(NAME)
	cp platform/$(PLT)/*.h $(PREFIX)/include/$(NAME)
