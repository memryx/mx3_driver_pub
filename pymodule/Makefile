RM := rm -rfv
CP := cp -v
PYTHON := python3

BUILD_DIR := build
BINARY_DIR := bin

.phony: create_dir
create_dir:
	@mkdir -p $(BINARY_DIR)

.phony: memx
memx: create_dir
	$(PYTHON) setup.py $(BUILD_DIR);
	$(CP) $(BUILD_DIR)/lib.*/*.so $(BINARY_DIR)

.phony: all
all: memx

.phony: clean
clean:
	@$(RM) $(BUILD_DIR)
	@$(RM) $(BINARY_DIR)

