PIN_ROOT ?= /opt/pin

CXX = g++-11
CXXFLAGS = -O2 -fPIC -std=c++11 -Wall -Werror \
           -I$(PIN_ROOT)/source/include/pin \
           -I$(PIN_ROOT)/source/include/pin/gen \
           -I$(PIN_ROOT)/extras/xed-intel64/include/xed \
           -I$(PIN_ROOT)/extras/crt/include \
	   -I/usr/lib/modules/6.14.9-arch1-1/build/include/ \
           -DTARGET_IA32E -D_LINUX_ -D_GNU_SOURCE

LDFLAGS = -shared

TARGET = mytool.so
OBJS = mytool.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

