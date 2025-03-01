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
// File       : PIO_EP.v
// Version    : 3.0
//
// Description: Endpoint Programmed I/O module.
//	      Consists of Receive and Transmit modules and a Memory Aperture
//
//------------------------------------------------------------------------------

`timescale 1ps/1ps
`ifdef SIMULATION
`include "../rtl/setup.v"
`else
`include "../setup.v"
`endif

(* DowngradeIPIdentifiedWarnings = "yes" *)
module PIO_EP #(
  parameter C_DATA_WIDTH = 64,	    // RX/TX interface data width

  // Do not override parameters below this line
  parameter KEEP_WIDTH = C_DATA_WIDTH / 8,	      // TSTRB width
  parameter TCQ	= 1
) (
  input			 sys_rst,

  input			 clk,
  input			 rst_n,
  input                  user_lnk_up,

  output  [1:0]                 pl_directed_link_change,
  output  [1:0]                 pl_directed_link_width,
  output                        pl_directed_link_speed,
  output                        pl_directed_link_auton,
  input                         pl_directed_change_done,
  input                         pl_sel_lnk_rate,
  input [1:0]                   pl_sel_lnk_width,
  input [5:0]                   pl_ltssm_state,

  // AXIS TX
  input			 s_axis_tx_tready,
  output  [C_DATA_WIDTH-1:0]    s_axis_tx_tdata,
  output  [KEEP_WIDTH-1:0]      s_axis_tx_tkeep,
  output			s_axis_tx_tlast,
  output			s_axis_tx_tvalid,
  output			tx_src_dsc,

  //AXIS RX
  input   [C_DATA_WIDTH-1:0]    m_axis_rx_tdata,
  input   [KEEP_WIDTH-1:0]      m_axis_rx_tkeep,
  input			 m_axis_rx_tlast,
  input			 m_axis_rx_tvalid,
  output			m_axis_rx_tready,
  input   [21:0]		m_axis_rx_tuser,

  output			req_compl,
  output			compl_done,

  input   [15:0]		cfg_completer_id,

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

);

    // Local wires

    wire  [13:0]      rd_addr;
    wire  [3:0]       rd_be;
    wire  [31:0]      rd_data;

    wire  [13:0]      wr_addr;
    wire  [7:0]       wr_be;
    wire  [31:0]      wr_data;
    wire	      wr_en;
    wire	      wr_busy;

    wire	      req_compl_int;
    wire	      req_compl_wd;
    wire	      compl_done_int;

    wire  [2:0]       req_tc;
    wire	      req_td;
    wire	      req_ep;
    wire  [1:0]       req_attr;
    wire  [9:0]       req_len;
    wire  [15:0]      req_rid;
    wire  [7:0]       req_tag;
    wire  [7:0]       req_be;
    wire  [15:0]      req_addr;

    wire              m_axis_rx_tready1;

    assign m_axis_rx_tready = 1'b1; //macchan (m_axis_rx_tready1);

	// PCIe user registers
wire  [2:0] dma_testmode;
wire [31:2] dma_addrl;
wire [15:0] dma_addrh;
wire [31:2] dma_length;
wire [31:0] dma_para;
wire [31:0] dma_tx_pps;
wire [31:0] dma_tx_dw;
wire [31:0] dma_rx_pps;
wire [31:0] dma_rx_dw;

wire soft_reset;

    //
    // ENDPOINT MEMORY : 8KB memory aperture implemented in FPGA BlockRAM(*)
    //
