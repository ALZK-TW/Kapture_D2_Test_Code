#
_XDCBUILDCOUNT = 
ifneq (,$(findstring path,$(_USEXDCENV_)))
override XDCPATH = C:/TI/simplelink_cc13x2_26x2_sdk_3_10_00_53/source;C:/TI/simplelink_cc13x2_26x2_sdk_3_10_00_53/kernel/tirtos/packages
override XDCROOT = C:/TI/xdctools_3_51_02_21_core
override XDCBUILDCFG = ./config.bld
endif
ifneq (,$(findstring args,$(_USEXDCENV_)))
override XDCARGS = 
override XDCTARGETS = 
endif
#
ifeq (0,1)
PKGPATH = C:/TI/simplelink_cc13x2_26x2_sdk_3_10_00_53/source;C:/TI/simplelink_cc13x2_26x2_sdk_3_10_00_53/kernel/tirtos/packages;C:/TI/xdctools_3_51_02_21_core/packages;..
HOSTOS = Windows
endif
