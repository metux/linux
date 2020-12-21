===============
Power off hooks
===============

Copyright (c) 2020 Enrico Weigelt <info@metux.net>, metux IT consult

Machine power off requires board specific actions (eg. switching down
regulators, talking to some external controller, etc). While common PCs
implement this in ACPI, other platforms need special handling, depending
on cpu architecture, machine type or even board specific.

The power off hooks provide a generic mechanism for archs and platform
drivers to register callbacks that are called when power to the machine
should be cut.

.. contents:

   1. General design
   2. Poweroff preparation hooks
   3. Poweroff hooks

1. General design
=================

The hooks are implemented by notifier chains (see include/linux/notifier.h),
which are lists of callbacks with an priority: calling a notifier chain
causes all registered notifiers to be called sequentially, higher priority
before lower.

In order to allow more specific drivers to take precedence over less specific
ones (eg. board specific over architecture generic), the following priority
slots are defined:

+------------+----------------------------------------------------+
| 6000..6499 | application specific (eg. external power supplies) |
+------------+----------------------------------------------------+
| 5000..5499 | board specific (eg. board drivers or oftree)       |
+------------+----------------------------------------------------+
| 4000..4499 | firmware (eg. acpi)                                |
+------------+----------------------------------------------------+
| 3000..3499 | machine type                                       |
+------------+----------------------------------------------------+
| 2000..2499 | architecture                                       |
+------------+----------------------------------------------------+

(wholes between the ranges are left for future use)

Notifiers should only either return NOTIFY_OK or NOTIFY_STOP, depending on
whether they want the lower priority handlers also to be called or skipped.

2. Poweroff preparation hooks
=============================

Poweroff preparation hooks are called before reboot, poweroff as well as
S5 sleep / standby. The call is made right before the kernel migrates all
processes to the boot cpu and starts further shutdown operations.

API functions:

* register_pm_power_off_prepare(notifier_block):

  registers a notifier block to be called on poweroff preparation

* unregister_pm_power_off_prepare(notifier_block):

  unregisters a notifier block

* devm_register_pm_power_off_prepare(device, notifier_block)

  device-managed variant of registering a notifier block

* call_pm_power_off_prepare()

  call the poweroff preparation notifier chain

3. Poweroff hooks
=================

Poweroff hooks are called when shutdown is completed and finally power
shall be cut off.

API functions:

* register_pm_power_off(notifier_block):

  registers a notifier block to be called on poweroff

* unregister_pm_power_off(notifier_block):

  unregisters a notifier block

* devm_register_pm_power_off(device, notifier_block)

  device-managed variant of registering a notifier block

* call_pm_power_off()

  call the poweroff notifier chain
