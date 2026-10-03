# ReCraft's primary build is intentionally plain GNU make and C99.
# Tested source versions are recorded in third_party/README.md.
APP_NAME ?= ReCraft
RAYLIB_ROOT ?= third_party/raylib-1.4.0
GLFW_ROOT ?= third_party/glfw-3.1.2
GLFW_LIB ?= $(GLFW_ROOT)/build-legacy/src/libglfw3.a
CC ?= gcc
AR ?= ar

BUILD_DIR = build/legacy
VERSION_HEADER = $(BUILD_DIR)/generated/recraft_version.h
OUT_DIR = build
APP_BUNDLE = $(OUT_DIR)/$(APP_NAME).app
APP_EXE = $(APP_BUNDLE)/Contents/MacOS/$(APP_NAME)

RAYLIB_NAMES = core rlgl glad shapes text textures models audio utils camera gestures stb_vorbis
RAYLIB_OBJECTS = $(addprefix $(BUILD_DIR)/raylib/,$(addsuffix .o,$(RAYLIB_NAMES)))
RAYLIB_LIB = $(BUILD_DIR)/libraylib.a
GAME_SOURCES = $(wildcard src/*.c src/*/*.c src/*/*/*.c)
GAME_OBJECTS = $(patsubst src/%.c,$(BUILD_DIR)/game/%.o,$(GAME_SOURCES))

LEGACY_FLAGS = -O2 -Wall -std=gnu99 -fgnu89-inline -arch i386 -mmacosx-version-min=10.6 \
               -DPLATFORM_DESKTOP -DGRAPHICS_API_OPENGL_11 -DRECRAFT_ACCOUNT_USE_BUILD_DEFAULT \
               -DRECRAFT_TITLE='"$(APP_NAME)"'
RAYLIB_INCLUDES = -I$(RAYLIB_ROOT)/src -I$(GLFW_ROOT)/include \
                  -I$(RAYLIB_ROOT)/external/openal_soft/include \
                  -I$(RAYLIB_ROOT)/external/glew/include
GAME_INCLUDES = -Isrc -I$(BUILD_DIR)/generated -I$(RAYLIB_ROOT)/src -I$(GLFW_ROOT)/include -I$(RAYLIB_ROOT)/external/openal_soft/include
MAC_FRAMEWORKS = -framework Cocoa -framework OpenGL -framework IOKit \
                 -framework CoreFoundation -framework CoreVideo -framework OpenAL

.PHONY: legacy runtime-assets clean help

help:
	@printf '%s\n' 'make legacy RAYLIB_ROOT=... GLFW_ROOT=... GLFW_LIB=...' \
	    'See docs/SNOW_LEOPARD_BUILD.md for the pinned dependency setup.'

legacy: $(APP_EXE) $(APP_BUNDLE)/Contents/Info.plist runtime-assets

runtime-assets:
	@while IFS= read -r asset; do \
	    mkdir -p "$(OUT_DIR)/assets/$${asset%/*}" && \
	    cp "assets/$$asset" "$(OUT_DIR)/assets/$$asset" || exit 1; \
	done < assets/runtime_assets.txt

$(BUILD_DIR)/raylib/%.o: $(RAYLIB_ROOT)/src/%.c
	@mkdir -p $(@D)
	$(CC) $(LEGACY_FLAGS) $(RAYLIB_INCLUDES) -c $< -o $@

$(RAYLIB_LIB): $(RAYLIB_OBJECTS)
	@mkdir -p $(@D)
	$(AR) rcs $@ $(RAYLIB_OBJECTS)

$(VERSION_HEADER): VERSION MICROSOFT_CLIENT_ID tools/write_version_header.sh
	@sh tools/write_version_header.sh VERSION $@ MICROSOFT_CLIENT_ID

$(GAME_OBJECTS): $(VERSION_HEADER)

$(BUILD_DIR)/game/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(LEGACY_FLAGS) $(GAME_INCLUDES) -MMD -MP -c $< -o $@

-include $(GAME_OBJECTS:.o=.d)

$(BUILD_DIR)/game/world/beta_blocks.o: src/world/beta_blocks.def src/world/beta_blocks.h

$(APP_EXE): $(GAME_OBJECTS) $(RAYLIB_LIB) $(GLFW_LIB)
	@mkdir -p $(@D)
	$(CC) -arch i386 -mmacosx-version-min=10.6 -o $@ $(GAME_OBJECTS) \
	    $(RAYLIB_LIB) $(GLFW_LIB) $(MAC_FRAMEWORKS) -lm -lz -lpthread

$(APP_BUNDLE)/Contents/Info.plist: assets/Info.plist.in VERSION
	@mkdir -p $(@D)
	@version=$$(sed 's/[-+].*//' VERSION); sed -e 's/@APP_NAME@/$(APP_NAME)/g' -e "s/@APP_VERSION@/$$version/g" -e 's/@MIN_MACOS@/10.6/g' $< > $@

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(APP_EXE) $(APP_BUNDLE)/Contents/MacOS/$(APP_NAME) $(APP_BUNDLE)/Contents/Info.plist
