# pacman_fwcheck, bare metal:  load the bitstream and run fwcheck over JTAG (from the fwcheck workspace).
setws .
connect
targets -set -nocase -filter {name =~ "ARM*#0"}
rst -system
# the board boots from SD after the reset, and its FSBL configures the PL:  let it finish, then stop the CPU
after 4000
stop
fpga pacman/hw/pacman.bit
source pacman/hw/ps7_init.tcl
ps7_init
ps7_post_config
dow fwcheck/Debug/fwcheck.elf
con
