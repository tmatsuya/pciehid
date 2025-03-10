//-----------------------------------------------------------------------------
//
// (c) Copyright 2010-2011 Xilinx, Inc. All rights reserved.
//
// This file contains confidential and proprietary information
// of Xilinx, Inc. and is protected under U.S. and
// international copyright and other intellectual property
// laws.
//
// DISCLAIMER
// This disclaimer is not a license and does not grant any
// rights to the materials distributed herewith. Except as
// otherwise provided in a valid license issued to you by
// Xilinx, and to the maximum extent permitted by applicable
// law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND
// WITH ALL FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES
// AND CONDITIONS, EXPRESS, IMPLIED, OR STATUTORY, INCLUDING
// BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY, NON-
// INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE; and
// (2) Xilinx shall not be liable (whether in contract or tort,
// including negligence, or under any other theory of
// liability) for any loss or damage of any kind or nature
// related to, arising under or in connection with these
// materials, including for any direct, or any indirect,
// special, incidental, or consequential loss or damage
// (including loss of data, profits, goodwill, or any type of
// loss or damage suffered as a result of any action brought
// by a third party) even if such damage or loss was
// reasonably foreseeable or Xilinx had been advised of the
// possibility of the same.
//
// CRITICAL APPLICATIONS
// Xilinx products are not designed or intended to be fail-
// safe, or for use in any application requiring fail-safe
// performance, such as life-support or safety devices or
// systems, Class III medical devices, nuclear facilities,
// applications related to the deployment of airbags, or any
// other applications that could lead to death, personal
// injury, or severe property or environmental damage
// (individually and collectively, "Critical
// Applications"). Customer assumes the sole risk and
// liability of any use of Xilinx products in Critical
// Applications, subject only to applicable laws and
// regulations governing limitations on product liability.
//
// THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS
// PART OF THIS FILE AT ALL TIMES.
//
//-----------------------------------------------------------------------------
// Project    : Series-7 Integrated Block for PCI Express
// File       : PIO.v
// Version    : 3.0
//
// Description:  Programmed I/O module. Design implements 8 KBytes of programmable
//--              memory space. Host processor can access this memory space using
//--              Memory Read 32 and Memory Write 32 TLPs. Design accepts
//--              1 Double Word (DW) payload length on Memory Write 32 TLP and
//--              responds to 1 DW length Memory Read 32 TLPs with a Completion
//--              with Data TLP (1DW payload).
//--
//--------------------------------------------------------------------------------

