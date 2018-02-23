.. include:: <isonum.txt>

=========================
Boot Constraint Subsystem
=========================

:Copyright: |copy| 2017 Viresh Kumar <vireshk@kernel.org>, Linaro Ltd.

Introduction
============

A lot of devices are configured and powered ON by the bootloader before passing
on control to the operating system, Linux in our case.  It is important for some
of them to keep working until the time a Linux device driver probes the device
and reconfigure it.

A typical example of that can be the LCD controller, which is used by the
bootloaders to show image(s) while the platform boots Linux.  The LCD controller
can be using resources like clk, regulators, etc, that are shared with other
devices. These shared resources should be configured in such a way that they
satisfy need of all the user devices.  If another device's (X) driver gets
probed before the LCD controller driver, then it may end up disabling or
reconfiguring these resources to ranges satisfying only the current user (device
X) and that may make the LCD screen unstable and present a bad user experience.

Another case can be a debug serial port (earlycon) enabled from the bootloader,
which may be used to debug early kernel oops.

There are also cases where the resources may not be shared, but the kernel will
disable them forcefully as no users may have appeared until a certain point in
the kernel boot.

Of course we can have more complex cases where the same resource is getting used
by multiple devices while the kernel boots and the order in which the devices
get probed wouldn't matter as the other devices may break because of the chosen
configuration of the first probed device.

Adding boot constraints
=======================

A boot constraint defines a configuration requirement set for the device by the
boot loader. For example, if a clock is enabled for a device by the bootloader
and we want the device to be working as is until the time the device is probed
by its driver, then keeping this clock enabled is one of the boot constraint.

Following are the different type of boot constraints supported currently by the
core:

.. kernel-doc:: include/linux/boot_constraint.h
   :functions: dev_boot_constraint_type


A single boot constraint can be added using the following helper:

.. kernel-doc:: drivers/bootconstraint/core.c
   :functions: dev_boot_constraint_add


The second parameter to this routine describes the constraint to add and is
represented by following structures:

.. kernel-doc:: include/linux/boot_constraint.h
   :functions: dev_boot_constraint dev_boot_constraint_info

The power domain boot constraint doesn't need any data, while the clock and
power supply boot constraint specific data is represented by following
structures:

.. kernel-doc:: include/linux/boot_constraint.h
   :functions: dev_boot_constraint_supply_info dev_boot_constraint_clk_info


In order to simplify adding multiple boot constraints for a platform, the boot
constraints core supplies another helper which can be used to add all
constraints for the platform.

.. kernel-doc:: drivers/bootconstraint/deferrable_dev.c
   :functions: dev_boot_constraint_add_deferrable_of


The argument of this routine is described by following structure:

.. kernel-doc:: include/linux/boot_constraint.h
   :functions: dev_boot_constraint_of


Removing boot constraints
=========================

Once the boot constraints are added, they will be honored by the boot constraint
core until the time a driver tries to probe the device. The constraints are
removed by the driver core if either the driver successfully probed the device
or failed with an error value other than -EPROBE_DEFER. The constraints are kept
as is for deferred probe. The driver core removes the constraints using the
following helper, which must not be called directly by the platforms:

.. kernel-doc:: drivers/bootconstraint/core.c
   :functions: dev_boot_constraints_remove
