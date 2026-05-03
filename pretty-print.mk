COLOR_RESET   := \033[0m
COLOR_BOLD    := \033[1m
COLOR_DIM     := \033[2m
COLOR_GREEN   := \033[32m
COLOR_YELLOW  := \033[33m
COLOR_BLUE    := \033[34m
COLOR_CYAN    := \033[36m
COLOR_RED     := \033[31m

define silent_or_error
	@$(1) > /dev/null 2>&1 || { echo "ERROR: $(2)"; exit 1; }
endef

# Prologue
define prologue
	@printf "$(COLOR_BOLD)[!] $(1) started!$(COLOR_RESET)\n"
endef

# Epilogue
define epilogue
	@printf "$(COLOR_GREEN)$(COLOR_BOLD)[^^] $(1) complete!$(COLOR_RESET)\n"
endef

# Command
define cecho
	@printf "$(COLOR_CYAN)>>$(COLOR_RESET) $(1)...\n"
endef

define cecho_run
	@printf "$(COLOR_CYAN)>>$(COLOR_RESET) $(1)...\n"
	@$(1)
endef

# Silent build
define silent_build
	@printf "$(COLOR_CYAN)>>$(COLOR_RESET) $(1)...\n"
	@$(1) > /dev/null || { \
		printf "$(COLOR_RED)$(COLOR_BOLD)[X] ERROR $(1) $(COLOR_RESET)\n"; \
		exit 1; \
	}
	@printf "$(COLOR_GREEN)[^^] Done $(1) $(COLOR_RESET)\n"
endef
