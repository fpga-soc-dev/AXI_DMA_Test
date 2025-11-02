create_debug_core u_ila_0 ila
set_property ALL_PROBE_SAME_MU true [get_debug_cores u_ila_0]
set_property ALL_PROBE_SAME_MU_CNT 1 [get_debug_cores u_ila_0]
set_property C_ADV_TRIGGER false [get_debug_cores u_ila_0]
set_property C_DATA_DEPTH 131072 [get_debug_cores u_ila_0]
set_property C_EN_STRG_QUAL false [get_debug_cores u_ila_0]
set_property C_INPUT_PIPE_STAGES 0 [get_debug_cores u_ila_0]
set_property C_TRIGIN_EN false [get_debug_cores u_ila_0]
set_property C_TRIGOUT_EN false [get_debug_cores u_ila_0]
set_property port_width 1 [get_debug_ports u_ila_0/clk]
connect_debug_port u_ila_0/clk [get_nets [list pynq_z1_i/ps7_0/inst/FCLK_CLK1]]
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe0]
set_property port_width 32 [get_debug_ports u_ila_0/probe0]
connect_debug_port u_ila_0/probe0 [get_nets [list {pynq_z1_i/DataGenerator_0_m_axis_TDATA[0]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[1]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[2]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[3]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[4]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[5]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[6]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[7]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[8]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[9]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[10]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[11]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[12]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[13]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[14]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[15]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[16]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[17]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[18]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[19]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[20]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[21]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[22]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[23]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[24]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[25]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[26]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[27]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[28]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[29]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[30]} {pynq_z1_i/DataGenerator_0_m_axis_TDATA[31]}]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe1]
set_property port_width 1 [get_debug_ports u_ila_0/probe1]
connect_debug_port u_ila_0/probe1 [get_nets [list {pynq_z1_i/DataGenerator_0_m_axis_TKEEP[0]}]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe2]
set_property port_width 1 [get_debug_ports u_ila_0/probe2]
connect_debug_port u_ila_0/probe2 [get_nets [list pynq_z1_i/axi_dma_0_s2mm_introut]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe3]
set_property port_width 1 [get_debug_ports u_ila_0/probe3]
connect_debug_port u_ila_0/probe3 [get_nets [list pynq_z1_i/DataGenerator_0_m_axis_TLAST]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe4]
set_property port_width 1 [get_debug_ports u_ila_0/probe4]
connect_debug_port u_ila_0/probe4 [get_nets [list pynq_z1_i/DataGenerator_0_m_axis_TREADY]]
create_debug_port u_ila_0 probe
set_property PROBE_TYPE DATA_AND_TRIGGER [get_debug_ports u_ila_0/probe5]
set_property port_width 1 [get_debug_ports u_ila_0/probe5]
connect_debug_port u_ila_0/probe5 [get_nets [list pynq_z1_i/DataGenerator_0_m_axis_TVALID]]
set_property C_CLK_INPUT_FREQ_HZ 300000000 [get_debug_cores dbg_hub]
set_property C_ENABLE_CLK_DIVIDER false [get_debug_cores dbg_hub]
set_property C_USER_SCAN_CHAIN 1 [get_debug_cores dbg_hub]
connect_debug_port dbg_hub/clk [get_nets u_ila_0_FCLK_CLK1]

