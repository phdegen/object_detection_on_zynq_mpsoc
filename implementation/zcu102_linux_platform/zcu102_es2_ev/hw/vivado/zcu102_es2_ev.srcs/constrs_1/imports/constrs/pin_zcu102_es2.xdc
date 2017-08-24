###################
# Pin Constraints #
###################
#
# Video Clock SI570
#
# PL Port      Pin  Schematic
#
# si570_clk_n  L28  USER_MGT_SI570_N
# si570_clk_p  L27  USER_MGT_SI570_P
#
set_property PACKAGE_PIN L28 [get_ports si570_clk_n]
#
# IIC - FMC-IMAGEON/FMC-HDMI-CAM
#
# PL Port                 FMC  Schematic  HPC0
#
# fmc_imageon_iic_scl_io  G18  LA16_P     Y12
# fmc_imageon_iic_sda_io  G19  LA16_N     AA12
# fmc_imageon_iic_rst_b   D9   LA01_N_CC  AC4
#
set_property PACKAGE_PIN Y12 [get_ports fmc_imageon_iic_scl_io]
set_property PACKAGE_PIN AA12 [get_ports fmc_imageon_iic_sda_io]
set_property PACKAGE_PIN AC4 [get_ports fmc_imageon_iic_rst_b]
set_property IOSTANDARD LVCMOS18 [get_ports fmc_imageon_iic_*]
#
# HDMI In (ADV7611) - FMC-IMAGEON/FMC-HDMI-CAM
#
# PL Port                              FMC  Schematic   HPC0
#
# fmc_imageon_hdmii_clk                G2   CLK1_M2C_P  T8
# fmc_imageon_hdmii_spdif              H29  LA24_N      K12
# fmc_imageon_hdmii_data[0]   CBCR[0]  H38  LA32_N      T11
# fmc_imageon_hdmii_data[1]   CBCR[1]  H37  LA32_P      U11
# fmc_imageon_hdmii_data[2]   CBCR[2]  G37  LA33_N      V11
# fmc_imageon_hdmii_data[3]   CBCR[3]  G36  LA33_P      V12
# fmc_imageon_hdmii_data[4]   CBCR[4]  H35  LA30_N      U6
# fmc_imageon_hdmii_data[5]   CBCR[5]  H34  LA30_P      V6
# fmc_imageon_hdmii_data[6]   CBCR[6]  G34  LA31_N      V7
# fmc_imageon_hdmii_data[7]   CBCR[7]  G33  LA31_P      V8
# fmc_imageon_hdmii_data[8]   Y[0]     H32  LA28_N      T6
# fmc_imageon_hdmii_data[9]   Y[1]     H31  LA28_P      T7
# fmc_imageon_hdmii_data[10]  Y[2]     G31  LA29_N      U8
# fmc_imageon_hdmii_data[11]  Y[3]     C27  LA27_N      L10
# fmc_imageon_hdmii_data[12]  Y[4]     D27  LA26_N      K15
# fmc_imageon_hdmii_data[13]  Y[5]     G30  LA29_P      U9
# fmc_imageon_hdmii_data[14]  Y[6]     C26  LA27_P      M10
# fmc_imageon_hdmii_data[15]  Y[7]     D26  LA26_P      L15
#
set_property PACKAGE_PIN  T8   [get_ports fmc_imageon_hdmii_clk]
set_property PACKAGE_PIN  K12  [get_ports fmc_imageon_hdmii_spdif]
set_property PACKAGE_PIN  T11  [get_ports {fmc_imageon_hdmii_data[0]}]
set_property PACKAGE_PIN  U11  [get_ports {fmc_imageon_hdmii_data[1]}]
set_property PACKAGE_PIN  V11  [get_ports {fmc_imageon_hdmii_data[2]}]
set_property PACKAGE_PIN  V12  [get_ports {fmc_imageon_hdmii_data[3]}]
set_property PACKAGE_PIN  U6   [get_ports {fmc_imageon_hdmii_data[4]}]
set_property PACKAGE_PIN  V6   [get_ports {fmc_imageon_hdmii_data[5]}]
set_property PACKAGE_PIN  V7   [get_ports {fmc_imageon_hdmii_data[6]}]
set_property PACKAGE_PIN  V8   [get_ports {fmc_imageon_hdmii_data[7]}]
set_property PACKAGE_PIN  T6   [get_ports {fmc_imageon_hdmii_data[8]}]
set_property PACKAGE_PIN  T7   [get_ports {fmc_imageon_hdmii_data[9]}]
set_property PACKAGE_PIN  U8   [get_ports {fmc_imageon_hdmii_data[10]}]
set_property PACKAGE_PIN  L10  [get_ports {fmc_imageon_hdmii_data[11]}]
set_property PACKAGE_PIN  K15  [get_ports {fmc_imageon_hdmii_data[12]}]
set_property PACKAGE_PIN  U9   [get_ports {fmc_imageon_hdmii_data[13]}]
set_property PACKAGE_PIN  M10  [get_ports {fmc_imageon_hdmii_data[14]}]
set_property PACKAGE_PIN  L15  [get_ports {fmc_imageon_hdmii_data[15]}]
set_property IOSTANDARD LVCMOS18 [get_ports fmc_imageon_hdmii_*]
