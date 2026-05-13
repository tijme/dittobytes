#!/usr/bin/make -f

# GNU General Public License, version 2.0.
#
# Copyright (c) 2025 Tijme Gommers (@tijme).
#
# This source code file is part of Dittobytes. Dittobytes is 
# licensed under GNU General Public License, version 2.0, and 
# you are free to use, modify, and distribute this file under 
# its terms. However, any modified versions of this file must 
# include this same license and copyright notice.

##########################################
## Globals                              ##
##########################################

EXTENSION							   	:= .c
SOURCE_PATH                             ?= ./code/beacon${EXTENSION}
SOURCES								  	:= ./code
SOURCE_FILES 							:= $(shell find $(SOURCES) -name '*${EXTENSION}')
INCLUDE_PATH                           	:= ./code/inc
BUILD_DIR                               := ./build
TESTS_DIR                               := ./ditto/tests
PYTHON_PATH                             := python3
LLVM_DIR_WIN                            := /opt/llvm-winlin/bin
ENTRY_FUNCTION                          ?= shellcode

# A small stamp file per ENTRY_FUNCTION value. When the entry-function changes
# (e.g. switching between `shellcode` and `__main`) we want the intermediate
# LLVM IR files to rebuild. Make will treat a different stamp filename as a
# missing prerequisite and therefore re-run the compile step.
ENTRY_STAMP                             := $(BUILD_DIR)/.entry-$(ENTRY_FUNCTION)

DEBUG                                   := false
EXPAND_MEMCPY_CALLS                     ?= true
EXPAND_MEMSET_CALLS                     ?= true
MOVE_GLOBALS_TO_STACK                   ?= true

IS_COMPILER_CONTAINER                   := $(shell if [ "$(IS_COMPILER_CONTAINER)" = "true" ] || [ -f /tmp/.dittobytes-env-all-encompassing ]; then echo "true"; else echo "false"; fi)

##########################################
## Your build(s)                        ##
##########################################

WIN_AMD64_BEACON_NAME                   := beacon-win-amd64
EXE_WIN_AMD64_BEACON_NAME               := beacon-win-amd64-exe
RAW_WIN_AMD64_BEACON_NAME               := beacon-win-amd64-raw
BOF_WIN_AMD64_BEACON_NAME               := beacon-win-amd64-bof

##########################################
## Metamorphications                    ##
##########################################

MM_DEFAULT						        ?= true
MM_TEST_DEFAULT						    ?= false

MM_TRANSFORM_REG_MOV_IMMEDIATES         ?= $(MM_DEFAULT)
MM_TEST_TRANSFORM_REG_MOV_IMMEDIATES    ?= $(MM_TEST_DEFAULT)

MM_TRANSFORM_STACK_MOV_IMMEDIATES       ?= $(MM_DEFAULT)
MM_TEST_TRANSFORM_STACK_MOV_IMMEDIATES  ?= $(MM_TEST_DEFAULT)

MM_TRANSFORM_NULLIFICATIONS             ?= $(MM_DEFAULT)
MM_TEST_TRANSFORM_NULLIFICATIONS        ?= $(MM_TEST_DEFAULT)

MM_RANDOMIZE_REGISTER_ALLOCATION        ?= $(MM_DEFAULT)
MM_TEST_RANDOMIZE_REGISTER_ALLOCATION   ?= $(MM_TEST_DEFAULT)

MM_RANDOMIZE_FRAME_INSERTIONS           ?= $(MM_DEFAULT)
MM_TEST_RANDOMIZE_FRAME_INSERTIONS      ?= $(MM_TEST_DEFAULT)

MM_INSERT_SEMANTIC_NOISE                ?= $(MM_DEFAULT)
MM_TEST_INSERT_SEMANTIC_NOISE           ?= $(MM_TEST_DEFAULT)

##########################################
## Default runs                         ##
##########################################

# By default compile all beacons
all: check_environment beacons

# Alias for all beacons
beacons: check_environment beacon-all-all-all

# All code/beacons
beacon-all-all-all: check_environment \
	$(EXE_WIN_AMD64_BEACON_NAME) \
	$(RAW_WIN_AMD64_BEACON_NAME) \
	$(BOF_WIN_AMD64_BEACON_NAME)

