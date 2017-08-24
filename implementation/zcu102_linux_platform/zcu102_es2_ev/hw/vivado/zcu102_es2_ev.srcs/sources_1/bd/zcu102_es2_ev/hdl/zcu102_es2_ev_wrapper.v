//Copyright 1986-2016 Xilinx, Inc. All Rights Reserved.
//--------------------------------------------------------------------------------
//Tool Version: Vivado v.2016.3_sdx (lin64) Build 1721784 Tue Nov 29 22:12:24 MST 2016
//Date        : Wed Dec 14 23:39:05 2016
//Host        : xsjl23957 running 64-bit Red Hat Enterprise Linux Workstation release 6.6 (Santiago)
//Command     : generate_target zcu102_es2_ev_wrapper.bd
//Design      : zcu102_es2_ev_wrapper
//Purpose     : IP block netlist
//--------------------------------------------------------------------------------
`timescale 1 ps / 1 ps

module zcu102_es2_ev_wrapper
   (fmc_imageon_hdmii_clk,
    fmc_imageon_hdmii_data,
    fmc_imageon_hdmii_spdif,
    fmc_imageon_iic_rst_b,
    fmc_imageon_iic_scl_io,
    fmc_imageon_iic_sda_io,
    si570_clk_n,
    si570_clk_p);
  input fmc_imageon_hdmii_clk;
  input [15:0]fmc_imageon_hdmii_data;
  input fmc_imageon_hdmii_spdif;
  output [0:0]fmc_imageon_iic_rst_b;
  inout fmc_imageon_iic_scl_io;
  inout fmc_imageon_iic_sda_io;
  input [0:0]si570_clk_n;
  input [0:0]si570_clk_p;

  wire fmc_imageon_hdmii_clk;
  wire [15:0]fmc_imageon_hdmii_data;
  wire fmc_imageon_hdmii_spdif;
  wire [0:0]fmc_imageon_iic_rst_b;
  wire fmc_imageon_iic_scl_i;
  wire fmc_imageon_iic_scl_io;
  wire fmc_imageon_iic_scl_o;
  wire fmc_imageon_iic_scl_t;
  wire fmc_imageon_iic_sda_i;
  wire fmc_imageon_iic_sda_io;
  wire fmc_imageon_iic_sda_o;
  wire fmc_imageon_iic_sda_t;
  wire [0:0]si570_clk_n;
  wire [0:0]si570_clk_p;

  IOBUF fmc_imageon_iic_scl_iobuf
       (.I(fmc_imageon_iic_scl_o),
        .IO(fmc_imageon_iic_scl_io),
        .O(fmc_imageon_iic_scl_i),
        .T(fmc_imageon_iic_scl_t));
  IOBUF fmc_imageon_iic_sda_iobuf
       (.I(fmc_imageon_iic_sda_o),
        .IO(fmc_imageon_iic_sda_io),
        .O(fmc_imageon_iic_sda_i),
        .T(fmc_imageon_iic_sda_t));
  zcu102_es2_ev zcu102_es2_ev_i
       (.fmc_imageon_hdmii_clk(fmc_imageon_hdmii_clk),
        .fmc_imageon_hdmii_data(fmc_imageon_hdmii_data),
        .fmc_imageon_hdmii_spdif(fmc_imageon_hdmii_spdif),
        .fmc_imageon_iic_rst_b(fmc_imageon_iic_rst_b),
        .fmc_imageon_iic_scl_i(fmc_imageon_iic_scl_i),
        .fmc_imageon_iic_scl_o(fmc_imageon_iic_scl_o),
        .fmc_imageon_iic_scl_t(fmc_imageon_iic_scl_t),
        .fmc_imageon_iic_sda_i(fmc_imageon_iic_sda_i),
        .fmc_imageon_iic_sda_o(fmc_imageon_iic_sda_o),
        .fmc_imageon_iic_sda_t(fmc_imageon_iic_sda_t),
        .si570_clk_n(si570_clk_n),
        .si570_clk_p(si570_clk_p));
endmodule
