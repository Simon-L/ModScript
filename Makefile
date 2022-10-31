# If RACK_DIR is not defined when calling the Makefile, default to two directories above
RACK_DIR ?= ../..

# FLAGS will be passed to both the C and C++ compiler
FLAGS += -Idep/include
CFLAGS +=
CXXFLAGS +=

include $(RACK_DIR)/arch.mk

# Adapted from VCV-Prototype https://github.com/VCVRack/VCV-Prototype/blob/v2/Makefile#L74
# and https://github.com/AriaSalvatrice/AriaModules/blob/master/Makefile#L31-L46
luajit := dep/lib/libluajit-5.1.a
OBJECTS += $(luajit)
DEPS += $(luajit)
$(luajit):
	git clone https://github.com/openresty/luajit2 dep/luajit2
	# Remove the root system paths from the default path
	sed -i 's/\<LUA_JPATH LUA_LLPATH LUA_RLPATH\>/LUA_JPATH LUA_RLPATH/g' dep/luajit2/src/luaconf.h
	sed -i 's/\<LUA_LCPATH1 LUA_RCPATH LUA_LCPATH2\>/LUA_RCPATH/g' dep/luajit2/src/luaconf.h
# handle the special case of building for Windows using the rack-plugin-toolchain
# this is set here https://github.com/VCVRack/rack-plugin-toolchain/blob/v2/Makefile#L138
# TODO: test OSX compability and find a proper way to handle this
ifneq (,$(findstring x86_64-w64-mingw32,$(PATH)))
		$(MAKE) -C dep/luajit2 HOST_CC="gcc" CROSS=x86_64-w64-mingw32- TARGET_SYS=Windows BUILDMODE=static PREFIX="$(DEP_PATH)"
		# separate make and make install to fix naming for windows
		cd dep/luajit2/src && mv luajit.exe luajit
		$(MAKE) -C dep/luajit2 HOST_CC="gcc" CROSS=x86_64-w64-mingw32- TARGET_SYS=Windows BUILDMODE=static PREFIX="$(DEP_PATH)" install
else	
		$(MAKE) -C dep/luajit2 BUILDMODE=static PREFIX="$(DEP_PATH)" install
endif

losc := dep/share/lua/5.1/losc
DEPS += $(losc)
$(losc): $(luajit)
	git clone https://github.com/davidgranstrom/losc dep/losc
	cp -r dep/losc/src/losc dep/share/lua/5.1/

liblo := dep/lib/liblo.a
OBJECTS += $(liblo)
DEPS += $(liblo)
$(liblo):
	git clone https://github.com/radarsat1/liblo dep/liblo
	cd dep/liblo && ./autogen.sh --prefix="$(DEP_PATH)" --enable-static=yes --enable-shared=no --disable-tests --disable-network-tests --disable-tools --disable-examples
	$(MAKE) -C dep/liblo
	$(MAKE) -C dep/liblo install

# Careful about linking to shared libraries, since you can't assume much about the user's environment and library search path.
# Static libraries are fine, but they should be added to this plugin's build system.
LDFLAGS +=

# Add .cpp files to the build
SOURCES += $(wildcard src/*.cpp)

# Add files to the ZIP package when running `make dist`
# The compiled plugin and "plugin.json" are automatically added.
DISTRIBUTABLES += res scripts/example.lua scripts/apcmini.lua scripts/lib/* scripts/mpk_303.lua scripts/keyboard.lua
DISTRIBUTABLES += $(wildcard LICENSE*)
DISTRIBUTABLES += $(wildcard presets)

# Include the Rack plugin Makefile framework
include $(RACK_DIR)/plugin.mk