wire [7:0] debug;
wire [31:0] debug1, debug2, debug3, debug4;
    PIO_EP_MEM_ACCESS  #(
       .TCQ( TCQ )
       ) EP_MEM_inst (
      .sys_rst(sys_rst),       // I
      
      .clk(clk),	       // I

      .soft_reset(soft_reset),
      
      // Read Port
      
      .rd_addr(rd_addr),     // I [13:0]
      .rd_be(rd_be),	 // I [3:0]
      .rd_data(rd_data),     // O [31:0]
      
      // Write Port
      
      .wr_addr(wr_addr),     // I [13:0]
      .wr_be(wr_be),	 // I [7:0]
      .wr_data(wr_data),     // I [31:0]
      .wr_en(wr_en),	 // I
      .wr_busy(wr_busy),     // O

	// PCIe user registers
	.dma_testmode(dma_testmode),
	.dma_addrl(dma_addrl),
	.dma_addrh(dma_addrh),
	.dma_length(dma_length),
	.dma_para(dma_para),
	.dma_tx_pps(dma_tx_pps),
	.dma_tx_dw(dma_tx_dw),
	.dma_rx_pps(dma_rx_pps),
	.dma_rx_dw(dma_rx_dw),

        .user_lnk_up(user_lnk_up),
        .cfg_command( cfg_command ),
        .cfg_dcommand( cfg_dcommand ),
        .cfg_lcommand( cfg_lcommand ),
        .cfg_dcommand2( cfg_dcommand2 ),

        .pl_directed_link_auton( pl_directed_link_auton ),
        .pl_directed_link_change( pl_directed_link_change ),
        .pl_directed_link_speed( pl_directed_link_speed ),
        .pl_directed_link_width( pl_directed_link_width ),
        .pl_directed_change_done( pl_directed_change_done ),
        .pl_sel_lnk_rate ( pl_sel_lnk_rate ),
        .pl_sel_lnk_width ( pl_sel_lnk_width ),
        .pl_ltssm_state ( pl_ltssm_state ),


  // BUTTON
  .button_n                       (button_n),
  .button_s                       (button_s),
  .button_w                       (button_w),
  .button_e                       (button_e),
  .button_c                       (button_c),

	.debug1(debug1),
	.debug2(debug2),
	.debug3(debug3),
	.debug4(debug4)
      );

    //
    // Local-Link Receive Controller
    //

  PIO_RX_ENGINE #(
    .C_DATA_WIDTH( C_DATA_WIDTH ),
    .KEEP_WIDTH( KEEP_WIDTH ),
    .TCQ( TCQ )

  ) EP_RX_inst (

    .clk(clk),			      // I
    .rst_n(rst_n),			  // I

    // AXIS RX
    .m_axis_rx_tdata( m_axis_rx_tdata ),    // I
    .m_axis_rx_tkeep( m_axis_rx_tkeep ),    // I
    .m_axis_rx_tlast( m_axis_rx_tlast ),    // I
    .m_axis_rx_tvalid( m_axis_rx_tvalid ),  // I
    .m_axis_rx_tready( m_axis_rx_tready1 ),  // O
    .m_axis_rx_tuser ( m_axis_rx_tuser ),   // I

    // Handshake with Tx engine
    .req_compl(req_compl_int),	      // O
    .req_compl_wd(req_compl_wd),	    // O
    .compl_done(compl_done_int),	    // I

    .req_tc(req_tc),			// O [2:0]
    .req_td(req_td),			// O
    .req_ep(req_ep),			// O
    .req_attr(req_attr),		    // O [1:0]
    .req_len(req_len),		      // O [9:0]
    .req_rid(req_rid),		      // O [15:0]
    .req_tag(req_tag),		      // O [7:0]
    .req_be(req_be),			// O [7:0]
    .req_addr(req_addr),		    // O [15:0]
					    
    // Memory Write Port		    
    .wr_addr(wr_addr),		      // O [13:0]
    .wr_be(wr_be),			  // O [7:0]
    .wr_data(wr_data),		      // O [31:0]
    .wr_en(wr_en),			  // O
    .wr_busy(wr_busy),		      // I

    .dipsw(dipsw),

    .debug(debug)
					    
  );

    //
    // PCIe TX MUX (2 inputs, 1 output)
    //
