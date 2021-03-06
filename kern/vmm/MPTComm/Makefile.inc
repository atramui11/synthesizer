# -*-Makefile-*-

OBJDIRS += $(KERN_OBJDIR)/vmm/MPTComm

KERN_SRCFILES += $(KERN_DIR)/vmm/MPTComm/MPTComm.c
ifdef TEST
KERN_SRCFILES += $(KERN_DIR)/vmm/MPTComm/test.c
endif

$(KERN_OBJDIR)/vmm/MPTComm/%.o: $(KERN_DIR)/vmm/MPTComm/%.c
	@echo + $(COMP_NAME)[KERN/vmm/MPTComm] $<
	@mkdir -p $(@D)
	$(V)$(CCOMP) $(CCOMP_KERN_CFLAGS) -c -o $@ $<

$(KERN_OBJDIR)/vmm/MPTComm/%.o: $(KERN_DIR)/vmm/MPTComm/%.S
	@echo + as[KERN/vmm/MPTComm] $<
	@mkdir -p $(@D)
	$(V)$(CC) $(KERN_CFLAGS) -c -o $@ $<
