CONFIG_MODULE_SIG=n
obj-m += kp.o 

kp-objs := main.o metadata_parser.o randomization_utils.o page_cache_management.o direct_io.o \
find_so_base.o kp_test.o addr_to_ptep.o debloating_utils.o kp_print.o kp_dep.o hash_dep.o

all:
	$(MAKE) -C /lib/modules/5.4.273/build M=$(PWD) modules

clean:
	$(MAKE) -C /lib/modules/5.4.273/build M=$(PWD) clean

