## -*- mode: makefile; -*-

SUBDIRS = src

all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	@for d in $(SUBDIRS); do $(MAKE) -C $$d clean; done

.PHONY: clean $(SUBDIRS)
