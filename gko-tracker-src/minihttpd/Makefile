TOPDIR = .
MAKE = make
OUTPUT = $(TOPDIR)/output

all: 
	mkdir -p output/test;
	mkdir -p output/sample;
	mkdir -p output/lib;
	mkdir -p output/include/libminihttpd;
	$(MAKE) -C $(TOPDIR)/src
	$(MAKE) -C $(TOPDIR)/sample
	$(MAKE) -C $(TOPDIR)/test

clean:
	$(MAKE) -C $(TOPDIR)/src clean
	$(MAKE) -C $(TOPDIR)/sample clean
	$(MAKE) -C $(TOPDIR)/test clean
	rm -rf output


