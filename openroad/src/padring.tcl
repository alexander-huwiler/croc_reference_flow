# Copyright (c) 2024 ETH Zurich and University of Bologna.
# Licensed under the Apache License, Version 2.0, see LICENSE for details.
# SPDX-License-Identifier: Apache-2.0
#
# Authors:
# - Philippe Sauter <phsauter@iis.ee.ethz.ch>

# Chip geometry comes from floorplan.tcl, keep only the pad positioning here.
# Pad order and positions follow the bond diagram for the 64-pad frame.
# -location is the IO-cell offset along each edge.

make_io_sites -horizontal_site sg13cmos5l_ioSite \
    -vertical_site sg13cmos5l_ioSite \
    -corner_site sg13cmos5l_ioSite \
    -offset $padBond \
    -rotation_horizontal R0 \
    -rotation_vertical R0 \
    -rotation_corner R0

##########################################################################
# Edge: LEFT (top to bottom)                                             #
##########################################################################
place_pad -row IO_WEST -location 1586 "pad_vddio0"
place_pad -row IO_WEST -location 1484 "pad_uart_rx_i"
place_pad -row IO_WEST -location 1382 "pad_uart_tx_o"
place_pad -row IO_WEST -location 1280 "pad_testmode_i"
place_pad -row IO_WEST -location 1178 "pad_status_o"
place_pad -row IO_WEST -location 1076 "pad_clk_i"
place_pad -row IO_WEST -location  974 "pad_ref_clk_i"
place_pad -row IO_WEST -location  872 "pad_rst_ni"
place_pad -row IO_WEST -location  770 "pad_jtag_tck_i"
place_pad -row IO_WEST -location  668 "pad_jtag_trst_ni"
place_pad -row IO_WEST -location  566 "pad_jtag_tms_i"
place_pad -row IO_WEST -location  464 "pad_jtag_tdi_i"
place_pad -row IO_WEST -location  362 "pad_jtag_tdo_o"
place_pad -row IO_WEST -location  260 "pad_vdd0"

##########################################################################
# Edge: BOTTOM (left to right)                                           #
##########################################################################
place_pad -row IO_SOUTH -location  250 "pad_vss0"
place_pad -row IO_SOUTH -location  358 "pad_vssio1"
place_pad -row IO_SOUTH -location  466 "pad_vddio1"
place_pad -row IO_SOUTH -location  574 "pad_gpio0_io"
place_pad -row IO_SOUTH -location  682 "pad_gpio1_io"
place_pad -row IO_SOUTH -location  790 "pad_gpio2_io"
place_pad -row IO_SOUTH -location  898 "pad_gpio3_io"
place_pad -row IO_SOUTH -location 1006 "pad_gpio4_io"
place_pad -row IO_SOUTH -location 1114 "pad_gpio5_io"
place_pad -row IO_SOUTH -location 1222 "pad_gpio6_io"
place_pad -row IO_SOUTH -location 1330 "pad_gpio7_io"
place_pad -row IO_SOUTH -location 1438 "pad_gpio8_io"
place_pad -row IO_SOUTH -location 1546 "pad_gpio9_io"
place_pad -row IO_SOUTH -location 1654 "pad_gpio10_io"
place_pad -row IO_SOUTH -location 1762 "pad_gpio11_io"
place_pad -row IO_SOUTH -location 1870 "pad_vdd1"
place_pad -row IO_SOUTH -location 1978 "pad_vss1"
place_pad -row IO_SOUTH -location 2086 "pad_vssio2"

##########################################################################
# Edge: RIGHT (bottom to top)                                            #
##########################################################################
place_pad -row IO_EAST -location  250 "pad_vddio2"
place_pad -row IO_EAST -location  352 "pad_gpio12_io"
place_pad -row IO_EAST -location  454 "pad_gpio13_io"
place_pad -row IO_EAST -location  556 "pad_gpio14_io"
place_pad -row IO_EAST -location  658 "pad_gpio15_io"
place_pad -row IO_EAST -location  760 "pad_gpio16_io"
place_pad -row IO_EAST -location  862 "pad_gpio17_io"
place_pad -row IO_EAST -location  964 "pad_gpio18_io"
place_pad -row IO_EAST -location 1066 "pad_gpio19_io"
place_pad -row IO_EAST -location 1168 "pad_gpio20_io"
place_pad -row IO_EAST -location 1270 "pad_gpio21_io"
place_pad -row IO_EAST -location 1372 "pad_gpio22_io"
place_pad -row IO_EAST -location 1474 "pad_gpio23_io"
place_pad -row IO_EAST -location 1576 "pad_vdd2"

##########################################################################
# Edge: TOP (left to right)                                              #
##########################################################################
place_pad -row IO_NORTH -location  250 "pad_vssio0"
place_pad -row IO_NORTH -location  358 "pad_vss3"
place_pad -row IO_NORTH -location  466 "pad_vdd3"
place_pad -row IO_NORTH -location  574 "pad_unused3_o"
place_pad -row IO_NORTH -location  682 "pad_unused2_o"
place_pad -row IO_NORTH -location  790 "pad_unused1_o"
place_pad -row IO_NORTH -location  898 "pad_unused0_o"
place_pad -row IO_NORTH -location 1006 "pad_gpio31_io"
place_pad -row IO_NORTH -location 1114 "pad_gpio30_io"
place_pad -row IO_NORTH -location 1222 "pad_gpio29_io"
place_pad -row IO_NORTH -location 1330 "pad_gpio28_io"
place_pad -row IO_NORTH -location 1438 "pad_gpio27_io"
place_pad -row IO_NORTH -location 1546 "pad_gpio26_io"
place_pad -row IO_NORTH -location 1654 "pad_gpio25_io"
place_pad -row IO_NORTH -location 1762 "pad_gpio24_io"
place_pad -row IO_NORTH -location 1870 "pad_vddio3"
place_pad -row IO_NORTH -location 1978 "pad_vssio3"
place_pad -row IO_NORTH -location 2086 "pad_vss2"

# Fill in the rest of the padring
place_corners $iocorner

place_io_fill -row IO_NORTH {*}$iofill
place_io_fill -row IO_SOUTH {*}$iofill
place_io_fill -row IO_WEST  {*}$iofill
place_io_fill -row IO_EAST  {*}$iofill

# Connect built-in power rings
connect_by_abutment

# Bondpad as separate cell placed in OpenROAD:
# place the bonding pad relative to the IO cell
place_bondpad -bond $bondPadCell -offset {5.0 -70.0} pad_*

# remove rows created by make_io_sites as they are no longer needed
remove_io_rows