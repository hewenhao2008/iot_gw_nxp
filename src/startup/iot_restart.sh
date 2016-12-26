#!/bin/sh
# ------------------------------------------------------------------
# Author:    nlv10677
# Copyright: NXP B.V. 2014. All rights reserved
# ------------------------------------------------------------------
#-就是一个脚本文件,可以允许很多命令
/etc/init.d/iot_zb_initd stop > /dev/null
/etc/init.d/iot_sj_initd stop > /dev/null
/etc/init.d/iot_ci_initd stop > /dev/null
/etc/init.d/iot_nd_initd stop > /dev/null
/etc/init.d/iot_gd_initd stop > /dev/null
/etc/init.d/iot_dbp_initd stop > /dev/null
/etc/init.d/iot_su_initd stop > /dev/null

/etc/init.d/iot_zb_initd start > /dev/null
/etc/init.d/iot_sj_initd start > /dev/null
/etc/init.d/iot_ci_initd start > /dev/null
/etc/init.d/iot_nd_initd start > /dev/null
/etc/init.d/iot_gd_initd start > /dev/null
/etc/init.d/iot_dbp_initd start > /dev/null
/etc/init.d/iot_su_initd start > /dev/null
