##################
# Primary Clocks #
##################

# Differential input clock from SI570 clock synthesizer
# Constrained to 297MHz (2160p30 video resolution)
create_clock -period 3.367 -name si570_clk [get_ports si570_clk_p]

# Single-ended input clock from HDMI input
# Constrained to 148.5MHz (1080p60 video resolution)
create_clock -period 6.734 -name hdmii_clk [get_ports fmc_imageon_hdmii_clk]

################
# Clock Groups #
################

# There is no defined phase relationship, hence they are treated as asynchronous
set_clock_groups -asynchronous -group [get_clocks -of [get_pins */clk_wiz_1/inst/mmcme3_adv_inst/CLKOUT0]] \
                               -group [get_clocks -of [get_pins */clk_wiz_1/inst/mmcme3_adv_inst/CLKOUT3]] \
                               -group si570_clk \
                               -group hdmii_clk
