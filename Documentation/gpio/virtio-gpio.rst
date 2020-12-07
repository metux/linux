"""""""""""""""""
Virtio-GPIO protocol specification
"""""""""""""""""
...........
Specification for virtio-based virtiual GPIO devices
...........

+------------
+Version_ 1.0
+------------

===================
General
===================

The virtio-gpio protocol provides access to general purpose IO devices
to virtual machine guests. These virtualized GPIOs could be either provided
by some simulator (eg. virtual HIL), routed to some external device or
routed to real GPIOs on the host (eg. virtualized embedded applications).

Instead of simulating some existing real GPIO chip within an VMM, this
protocol provides an hardware independent interface between host and guest
that solely relies on an active virtio connection (no matter which transport
actually used), no other buses or additional platform driver logic required.

===================
Protocol layout
===================

----------------------
Configuration space
----------------------

+--------+----------+-------------------------------+
| Offset | Type     | Description                   |
+========+==========+===============================+
| 0x00   | u8       | version                       |
+--------+----------+-------------------------------+
| 0x02   | u16      | number of GPIO lines          |
+--------+----------+-------------------------------+
| 0x04   | u32      | size of gpio name block       |
+--------+----------+-------------------------------+
| 0x20   | char[32] | device name (0-terminated)    |
+--------+----------+-------------------------------+
| 0x40   | char[]   | line names block              |
+--------+----------+-------------------------------+

- for version field currently only value 1 supported.
- the line names block holds a stream of zero-terminated strings,
  holding the individual line names.
- unspecified fields are reserved for future use and should be zero.

------------------------
Virtqueues and messages:
------------------------

- Queue #0: transmission from device to cpu
- Queue #1: transmission from cpu to device

The queues transport messages of the struct virtio_gpio_msg:

Message format:
---------------

+--------+----------+---------------+
| Offset | Type     | Description   |
+========+==========+===============+
| 0x00   | uint16   | event type    |
+--------+----------+---------------+
| 0x02   | uint16   | line id       |
+--------+----------+---------------+
| 0x04   | uint32   | value         |
+--------+----------+---------------+

Message types:
--------------

+-------+----------------------------------------+-----------------------------+
| Code  | Symbol                                |                             |
+=======+========================================+=============================+
| 0x01  | VIRTIO_GPIO_MSG_GUEST_REQUEST          | request gpio line           |
+-------+---------------------------------------+-----------------------------+
| 0x02  | VIRTIO_GPIO_MSG_GUEST_DIRECTION_INPUT  | set direction to input      |
+-------+---------------------------------------+-----------------------------+
| 0x03  | VIRTIO_GPIO_MSG_GUEST_DIRECTION_OUTPUT | set direction to output     |
+-------+---------------------------------------+-----------------------------+
| 0x04  | VIRTIO_GPIO_MSG_GUEST_GET_DIRECTION    | read current direction      |
+-------+---------------------------------------+-----------------------------+
| 0x05  | VIRTIO_GPIO_MSG_GUEST_GET_VALUE        | read current level          |
+-------+---------------------------------------+-----------------------------+
| 0x06  | VIRTIO_GPIO_MSG_GUEST_SET_VALUE        | set current (out) level     |
+-------+---------------------------------------+-----------------------------+
| 0x11  | VIRTIO_GPIO_MSG_HOST_LEVEL             | state changed (host->guest) |
+-------+----------------+-----------------------+-----------------------------+

----------------------
Data flow:
----------------------

- all operations, except ``VIRTIO_GPIO_MSG_HOST_LEVEL``, are guest-initiated
- host replies ``VIRTIO_GPIO_MSG_HOST_LEVEL`` OR'ed to the ``type`` field
- ``VIRTIO_GPIO_MSG_HOST_LEVEL`` is only sent asynchronically from host to guest
- in replies, a negative ``value`` field denotes an unix-style errno code
- valid direction values are:
  * 0 = output
  * 1 = input
- valid line state values are:
  * 0 = inactive
  * 1 = active

VIRTIO_GPIO_MSG_GUEST_REQUEST
----------------------------

- notify the host that given line# is going to be used
- request:
  * ``line`` field: line number
  * ``value`` field: unused
- reply:
  * ``value`` field: errno code (0 = success)

VIRTIO_GPIO_MSG_GUEST_DIRECTION_INPUT
------------------------------------

- set line line direction to input
- request:
  * ``line`` field: line number
  * ``value`` field: unused
- reply: value field holds errno
  * ``value`` field: errno code (0 = success)

VIRTIO_GPIO_MSG_GUEST_DIRECTION_OUTPUT
-------------------------------------

- set line direction to output and given line state
- request:
  * ``line`` field: line number
  * ``value`` field: output state (0=inactive, 1=active)
- reply:
  * ``value`` field: holds errno

VIRTIO_GPIO_MSG_GUEST_GET_DIRECTION
----------------------------------

- retrieve line direction
- request:
  * ``line`` field: line number
  * ``value`` field: unused
- reply:
  * ``value`` field: direction (0=output, 1=input) or errno code

VIRTIO_GPIO_MSG_GUEST_GET_VALUE
------------------------------

- retrieve line state value
- request:
  * ``line`` field: line number
  * ``value`` field: unused
- reply:
  * ``value`` field: line state (0=inactive, 1=active) or errno code

VIRTIO_GPIO_MSG_GUEST_SET_VALUE
------------------------------

- set line state value (output only)
- request:
  * ``line`` field: line number
  * ``value`` field: line state (0=inactive, 1=active)
- reply:
  * ``value`` field: new line state or errno code

VIRTIO_GPIO_MSG_HOST_LEVEL
-------------------------

- async notification from host to gues: line state changed
- ``line`` field: line number
- ``value`` field: new line state (0=inactive, 1=active)