wire s_axis_tx1_req, s_axis_tx2_req;
wire s_axis_tx1_ack, s_axis_tx2_ack;
wire s_axis_tx1_tready, s_axis_tx2_tready;
wire [C_DATA_WIDTH-1:0] s_axis_tx1_tdata, s_axis_tx2_tdata;
wire [KEEP_WIDTH-1:0] s_axis_tx1_tkeep, s_axis_tx2_tkeep;
wire s_axis_tx1_tlast, s_axis_tx2_tlast;
wire s_axis_tx1_tvalid, s_axis_tx2_tvalid;
wire tx1_src_dsc, tx2_src_dsc;
TX_MUX # (
	.C_DATA_WIDTH( C_DATA_WIDTH ),
	.KEEP_WIDTH( KEEP_WIDTH ),
	.TCQ( TCQ )
) TXMUX_inst (
	.clk(clk),
	.sys_rst(sys_rst),
	// AXIS Output
	.s_axis_tx_tready(s_axis_tx_tready),
	.s_axis_tx_tdata(s_axis_tx_tdata),
	.s_axis_tx_tkeep(s_axis_tx_tkeep),
	.s_axis_tx_tlast(s_axis_tx_tlast),
	.s_axis_tx_tvalid(s_axis_tx_tvalid),
	.tx_src_dsc(tx_src_dsc),
	// AXIS Input 1
	.s_axis_tx1_req(s_axis_tx1_req),
	.s_axis_tx1_ack(s_axis_tx1_ack),
	.s_axis_tx1_tready(s_axis_tx1_tready),
	.s_axis_tx1_tdata(s_axis_tx1_tdata),
	.s_axis_tx1_tkeep(s_axis_tx1_tkeep),
	.s_axis_tx1_tlast(s_axis_tx1_tlast),
	.s_axis_tx1_tvalid(s_axis_tx1_tvalid),
	.tx1_src_dsc(tx1_src_dsc),
	// AXIS Input 2
	.s_axis_tx2_req(s_axis_tx2_req),
	.s_axis_tx2_ack(s_axis_tx2_ack),
	.s_axis_tx2_tready(s_axis_tx2_tready),
	.s_axis_tx2_tdata(s_axis_tx2_tdata),
	.s_axis_tx2_tkeep(s_axis_tx2_tkeep),
	.s_axis_tx2_tlast(s_axis_tx2_tlast),
	.s_axis_tx2_tvalid(s_axis_tx2_tvalid),
	.tx2_src_dsc(tx2_src_dsc)
);

    //
    // Local-Link Transmit Controller
    //

  PIO_TX_ENGINE #(
    .C_DATA_WIDTH( C_DATA_WIDTH ),
    .KEEP_WIDTH( KEEP_WIDTH ),
    .TCQ( TCQ )
  )EP_TX_inst(

    .clk(clk),				  // I
    .rst_n(rst_n),			      // I

    // AXIS Tx
    .s_axis_tx_req(s_axis_tx1_req),
    .s_axis_tx_ack(s_axis_tx1_ack),
    .s_axis_tx_tready( s_axis_tx1_tready ),      // I
    .s_axis_tx_tdata( s_axis_tx1_tdata ),	// O
    .s_axis_tx_tkeep( s_axis_tx1_tkeep ),	// O
    .s_axis_tx_tlast( s_axis_tx1_tlast ),	// O
    .s_axis_tx_tvalid( s_axis_tx1_tvalid ),      // O
    .tx_src_dsc( tx1_src_dsc ),		  // O

    // Handshake with Rx engine
    .req_compl(req_compl_int),		// I
    .req_compl_wd(req_compl_wd),	      // I
    .compl_done(compl_done_int),		// 0

    .req_tc(req_tc),			  // I [2:0]
    .req_td(req_td),			  // I
    .req_ep(req_ep),			  // I
    .req_attr(req_attr),		      // I [1:0]
    .req_len(req_len),			// I [9:0]
    .req_rid(req_rid),			// I [15:0]
    .req_tag(req_tag),			// I [7:0]
    .req_be(req_be),			  // I [7:0]
    .req_addr(req_addr),		      // I [15:0]

    // Read Port

    .rd_addr(rd_addr),			// O [13:0]
    .rd_be(rd_be),			    // O [3:0]
    .rd_data(rd_data),			// I [31:0]

    .completer_id(cfg_completer_id)	   // I [15:0]

    );


PIO_DMATEST # (
	.C_DATA_WIDTH( C_DATA_WIDTH ),
	.KEEP_WIDTH( KEEP_WIDTH ),
	.TCQ( TCQ )
) PIO_DMATEST_inst (
	.clk(clk),
	.sys_rst(sys_rst),
        .soft_reset(soft_reset),

	// AXIS Tx
	.s_axis_other_req(s_axis_tx1_req),
	.s_axis_tx_req(s_axis_tx2_req),
	.s_axis_tx_ack(s_axis_tx2_ack),
	.s_axis_tx_tready( s_axis_tx2_tready ),      // I
	.s_axis_tx_tdata( s_axis_tx2_tdata ),	// O
	.s_axis_tx_tkeep( s_axis_tx2_tkeep ),	// O
	.s_axis_tx_tlast( s_axis_tx2_tlast ),	// O
	.s_axis_tx_tvalid( s_axis_tx2_tvalid ),      // O
	.tx_src_dsc( tx2_src_dsc ),		  // O

	.cfg_completer_id(cfg_completer_id),   // I [15:0]

	// AXIS RX
	.m_axis_rx_tdata( m_axis_rx_tdata ),    // I
	.m_axis_rx_tkeep( m_axis_rx_tkeep ),    // I
	.m_axis_rx_tlast( m_axis_rx_tlast ),    // I
	.m_axis_rx_tvalid( m_axis_rx_tvalid ),  // I
	.m_axis_rx_tuser ( m_axis_rx_tuser ),   // I

        .cfg_command( cfg_command ),
        .cfg_dcommand( cfg_dcommand ),
        .cfg_lcommand( cfg_lcommand ),
        .cfg_dcommand2( cfg_dcommand2 ),

	// PCIe user registers
	.dma_testmode(dma_testmode),
	.dma_addrl(dma_addrl),
	.dma_addrh(dma_addrh),
	.dma_length(dma_length),
	.dma_para(dma_para),
	.dma_tx_pps(dma_tx_pps),
	.dma_tx_dw(dma_tx_dw),
	.dma_rx_pps(dma_rx_pps),
	.dma_rx_dw(dma_rx_dw),

	.debug1(debug1),
	.debug2(debug2),
	.debug3(debug3),
	.debug4(debug4)
);

assign led = debug;

assign req_compl  = req_compl_int;
assign compl_done = compl_done_int;

endmodule // PIO_EP