`timescale 1ps/1ps

(* DowngradeIPIdentifiedWarnings = "yes" *)
module PIO #(
  parameter C_DATA_WIDTH = 64,            // RX/TX interface data width

  // Do not override parameters below this line
  parameter KEEP_WIDTH = C_DATA_WIDTH / 8,              // TSTRB width
  parameter TCQ        = 1
)(
  input                         sys_rst,

  input                         user_clk,
  input                         user_reset,
  input                         user_lnk_up,

  output  [1:0]                 pl_directed_link_change,
  output  [1:0]                 pl_directed_link_width,
  output                        pl_directed_link_speed,
  output                        pl_directed_link_auton,
  input                         pl_directed_change_done,
  input                         pl_sel_lnk_rate,
  input [1:0]                   pl_sel_lnk_width,
  input [5:0]                   pl_ltssm_state,

  // AXIS
  input                         s_axis_tx_tready,
  output  [C_DATA_WIDTH-1:0]    s_axis_tx_tdata,
  output  [KEEP_WIDTH-1:0]      s_axis_tx_tkeep,
  output                        s_axis_tx_tlast,
  output                        s_axis_tx_tvalid,
  output                        tx_src_dsc,


  input  [C_DATA_WIDTH-1:0]     m_axis_rx_tdata,
  input  [KEEP_WIDTH-1:0]       m_axis_rx_tkeep,
  input                         m_axis_rx_tlast,
  input                         m_axis_rx_tvalid,
  output                        m_axis_rx_tready,
  input    [21:0]               m_axis_rx_tuser,


  input                         cfg_to_turnoff,
  output                        cfg_turnoff_ok,

  input [15:0]                  cfg_completer_id,

  input [15:0]                  cfg_command,
  input [15:0]                  cfg_dcommand,
  input [15:0]                  cfg_lcommand,
  input [15:0]                  cfg_dcommand2,


        // BUTTON
        input button_n,
        input button_s,
        input button_w,
        input button_e,
        input button_c,

	input [3:0] dipsw,
	output [7:0] led

); // synthesis syn_hier = "hard"


  // Local wires

  wire          req_compl;
  wire          compl_done;
  reg           pio_reset_n;

  always @(posedge user_clk) begin
    if (user_reset)
        pio_reset_n <= #TCQ 1'b0;
    else
        pio_reset_n <= #TCQ user_lnk_up;
  end

  //
  // PIO instance
  //

  PIO_EP  #(
    .C_DATA_WIDTH( C_DATA_WIDTH ),
    .KEEP_WIDTH( KEEP_WIDTH ),
    .TCQ( TCQ )
  ) PIO_EP_inst (
    .sys_rst( sys_rst ),

    .clk( user_clk ),                             // I
    .rst_n( pio_reset_n ),                        // I
    .user_lnk_up( user_lnk_up ),

    .pl_directed_link_change( pl_directed_link_change ),
    .pl_directed_link_width( pl_directed_link_width ),
    .pl_directed_link_speed( pl_directed_link_speed ),
    .pl_directed_link_auton( pl_directed_link_auton ),
    .pl_directed_change_done ( pl_directed_change_done ),
    .pl_sel_lnk_rate ( pl_sel_lnk_rate ),
    .pl_sel_lnk_width ( pl_sel_lnk_width ),
    .pl_ltssm_state ( pl_ltssm_state ),

    .s_axis_tx_tready( s_axis_tx_tready ),        // I
    .s_axis_tx_tdata( s_axis_tx_tdata ),          // O
    .s_axis_tx_tkeep( s_axis_tx_tkeep ),          // O
    .s_axis_tx_tlast( s_axis_tx_tlast ),          // O
    .s_axis_tx_tvalid( s_axis_tx_tvalid ),        // O
    .tx_src_dsc( tx_src_dsc ),                    // O

    .m_axis_rx_tdata( m_axis_rx_tdata ),          // I
    .m_axis_rx_tkeep( m_axis_rx_tkeep ),          // I
    .m_axis_rx_tlast( m_axis_rx_tlast ),          // I
    .m_axis_rx_tvalid( m_axis_rx_tvalid ),        // I
    .m_axis_rx_tready( m_axis_rx_tready ),        // O
    .m_axis_rx_tuser ( m_axis_rx_tuser ),         // I

    .req_compl(req_compl),                        // O
    .compl_done(compl_done),                      // O

    .cfg_completer_id ( cfg_completer_id ),        // I [15:0]

    .cfg_command( cfg_command ),
    .cfg_dcommand( cfg_dcommand ),
    .cfg_lcommand( cfg_lcommand ),
    .cfg_dcommand2( cfg_dcommand2 ),

  // BUTTON
  .button_n                       (button_n),
  .button_s                       (button_s),
  .button_w                       (button_w),
  .button_e                       (button_e),
  .button_c                       (button_c),

	.dipsw(dipsw),
	.led(led)
  );


  //
  // Turn-Off controller
  //

  PIO_TO_CTRL #(
    .TCQ( TCQ )
  ) PIO_TO_inst  (
    .clk( user_clk ),                       // I
    .rst_n( pio_reset_n ),                  // I

    .req_compl( req_compl ),                // I
    .compl_done( compl_done ),              // I

    .cfg_to_turnoff( cfg_to_turnoff ),      // I
    .cfg_turnoff_ok( cfg_turnoff_ok )       // O
  );


endmodule // PIO

