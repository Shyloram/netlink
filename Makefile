WIFILIB_PATH:=$(PWD)
#CROSS_COMPILER:= arm-hisiv100nptl-linux-
#CROSS_COMPILER:= arm-hisiv300-linux-
CROSS_COMPILER:= arm-anykav200-linux-uclibcgnueabi-
CC:=$(CROSS_COMPILER)g++	
GCC:=$(CROSS_COMPILER)gcc
AR:=$(CROSS_COMPILER)ar

CFLAGS:= -Wall -fpic -O2 -fno-strict-aliasing 

#platform
#PLATFORM = HISI
PLATFORM = ANKA

ifeq ($(PLATFORM), HISI)
	CFLAGS += -DHISI

else ifeq($(PLATFORM), ANKA)
	CFLAGS += -DANKA
endif

#enable zink or eink
CFLAGS += -DEN_ZINK

SRC +=$(wildcard $(WIFILIB_PATH)/*.cpp)
OBJ = $(SRC:%.cpp=%.o)

LIBNAME		=libwifi.a

bin   =   wifi

all: $(OBJ) lib  	
$(OBJ): %.o:%.cpp	
	$(CC) $(CFLAGS)  -c -o $@ $<   
	
lib: $(OBJ)
	$(AR) -rcu $(LIBNAME) $(OBJ) 

$(bin):$(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread
	cp ./wifi ~/nfs/

clean:	
	rm -f $(LIBNAME)
	rm -f $(OBJ)
	rm -f $(bin)

.PHONY:clean lib all
