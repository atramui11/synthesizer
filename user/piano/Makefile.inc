# -*-Makefile-*-

OBJDIRS		+= $(USER_OBJDIR)/piano

USER_piano_SRC	+= $(USER_DIR)/piano/piano.c
USER_piano_OBJ	:= $(patsubst %.c, $(OBJDIR)/%.o, $(USER_piano_SRC))
USER_piano_OBJ	:= $(patsubst %.S, $(OBJDIR)/%.o, $(USER_piano_OBJ))
KERN_BINFILES	+= $(USER_OBJDIR)/piano/piano

piano: $(USER_OBJDIR)/piano/piano

$(USER_OBJDIR)/piano/piano: $(USER_LIB_OBJ) $(USER_piano_OBJ)
	@echo + ld[USER/piano] $@
	$(V)$(LD) -o $@ $(USER_LDFLAGS) $(USER_LIB_OBJ) $(USER_piano_OBJ) $(GCC_LIBS)
	$(V)$(OBJDUMP) -S $@ > $@.asm
	$(V)$(NM) -n $@ > $@.sym

$(USER_OBJDIR)/piano/%.o: $(USER_DIR)/piano/%.c
	@echo + cc[USER/piano] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<

$(USER_OBJDIR)/piano/%.o: $(USER_DIR)/piano/%.S
	@echo + as[USER/piano] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(USER_CFLAGS) -c -o $@ $<
