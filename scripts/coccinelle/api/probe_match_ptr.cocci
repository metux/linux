// SPDX-License-Identifier: GPL-2.0
//
// use of_match_ptr() macro for assigning .of_match_table field in driver
// use ACPI_PTR() macro for assigning acpi_match_table field in driver
//
// @copyright 2019 Enrico Weigelt, metux IT consult <info@metux.net>
//
virtual context
virtual patch
virtual org
virtual report

@replace_of_match@
constant driver_name;
identifier matchtable;
identifier my_name;
type driver_type;
@@
 static
 driver_type
 my_name = {
           .driver = {
-                    .name = driver_name,
-                    .of_match_table = matchtable,
-           },
+                    .name = driver_name,
+                    .of_match_table = of_match_ptr(matchtable),
+           },
 ...
  };

@replace_acpi_match@
constant driver_name;
identifier matchtable;
identifier my_name;
type driver_type;
@@
 static
 driver_type
 my_name = {
           .driver = {
-                    .name = driver_name,
-                    .acpi_match_table = matchtable,
-           },
+                    .name = driver_name,
+                    .acpi_match_table = ACPI_PTR(matchtable),
+           },
 ...
  };
