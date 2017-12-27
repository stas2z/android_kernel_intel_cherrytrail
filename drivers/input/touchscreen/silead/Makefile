obj-$(CONFIG_TOUCHSCREEN_SILEAD)	+= gslx68x-ts.o
ifdef CONFIG_CHUWI_HIBOOK
ifdef CONFIG_CHUWI_HI8PRO
gslx68x-ts-objs := intel_gsl_point_id.o gsl_ts_driver.o
endif
else
gslx68x-ts-objs := gsl_point_id.o gsl_ts_driver.o
$(obj)/gsl_point_id.o: $(srctree)/$(obj)/gsl_point_id
	cp $(srctree)/$(obj)/gsl_point_id $(obj)/gsl_point_id.o
endif
