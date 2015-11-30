
UTILOBJ=utils.o rt_names.o ll_types.o ll_proto.o ll_addr.o inet_proto.o

NLOBJ=ll_map.o libnetlink.o

all: libnetlink.a libipr2util.a

libnetlink.a: $(NLOBJ)
	$(AR) rcs $@ $(NLOBJ)

libipr2util.a: $(UTILOBJ) $(ADDLIB)
	$(AR) rcs $@ $(UTILOBJ) $(ADDLIB)

install:

clean:
	rm -f $(NLOBJ) $(UTILOBJ) $(ADDLIB) libnetlink.a libipr2util.a