# Platform & architecture specific code/beacons
beacon-win-amd64-all: check_environment $(EXE_WIN_AMD64_BEACON_NAME) $(RAW_WIN_AMD64_BEACON_NAME) $(BOF_WIN_AMD64_BEACON_NAME)

# Dittobytes loaders
ditto-loaders: check_environment 
	@echo "[+] Calling \`all\` in loaders makefile."
	@$(MAKE) IS_COMPILER_CONTAINER=$(IS_COMPILER_CONTAINER) --no-print-directory -C ./ditto/loaders/

# Dittobytes transpilers
ditto-transpilers: check_environment
	@echo "[+] Calling \`all\` in intermediate transpiler makefile."
	@$(MAKE) IS_COMPILER_CONTAINER=$(IS_COMPILER_CONTAINER) --no-print-directory -C ./ditto/transpilers/intermediate/
	@echo "[+] Calling \`all\` in machine transpiler makefile."
	@$(MAKE) IS_COMPILER_CONTAINER=$(IS_COMPILER_CONTAINER) --no-print-directory -C ./ditto/transpilers/machine/

# Everything
extensive: check_environment ditto-transpilers ditto-loaders code

##########################################
## Test suite                           ##
##########################################

test-suite-build: check_environment
	@set -e; \
	for AVAILABLE_TEST_OS in $(TESTS_DIR)/*; do \
		for AVAILABLE_TEST_ARCH in $$AVAILABLE_TEST_OS/*; do \
			for TEST_FILE in $$AVAILABLE_TEST_ARCH/*; do \
				[ -f "$$TEST_FILE" ] || continue; \
				AVAILABLE_TEST_OS_BASENAME=$$(basename "$$AVAILABLE_TEST_OS"); \
				AVAILABLE_TEST_ARCH_BASENAME=$$(basename "$$AVAILABLE_TEST_ARCH"); \
				AVAILABLE_TEST_FILE_BASENAME=$$(basename "$$TEST_FILE"); \
				[ -n "$(TEST_SOURCE_PATH)" ] && [ "$$(realpath "$$TEST_FILE")" != "$$(realpath $(TEST_SOURCE_PATH))" ] && continue; \
				[ -n "$(TEST_OS)" ] && [ "$$AVAILABLE_TEST_OS_BASENAME" != "all" ] && [ "$$AVAILABLE_TEST_OS_BASENAME" != "$(TEST_OS)" ] && continue; \
				[ -n "$(TEST_ARCH)" ] && [ "$$AVAILABLE_TEST_ARCH_BASENAME" != "all" ] && [ "$$AVAILABLE_TEST_ARCH_BASENAME" != "$(TEST_ARCH)" ] && continue; \
				if [ -n "$(TEST_OS)" ]; then BEACON_OS="$(TEST_OS)"; else BEACON_OS="$$AVAILABLE_TEST_OS_BASENAME"; fi; \
				if [ -n "$(TEST_ARCH)" ]; then BEACON_ARCH="$(TEST_ARCH)"; else BEACON_ARCH="$$AVAILABLE_TEST_ARCH_BASENAME"; fi; \
				echo "[+] TestSuite building \`$$AVAILABLE_TEST_OS_BASENAME-$$AVAILABLE_TEST_ARCH_BASENAME-$$AVAILABLE_TEST_FILE_BASENAME\` for \`$$BEACON_OS-$$BEACON_ARCH\`."; \
				[ -z "$(TEST_METAMORPHICATION)" ] || [ "$(TEST_METAMORPHICATION)" = "original" ] && $(MAKE) IS_COMPILER_CONTAINER=$(IS_COMPILER_CONTAINER) SOURCE_PATH="$$TEST_FILE" BEACON_NAME=$(basename $(notdir $(TESTS_DIR)))_$${AVAILABLE_TEST_OS_BASENAME}_$${AVAILABLE_TEST_ARCH_BASENAME}_$${AVAILABLE_TEST_FILE_BASENAME%.*}_original MM_DEFAULT=false MM_TEST_DEFAULT=false --no-print-directory beacon-$$BEACON_OS-$$BEACON_ARCH-all; \
				[ -z "$(TEST_METAMORPHICATION)" ] || [ "$(TEST_METAMORPHICATION)" = "transpiled" ] && $(MAKE) IS_COMPILER_CONTAINER=$(IS_COMPILER_CONTAINER) SOURCE_PATH="$$TEST_FILE" BEACON_NAME=$(basename $(notdir $(TESTS_DIR)))_$${AVAILABLE_TEST_OS_BASENAME}_$${AVAILABLE_TEST_ARCH_BASENAME}_$${AVAILABLE_TEST_FILE_BASENAME%.*}_transpiled_1 MM_DEFAULT=true MM_TEST_DEFAULT=true --no-print-directory beacon-$$BEACON_OS-$$BEACON_ARCH-all; \
				[ -z "$(TEST_METAMORPHICATION)" ] || [ "$(TEST_METAMORPHICATION)" = "transpiled" ] && $(MAKE) IS_COMPILER_CONTAINER=$(IS_COMPILER_CONTAINER) SOURCE_PATH="$$TEST_FILE" BEACON_NAME=$(basename $(notdir $(TESTS_DIR)))_$${AVAILABLE_TEST_OS_BASENAME}_$${AVAILABLE_TEST_ARCH_BASENAME}_$${AVAILABLE_TEST_FILE_BASENAME%.*}_transpiled_2 MM_DEFAULT=true MM_TEST_DEFAULT=true --no-print-directory beacon-$$BEACON_OS-$$BEACON_ARCH-all; \
				[ -z "$(TEST_METAMORPHICATION)" ] || [ "$(TEST_METAMORPHICATION)" = "transform_reg_mov_immediates" ] && $(MAKE) IS_COMPILER_CONTAINER=$(IS_COMPILER_CONTAINER) SOURCE_PATH="$$TEST_FILE" BEACON_NAME=$(basename $(notdir $(TESTS_DIR)))_$${AVAILABLE_TEST_OS_BASENAME}_$${AVAILABLE_TEST_ARCH_BASENAME}_$${AVAILABLE_TEST_FILE_BASENAME%.*}_transform_reg_mov_immediates MM_DEFAULT=false MM_TEST_DEFAULT=false MM_TRANSFORM_REG_MOV_IMMEDIATES=true MM_TEST_TRANSFORM_REG_MOV_IMMEDIATES=true --no-print-directory beacon-$$BEACON_OS-$$BEACON_ARCH-all; \
				[ -z "$(TEST_METAMORPHICATION)" ] || [ "$(TEST_METAMORPHICATION)" = "transform_stack_mov_immediates" ] && $(MAKE) IS_COMPILER_CONTAINER=$(IS_COMPILER_CONTAINER) SOURCE_PATH="$$TEST_FILE" BEACON_NAME=$(basename $(notdir $(TESTS_DIR)))_$${AVAILABLE_TEST_OS_BASENAME}_$${AVAILABLE_TEST_ARCH_BASENAME}_$${AVAILABLE_TEST_FILE_BASENAME%.*}_transform_stack_mov_immediates MM_DEFAULT=false MM_TEST_DEFAULT=false MM_TRANSFORM_STACK_MOV_IMMEDIATES=true MM_TEST_TRANSFORM_STACK_MOV_IMMEDIATES=true --no-print-directory beacon-$$BEACON_OS-$$BEACON_ARCH-all; \
				[ -z "$(TEST_METAMORPHICATION)" ] || [ "$(TEST_METAMORPHICATION)" = "transform_nullifications" ] && $(MAKE) IS_COMPILER_CONTAINER=$(IS_COMPILER_CONTAINER) SOURCE_PATH="$$TEST_FILE" BEACON_NAME=$(basename $(notdir $(TESTS_DIR)))_$${AVAILABLE_TEST_OS_BASENAME}_$${AVAILABLE_TEST_ARCH_BASENAME}_$${AVAILABLE_TEST_FILE_BASENAME%.*}_transform_nullifications MM_DEFAULT=false MM_TEST_DEFAULT=false MM_TRANSFORM_NULLIFICATIONS=true MM_TEST_TRANSFORM_NULLIFICATIONS=true --no-print-directory beacon-$$BEACON_OS-$$BEACON_ARCH-all; \
				[ -z "$(TEST_METAMORPHICATION)" ] || [ "$(TEST_METAMORPHICATION)" = "randomize_register_allocation" ] && $(MAKE) IS_COMPILER_CONTAINER=$(IS_COMPILER_CONTAINER) SOURCE_PATH="$$TEST_FILE" BEACON_NAME=$(basename $(notdir $(TESTS_DIR)))_$${AVAILABLE_TEST_OS_BASENAME}_$${AVAILABLE_TEST_ARCH_BASENAME}_$${AVAILABLE_TEST_FILE_BASENAME%.*}_randomize_register_allocation MM_DEFAULT=false MM_TEST_DEFAULT=false MM_RANDOMIZE_REGISTER_ALLOCATION=true MM_TEST_RANDOMIZE_REGISTER_ALLOCATION=true --no-print-directory beacon-$$BEACON_OS-$$BEACON_ARCH-all; \
				[ -z "$(TEST_METAMORPHICATION)" ] || [ "$(TEST_METAMORPHICATION)" = "randomize_frame_insertions" ] && $(MAKE) IS_COMPILER_CONTAINER=$(IS_COMPILER_CONTAINER) SOURCE_PATH="$$TEST_FILE" BEACON_NAME=$(basename $(notdir $(TESTS_DIR)))_$${AVAILABLE_TEST_OS_BASENAME}_$${AVAILABLE_TEST_ARCH_BASENAME}_$${AVAILABLE_TEST_FILE_BASENAME%.*}_randomize_frame_insertions MM_DEFAULT=false MM_TEST_DEFAULT=false MM_RANDOMIZE_FRAME_INSERTIONS=true MM_TEST_RANDOMIZE_FRAME_INSERTIONS=true --no-print-directory beacon-$$BEACON_OS-$$BEACON_ARCH-all; \
				[ -z "$(TEST_METAMORPHICATION)" ] || [ "$(TEST_METAMORPHICATION)" = "insert_semantic_noise" ] && $(MAKE) IS_COMPILER_CONTAINER=$(IS_COMPILER_CONTAINER) SOURCE_PATH="$$TEST_FILE" BEACON_NAME=$(basename $(notdir $(TESTS_DIR)))_$${AVAILABLE_TEST_OS_BASENAME}_$${AVAILABLE_TEST_ARCH_BASENAME}_$${AVAILABLE_TEST_FILE_BASENAME%.*}_insert_semantic_noise MM_DEFAULT=false MM_TEST_DEFAULT=false MM_INSERT_SEMANTIC_NOISE=true MM_TEST_INSERT_SEMANTIC_NOISE=true --no-print-directory beacon-$$BEACON_OS-$$BEACON_ARCH-all; \
			done \
		done \
	done

test-suite-test: check_environment
	@set -e; \
	for AVAILABLE_TEST_OS in $(TESTS_DIR)/*; do \
		for AVAILABLE_TEST_ARCH in $$AVAILABLE_TEST_OS/*; do \
			for TEST_FILE in $$AVAILABLE_TEST_ARCH/*; do \
				[ -f "$$TEST_FILE" ] || continue; \
				AVAILABLE_TEST_OS_BASENAME=$$(basename "$$AVAILABLE_TEST_OS"); \
				AVAILABLE_TEST_ARCH_BASENAME=$$(basename "$$AVAILABLE_TEST_ARCH"); \
				AVAILABLE_TEST_FILE_BASENAME=$$(basename "$$TEST_FILE"); \
				[ -n "$(TEST_SOURCE_PATH)" ] && [ "$$(realpath "$$TEST_FILE")" != "$$(realpath $(TEST_SOURCE_PATH))" ] && continue; \
				[ -n "$(TEST_OS)" ] && [ "$$AVAILABLE_TEST_OS_BASENAME" != "all" ] && [ "$$AVAILABLE_TEST_OS_BASENAME" != "$(TEST_OS)" ] && continue; \
				[ -n "$(TEST_ARCH)" ] && [ "$$AVAILABLE_TEST_ARCH_BASENAME" != "all" ] && [ "$$AVAILABLE_TEST_ARCH_BASENAME" != "$(TEST_ARCH)" ] && continue; \
				if [ -n "$(TEST_OS)" ]; then BEACON_OS="$(TEST_OS)"; else BEACON_OS="$${AVAILABLE_TEST_OS_BASENAME}"; fi; \
				if [ -n "$(TEST_ARCH)" ]; then BEACON_ARCH="$(TEST_ARCH)"; else BEACON_ARCH="$${AVAILABLE_TEST_ARCH_BASENAME}"; fi; \
				echo "[+] TestSuite testing  \`$$AVAILABLE_TEST_OS_BASENAME-$$AVAILABLE_TEST_ARCH_BASENAME-$$AVAILABLE_TEST_FILE_BASENAME\` for \`$$BEACON_OS-$$BEACON_ARCH\`."; \
				$(PYTHON_PATH) ./ditto/scripts/tests/test.py $${AVAILABLE_TEST_OS_BASENAME} $${AVAILABLE_TEST_ARCH_BASENAME} $${AVAILABLE_TEST_FILE_BASENAME%.*} $$TEST_FILE $$BEACON_OS $$BEACON_ARCH $$TEST_METAMORPHICATION; \
			done; \
		done; \
	done; \
	echo "[+] TestSuite finished successfully!";

test: test-suite-build test-suite-test

##########################################
## Environment check                    ##
##########################################

check_environment:
ifeq ($(IS_COMPILER_CONTAINER), false)
	@echo "[+] It appears you are not running this command inside the \`Dittobytes Compiler Container\`."
	@echo "[+] You can build it and run in in the root of the Dittobytes project directory."
	@echo "    $ docker buildx build -t dittobytes ."
	@echo "    $ docker run --rm -v ".:/tmp/workdir" -it dittobytes"
	@read -p "[+] Do you want to continue anyway? (y/N) " CONTINUE && \
	case "$$CONTINUE" in \
		[yY][eE][sS]|[yY]) echo "[+] Continuing outside container..." ;; \
		*) echo "[!] Aborting." && exit 1 ;; \
	esac
	$(eval IS_COMPILER_CONTAINER := true)
endif

##########################################
## Windows AMD64                        ##
##########################################

WIN_AMD64_TARGET              := x86_64-w64-mingw32
WIN_AMD64_DEFINES             := -D__WINDOWS__ -D__AMD64__ -DEntryFunction=$(ENTRY_FUNCTION)
WIN_AMD64_BEACON_PATH         := $(BUILD_DIR)/$(WIN_AMD64_BEACON_NAME)$(if $(BEACON_NAME),-$(BEACON_NAME))
WIN_AMD64_BEACON_CL1FLAGS     := -target $(WIN_AMD64_TARGET) $(WIN_AMD64_DEFINES) -I$(INCLUDE_PATH) $(if $(filter true,$(DEBUG_MODE)),-DDEBUG_MODE,) -fuse-ld=lld -O0 -emit-llvm -S -g0 -fPIC -ffreestanding -nostdlib -nodefaultlibs -fno-stack-protector -fpass-plugin=./ditto/transpilers/intermediate/build/libIntermediateTranspiler-`arch`.so -Xclang -disable-O0-optnone -fPIC -fno-rtti -fno-exceptions -fno-delayed-template-parsing -fno-modules -fno-fast-math -fno-builtin -fno-elide-constructors -fno-access-control -fno-jump-tables -fno-omit-frame-pointer -fno-ident -fno-inline -fno-inline-functions -mno-red-zone -fno-use-cxa-atexit -fno-threadsafe-statics -fvisibility=hidden -fvisibility-inlines-hidden
WIN_AMD64_BEACON_LLCFLAGS     := -mtriple $(WIN_AMD64_TARGET) -march=x86-64 -O0 --relocation-model=pic $(if $(filter true,$(MM_RANDOMIZE_REGISTER_ALLOCATION)),--fast-randomize-register-allocation) $(if $(filter true,$(MM_RANDOMIZE_FRAME_INSERTIONS)),--randomize-frame-insertions-amd64 --randomize-frame-insertions-arm64)
WIN_AMD64_BEACON_CL2FLAGS     := -target $(WIN_AMD64_TARGET) $(WIN_AMD64_DEFINES) -fuse-ld=lld -fPIC -ffreestanding -fno-stack-protector -mno-red-zone -fno-use-cxa-atexit -fno-threadsafe-statics -fvisibility=hidden -fvisibility-inlines-hidden

SOURCE_LL_FILES     := $(patsubst $(SOURCES)/%,$(BUILD_DIR)/%,$(SOURCE_FILES:${EXTENSION}=.ll))
SOURCE_PATH_LL      := $(patsubst $(SOURCES)/%,$(BUILD_DIR)/%,$(SOURCE_PATH:${EXTENSION}=.ll))
OTHER_LL_FILES      := $(filter-out $(SOURCE_PATH_LL),$(SOURCE_LL_FILES))

$(BUILD_DIR)/%.ll: $(SOURCES)/%${EXTENSION} | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "[+] Compiling $(WIN_AMD64_BEACON_NAME)$(if $(BEACON_NAME),-$(BEACON_NAME))."
	@echo "    - Intermediate compile of $@."
	@PATH=$(LLVM_DIR_WIN):$(PATH) \
	EXPAND_MEMCPY_CALLS=$(EXPAND_MEMCPY_CALLS) \
	EXPAND_MEMSET_CALLS=$(EXPAND_MEMSET_CALLS) \
	MOVE_GLOBALS_TO_STACK=$(MOVE_GLOBALS_TO_STACK) \
	clang $(WIN_AMD64_BEACON_CL1FLAGS) $< -o $@
	@$(PYTHON_PATH) ./ditto/scripts/make/modify-intermediate-metadata.py $@

$(WIN_AMD64_BEACON_PATH).ll: $(SOURCE_LL_FILES) | $(BUILD_DIR)
	@echo "[+] Compiling $(WIN_AMD64_BEACON_NAME)$(if $(BEACON_NAME),-$(BEACON_NAME))."
	@echo "    - Linking all LLVM IR files with llvm-link"
	@PATH=$(LLVM_DIR_WIN):$(PATH) llvm-link $(SOURCE_PATH_LL) $(OTHER_LL_FILES) -S -o $@
	@$(PYTHON_PATH) ./ditto/scripts/make/modify-intermediate-metadata.py $@

$(WIN_AMD64_BEACON_PATH).meta0.mir: $(WIN_AMD64_BEACON_PATH).ll
	@echo "    - Intermediate compile of $@."
	@PATH=$(LLVM_DIR_WIN):$(PATH) llc $(WIN_AMD64_BEACON_LLCFLAGS) -stop-before=regallocfast -o $@ $<

$(WIN_AMD64_BEACON_PATH).meta1.mir: $(WIN_AMD64_BEACON_PATH).meta0.mir
	@echo "    - Intermediate compile of $@."
	@PATH=$(LLVM_DIR_WIN):$(PATH) \
	MACHINE_TRANSPILER_STEP=first \
	MM_TRANSFORM_REG_MOV_IMMEDIATES=$(MM_TRANSFORM_REG_MOV_IMMEDIATES) MM_TEST_TRANSFORM_REG_MOV_IMMEDIATES=$(MM_TEST_TRANSFORM_REG_MOV_IMMEDIATES) \
	MM_TRANSFORM_STACK_MOV_IMMEDIATES=$(MM_TRANSFORM_STACK_MOV_IMMEDIATES) MM_TEST_TRANSFORM_STACK_MOV_IMMEDIATES=$(MM_TEST_TRANSFORM_STACK_MOV_IMMEDIATES) \
	MM_TRANSFORM_NULLIFICATIONS=$(MM_TRANSFORM_NULLIFICATIONS) MM_TEST_TRANSFORM_NULLIFICATIONS=$(MM_TEST_TRANSFORM_NULLIFICATIONS) \
	MM_INSERT_SEMANTIC_NOISE=$(MM_INSERT_SEMANTIC_NOISE) MM_TEST_INSERT_SEMANTIC_NOISE=$(MM_TEST_INSERT_SEMANTIC_NOISE) \
	llc $(WIN_AMD64_BEACON_LLCFLAGS) -load ./ditto/transpilers/machine/build/libMachineTranspiler-`arch`.so --run-pass=MachineTranspiler -o $@ $<

$(WIN_AMD64_BEACON_PATH).meta2.mir: $(WIN_AMD64_BEACON_PATH).meta1.mir
	@echo "    - Intermediate compile of $@."
	@PATH=$(LLVM_DIR_WIN):$(PATH) llc $(WIN_AMD64_BEACON_LLCFLAGS) -start-before=regallocfast -stop-after=virtregrewriter -o $@ $<

$(WIN_AMD64_BEACON_PATH).meta3.mir: $(WIN_AMD64_BEACON_PATH).meta2.mir
	@echo "    - Intermediate compile of $@."
	@PATH=$(LLVM_DIR_WIN):$(PATH) \
	MACHINE_TRANSPILER_STEP=last \
	MM_TRANSFORM_REG_MOV_IMMEDIATES=$(MM_TRANSFORM_REG_MOV_IMMEDIATES) MM_TEST_TRANSFORM_REG_MOV_IMMEDIATES=$(MM_TEST_TRANSFORM_REG_MOV_IMMEDIATES) \
	MM_TRANSFORM_STACK_MOV_IMMEDIATES=$(MM_TRANSFORM_STACK_MOV_IMMEDIATES) MM_TEST_TRANSFORM_STACK_MOV_IMMEDIATES=$(MM_TEST_TRANSFORM_STACK_MOV_IMMEDIATES) \
	MM_TRANSFORM_NULLIFICATIONS=$(MM_TRANSFORM_NULLIFICATIONS) MM_TEST_TRANSFORM_NULLIFICATIONS=$(MM_TEST_TRANSFORM_NULLIFICATIONS) \
	MM_INSERT_SEMANTIC_NOISE=$(MM_INSERT_SEMANTIC_NOISE) MM_TEST_INSERT_SEMANTIC_NOISE=$(MM_TEST_INSERT_SEMANTIC_NOISE) \
	llc $(WIN_AMD64_BEACON_LLCFLAGS) -load ./ditto/transpilers/machine/build/libMachineTranspiler-`arch`.so --run-pass=MachineTranspiler -o $@ $<

$(WIN_AMD64_BEACON_PATH).obj: $(WIN_AMD64_BEACON_PATH).meta3.mir
	@echo "    - Intermediate compile of $@."
	@PATH=$(LLVM_DIR_WIN):$(PATH) llc $(WIN_AMD64_BEACON_LLCFLAGS) -filetype=obj -start-after=virtregrewriter -o $@ $<
	@$(PYTHON_PATH) ./ditto/scripts/make/notify-user-about-bof.py $<
	@PATH=$(LLVM_DIR_WIN):$(PATH) llvm-strip --strip-debug $@

$(WIN_AMD64_BEACON_PATH).lkd: $(WIN_AMD64_BEACON_PATH).obj
	@echo "    - Intermediate compile of $@."
	@PATH=$(LLVM_DIR_WIN):$(PATH) clang $(WIN_AMD64_BEACON_CL2FLAGS) -e $(ENTRY_FUNCTION) -nostdlib -nodefaultlibs -o $@ $<
	@PATH=$(LLVM_DIR_WIN):$(PATH) llvm-strip --strip-all $@

$(WIN_AMD64_BEACON_PATH).raw: $(WIN_AMD64_BEACON_PATH).lkd
	@echo "    - Intermediate compile of $@."
	@$(PYTHON_PATH) ./ditto/scripts/make/extract-text-segment.py $< $@
		
$(WIN_AMD64_BEACON_PATH).exe: $(WIN_AMD64_BEACON_PATH).obj
	@echo "    - Intermediate compile of $@."
	@PATH=$(LLVM_DIR_WIN):$(PATH) clang $(WIN_AMD64_BEACON_CL2FLAGS) -e $(ENTRY_FUNCTION) -nostdlib -nodefaultlibs -o $@ $<
	@PATH=$(LLVM_DIR_WIN):$(PATH) llvm-strip --strip-all $@

$(EXE_WIN_AMD64_BEACON_NAME):
	@$(MAKE) ENTRY_FUNCTION=__main --no-print-directory $(WIN_AMD64_BEACON_PATH).exe
ifeq ($(DEBUG), false)
	@echo "    - Intermediate cleanup of build files."
	@rm -f $(WIN_AMD64_BEACON_PATH)*.lkd 
	@rm -f $(WIN_AMD64_BEACON_PATH)*.*mir
	@rm -f $(SOURCE_LL_FILES)
endif
	@echo "    - Done building EXE $@."	

$(RAW_WIN_AMD64_BEACON_NAME):
	@$(MAKE) ENTRY_FUNCTION=shellcode --no-print-directory $(WIN_AMD64_BEACON_PATH).raw
ifeq ($(DEBUG), false)
	@echo "    - Intermediate cleanup of build files."
	@rm -f $(WIN_AMD64_BEACON_PATH)*.lkd 
	@rm -f $(WIN_AMD64_BEACON_PATH)*.*mir
	@rm -f $(SOURCE_LL_FILES)
endif
	@echo "    - Done building RAW $@."	

$(BOF_WIN_AMD64_BEACON_NAME):
	@$(MAKE) ENTRY_FUNCTION=go --no-print-directory $(WIN_AMD64_BEACON_PATH).obj
ifeq ($(DEBUG), false)
	@echo "    - Intermediate cleanup of build files."
	@rm -f $(WIN_AMD64_BEACON_PATH)*.lkd 
	@rm -f $(WIN_AMD64_BEACON_PATH)*.*mir
	@rm -f $(SOURCE_LL_FILES)
endif
	@echo "    - Done building BOF $@."

##########################################
## Utility targets                      ##
##########################################

$(BUILD_DIR):
	@echo "[+] Creating build directory."
	@mkdir -p $(BUILD_DIR)

dependencies:
	@echo "[+] Installing Python dependencies."
	@$(PYTHON_PATH) -m pip install --upgrade pip
	@$(PYTHON_PATH) -m pip install -r ditto/scripts/requirements.txt --break-system-packages

clean:
	@echo "[+] Removing compiled user beacons from build folder."
	@rm -rf $(BUILD_DIR)/beacon*
	@rm -rf $(BUILD_DIR)/src

clean-beacons: clean

clean-ditto-loaders:
	@echo "[+] Calling \`clean\` in loaders makefile."
	@$(MAKE) IS_COMPILER_CONTAINER=$(IS_COMPILER_CONTAINER) --no-print-directory -C ./ditto/loaders/ clean

clean-ditto-transpilers:
	@echo "[+] Calling \`clean\` in intermediate transpiler makefile."
	@$(MAKE) IS_COMPILER_CONTAINER=$(IS_COMPILER_CONTAINER) --no-print-directory -C ./ditto/transpilers/intermediate/ clean
	@echo "[+] Calling \`clean\` in machine transpiler makefile."
	@$(MAKE) IS_COMPILER_CONTAINER=$(IS_COMPILER_CONTAINER) --no-print-directory -C ./ditto/transpilers/machine/ clean

clean-extensive: clean-ditto-transpilers clean-ditto-loaders clean-beacons

help:
	@echo "[+] Building your code:"
	@echo "    - make beacon-win-amd64-[format]            			// (Re-)compile your code"
	@echo "      ↳ Available platforms: win"
	@echo "      ↳ Available architectures: amd64"
	@echo "      ↳ Available formats: exe,raw,bof"
	@echo "[+] Dittobytes internals"
	@echo "    - make ditto-loaders                                // (Re-)compile all pre-shipped Ditto shellcode loaders"
	@echo "    - make ditto-transpilers                            // (Re-)compile the pre-shipped Ditto LLVM transpilers/passes"
	@echo "[+] Test suite:"
	@echo "    - make test-suite-build                             // (Re-)compile all feature tests"
	@echo "    - make test-suite-test                              // Run all feature tests (for the current architecture)"
	@echo "[+] Cleanup:"
	@echo "    - make clean                                        // Remove all your code builds ('beacon-*') from the build folder"
	@echo "    - make clean-ditto-loaders                          // Remove all loader builds ('loaders-*') from the build folder"
	@echo "    - make clean-ditto-transpilers                      // Remove all transpiler builds from the transpiler build folders"
	@echo "[+] Help:"
	@echo "    - make help                                         // Show this help message"

.PHONY: all check_environment dependencies clean \
	$(EXE_WIN_AMD64_BEACON_NAME) $(RAW_WIN_AMD64_BEACON_NAME) $(BOF_WIN_AMD64_BEACON_NAME) $(WIN_AMD64_BEACON_PATH) $(WIN_AMD64_BEACON_PATH).exe $(WIN_AMD64_BEACON_PATH).obj $(WIN_AMD64_BEACON_PATH).raw $(WIN_AMD64_BEACON_PATH).lkd $(WIN_AMD64_BEACON_PATH).ll $(WIN_AMD64_BEACON_PATH).meta0.mir $(WIN_AMD64_BEACON_PATH).meta1.mir $(WIN_AMD64_BEACON_PATH).meta2.mir $(WIN_AMD64_BEACON_PATH).meta3.mir \