library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity DataGenerator is
    generic (
        C_S_AXI_ADDR_WIDTH : integer := 6; -- 64-byte space
        C_S_AXI_DATA_WIDTH : integer := 32;
        C_M_AXIS_TDATA_W   : integer := 32
    );
    port (
        ----------------------------------------------------------------
        -- AXI4-Lite slave interface (to PS GP master)
        ----------------------------------------------------------------
        s_axi_aclk    : in std_logic;
        s_axi_aresetn : in std_logic;

        s_axi_awaddr  : in std_logic_vector(C_S_AXI_ADDR_WIDTH - 1 downto 0);
        s_axi_awvalid : in std_logic;
        s_axi_awready : out std_logic;

        s_axi_wdata  : in std_logic_vector(C_S_AXI_DATA_WIDTH - 1 downto 0);
        s_axi_wstrb  : in std_logic_vector((C_S_AXI_DATA_WIDTH/8) - 1 downto 0);
        s_axi_wvalid : in std_logic;
        s_axi_wready : out std_logic;

        s_axi_bresp  : out std_logic_vector(1 downto 0);
        s_axi_bvalid : out std_logic;
        s_axi_bready : in std_logic;

        s_axi_araddr  : in std_logic_vector(C_S_AXI_ADDR_WIDTH - 1 downto 0);
        s_axi_arvalid : in std_logic;
        s_axi_arready : out std_logic;

        s_axi_rdata  : out std_logic_vector(C_S_AXI_DATA_WIDTH - 1 downto 0);
        s_axi_rresp  : out std_logic_vector(1 downto 0);
        s_axi_rvalid : out std_logic;
        s_axi_rready : in std_logic;

        ----------------------------------------------------------------
        -- AXI4-Stream master interface (to AXI DMA S2MM)
        ----------------------------------------------------------------
        m_axis_aclk    : in std_logic;
        m_axis_aresetn : in std_logic;
        m_axis_tdata   : out std_logic_vector(C_M_AXIS_TDATA_W - 1 downto 0);
        m_axis_tvalid  : out std_logic;
        m_axis_tready  : in std_logic;
        m_axis_tlast   : out std_logic;
        m_axis_tkeep   : out std_logic_vector((C_M_AXIS_TDATA_W/8) - 1 downto 0)
    );
end entity DataGenerator;
architecture rtl of DataGenerator is

    ------------------------------------------------------------------
    -- Address map (word offsets = addr[5:2])
    -- 0x00 CONTROL       [0]=ENABLE [1]=SOFT_RESET (self-clearing pulse)
    -- 0x04 SAMPLE_PERIOD
    -- 0x08 FRAME_SAMPLES
    -- 0x0C FRAME_ID      (R/W current frame counter start)
    -- 0x10 STATUS        [0]=RUNNING [1]=BACKPRESSURE_SEEN [31:16]=BACKPRESSURE_COUNT
    -- 0x14 STATUS_CLR    write bit1=1 to clear backpressure flags/counter
    -- 0x18 VERSION       read-only constant 0x0000DEAD
    ------------------------------------------------------------------

    constant CONTROL_ENABLE_BIT     : integer := 0;
    constant CONTROL_SOFT_RESET_BIT : integer := 1;
    constant STATUS_RUNNING_BIT     : integer := 0;
    constant STATUS_BP_SEEN_BIT     : integer := 1;

    constant BACKPRESSURE_MAX : unsigned(15 downto 0)         := to_unsigned(65535, 16);
    constant VERSION_CONST    : std_logic_vector(31 downto 0) := x"0000DEAD";

    -- AXI-lite registers
    signal reg_control       : std_logic_vector(31 downto 0) := (others => '0');
    signal reg_sample_period : unsigned(31 downto 0)         := (others => '0');
    signal reg_frame_samples : unsigned(31 downto 0)         := to_unsigned(256, 32);
    signal reg_frame_id_rw   : unsigned(31 downto 0)         := (others => '0');

    -- status signals
    signal status_running           : std_logic             := '0';
    signal status_backpressure_seen : std_logic             := '0';
    signal status_backpressure_cnt  : unsigned(15 downto 0) := (others => '0');
    signal status_word              : std_logic_vector(31 downto 0);

    -- control pulses from writes
    signal soft_reset_req   : std_logic := '0';
    signal clr_backpressure : std_logic := '0';

    ------------------------------------------------------------------
    -- AXI-Lite slave state
    ------------------------------------------------------------------
    -- Write address/data handshake
    signal awready_i : std_logic                    := '0';
    signal wready_i  : std_logic                    := '0';
    signal bvalid_i  : std_logic                    := '0';
    signal bresp_i   : std_logic_vector(1 downto 0) := "00";

    signal arready_i : std_logic                     := '0';
    signal rvalid_i  : std_logic                     := '0';
    signal rresp_i   : std_logic_vector(1 downto 0)  := "00";
    signal rdata_i   : std_logic_vector(31 downto 0) := (others => '0');

    -- Latching write address/data when both valid
    signal awaddr_lat : std_logic_vector(C_S_AXI_ADDR_WIDTH - 1 downto 0) := (others => '0');
    signal wdata_lat  : std_logic_vector(31 downto 0)                     := (others => '0');
    signal wstrb_lat  : std_logic_vector(3 downto 0)                      := (others => '0');
    signal write_fire : std_logic                                         := '0';

    -- Latching read address
    signal araddr_lat : std_logic_vector(C_S_AXI_ADDR_WIDTH - 1 downto 0) := (others => '0');
    signal read_fire  : std_logic                                         := '0';

    ------------------------------------------------------------------
    -- Stream generator state
    ------------------------------------------------------------------
    signal running_i  : std_logic                     := '0';
    signal frame_id   : unsigned(31 downto 0)         := (others => '0');
    signal sample_idx : unsigned(31 downto 0)         := (others => '0');
    signal tvalid_i   : std_logic                     := '0';
    signal tlast_i    : std_logic                     := '0';
    signal tdata_i    : std_logic_vector(31 downto 0) := (others => '0');

    -- rate control
    signal div_cnt   : unsigned(31 downto 0) := (others => '0');
    signal sample_ce : std_logic             := '0';

begin

    -----------------------------------------------------------------------------
    -- AXI-Lite WRITE channel
    -----------------------------------------------------------------------------
    axi_write_proc : process (s_axi_aclk)
    begin
        if rising_edge(s_axi_aclk) then
            if s_axi_aresetn = '0' then
                awready_i  <= '0';
                wready_i   <= '0';
                bvalid_i   <= '0';
                bresp_i    <= "00";
                awaddr_lat <= (others => '0');
                wdata_lat  <= (others => '0');
                wstrb_lat  <= (others => '0');
                write_fire <= '0';

            else
                -- defaults
                write_fire <= '0';
                awready_i  <= '0';
                wready_i   <= '0';

                if bvalid_i = '0' then
                    -- accept a write only when address and data are valid together
                    if (s_axi_awvalid = '1') and (s_axi_wvalid = '1') then
                        awready_i  <= '1';
                        wready_i   <= '1';
                        awaddr_lat <= s_axi_awaddr;
                        wdata_lat  <= s_axi_wdata;
                        wstrb_lat  <= s_axi_wstrb;
                        write_fire <= '1';
                        bvalid_i   <= '1';
                        bresp_i    <= "00"; -- OKAY
                    end if;

                else
                    -- wait for BRESP handshake before accepting next write
                    if s_axi_bready = '1' then
                        bvalid_i <= '0';
                    end if;
                end if;
            end if;
        end if;
    end process;

    -----------------------------------------------------------------------------
    -- AXI-Lite READ channel
    -----------------------------------------------------------------------------
    axi_read_proc : process (s_axi_aclk)
    begin
        if rising_edge(s_axi_aclk) then
            if s_axi_aresetn = '0' then
                arready_i  <= '0';
                rvalid_i   <= '0';
                rresp_i    <= "00";
                araddr_lat <= (others => '0');
                read_fire  <= '0';

            else
                read_fire <= '0';

                if rvalid_i = '0' then
                    -- we can accept a new read address
                    arready_i <= '1';

                    if s_axi_arvalid = '1' then
                        arready_i  <= '0';
                        araddr_lat <= s_axi_araddr;
                        read_fire  <= '1';

                        -- we'll produce data immediately this cycle
                        rvalid_i <= '1';
                        rresp_i  <= "00"; -- OKAY
                    end if;

                else
                    -- we are holding RVALID high waiting for RREADY
                    if s_axi_rready = '1' then
                        rvalid_i <= '0';
                    end if;
                end if;
            end if;
        end if;
    end process;

    -----------------------------------------------------------------------------
    -- Register write side-effects (CONTROL, SAMPLE_PERIOD, etc.)
    -----------------------------------------------------------------------------
    reg_write_proc : process (s_axi_aclk)
        variable word_sel : std_logic_vector(3 downto 0);
    begin
        if rising_edge(s_axi_aclk) then
            if s_axi_aresetn = '0' then
                reg_control       <= (others => '0');
                reg_sample_period <= (others => '0');
                reg_frame_samples <= to_unsigned(256, 32);
                reg_frame_id_rw   <= (others => '0');

                soft_reset_req   <= '0';
                clr_backpressure <= '0';

            else
                -- default pulses low each cycle
                soft_reset_req   <= '0';
                clr_backpressure <= '0';

                if write_fire = '1' then
                    -- byte enables (we only do full-word writes, but keep this var)
                    word_sel := wstrb_lat;

                    case awaddr_lat(5 downto 2) is
                        when "0000" => -- CONTROL
                            reg_control <= wdata_lat;
                            if wdata_lat(CONTROL_SOFT_RESET_BIT) = '1' then
                                soft_reset_req <= '1';
                            end if;

                        when "0001" => -- SAMPLE_PERIOD
                            reg_sample_period <= unsigned(wdata_lat);

                        when "0010" => -- FRAME_SAMPLES
                            reg_frame_samples <= unsigned(wdata_lat);

                        when "0011" => -- FRAME_ID preload
                            reg_frame_id_rw <= unsigned(wdata_lat);

                        when "0101" => -- STATUS_CLR
                            if wdata_lat(1) = '1' then
                                clr_backpressure <= '1';
                            end if;

                        when others =>
                            null;
                    end case;
                end if;
            end if;
        end if;
    end process;

    -----------------------------------------------------------------------------
    -- running_i is ENABLE unless soft_reset_req is active
    -----------------------------------------------------------------------------
    run_ctl_proc : process (s_axi_aclk)
    begin
        if rising_edge(s_axi_aclk) then
            if s_axi_aresetn = '0' then
                running_i <= '0';
            else
                if soft_reset_req = '1' then
                    running_i <= '0';
                else
                    running_i <= reg_control(CONTROL_ENABLE_BIT);
                end if;
            end if;
        end if;
    end process;

    status_running <= running_i;

    -----------------------------------------------------------------------------
    -- Build STATUS word for reads
    -----------------------------------------------------------------------------
    status_word_proc : process (status_running,
        status_backpressure_seen,
        status_backpressure_cnt)
    begin
        status_word <= (others => '0');

        status_word(STATUS_RUNNING_BIT) <= status_running;
        status_word(STATUS_BP_SEEN_BIT) <= status_backpressure_seen;
        status_word(31 downto 16)       <= std_logic_vector(status_backpressure_cnt);
    end process;

    -----------------------------------------------------------------------------
    -- Register read mux (combinational)
    -----------------------------------------------------------------------------
    reg_read_mux_proc : process (araddr_lat,
        reg_control,
        reg_sample_period,
        reg_frame_samples,
        frame_id,
        status_word)
        variable vdata : std_logic_vector(31 downto 0);
    begin
        case araddr_lat(5 downto 2) is
            when "0000" => -- 0x00 CONTROL
                vdata := reg_control;

            when "0001" => -- 0x04 SAMPLE_PERIOD
                vdata := std_logic_vector(reg_sample_period);

            when "0010" => -- 0x08 FRAME_SAMPLES
                vdata := std_logic_vector(reg_frame_samples);

            when "0011" => -- 0x0C FRAME_ID (live)
                vdata := std_logic_vector(frame_id);

            when "0100" => -- 0x10 STATUS
                vdata := status_word;

            when "0101"      => -- 0x14 STATUS_CLR (reads as zero in previous rev)
                vdata := (others => '0');

            when "0110" => -- 0x18 VERSION
                vdata := VERSION_CONST;

            when others      =>
                vdata := (others => '0');
        end case;

        rdata_i <= vdata;
    end process;

    -----------------------------------------------------------------------------
    -- Data generator + backpressure monitor (AXI-Stream source)
    -- Assumes m_axis_aclk == s_axi_aclk in your design.
    -----------------------------------------------------------------------------
    stream_proc : process (m_axis_aclk)
    begin
        if rising_edge(m_axis_aclk) then
            if (m_axis_aresetn = '0') or (soft_reset_req = '1') then
                div_cnt                  <= (others => '0');
                sample_ce                <= '0';
                frame_id                 <= (others => '0');
                sample_idx               <= (others => '0');
                tvalid_i                 <= '0';
                tlast_i                  <= '0';
                status_backpressure_seen <= '0';
                status_backpressure_cnt  <= (others => '0');

            elsif clr_backpressure = '1' then
                status_backpressure_seen <= '0';
                status_backpressure_cnt  <= (others => '0');

            else
                -- Rate divider
                if running_i = '1' then
                    if div_cnt = reg_sample_period then
                        div_cnt   <= (others => '0');
                        sample_ce <= '1';
                    else
                        div_cnt   <= div_cnt + 1;
                        sample_ce <= '0';
                    end if;
                else
                    div_cnt   <= (others => '0');
                    sample_ce <= '0';
                end if;

                -- Backpressure monitor
                if (tvalid_i = '1') and (m_axis_tready = '0') then
                    status_backpressure_seen <= '1';
                    if status_backpressure_cnt /= BACKPRESSURE_MAX then
                        status_backpressure_cnt <= status_backpressure_cnt + 1;
                    end if;
                end if;

                -- Streaming FSM
                -- Drop TVALID/TLAST only after the current beat is accepted
                if (tvalid_i = '1') and (m_axis_tready = '1') then
                    tvalid_i <= '0';
                    tlast_i  <= '0';
                end if;

                if running_i = '1' then
                    if sample_ce = '1' then
                        if (tvalid_i = '0') or ((tvalid_i = '1') and (m_axis_tready = '1')) then
                            -- first word of frame is frame_id, rest is incrementing sample index-1
                            if sample_idx = to_unsigned(0, 32) then
                                tdata_i <= std_logic_vector(frame_id);
                            else
                                tdata_i <= std_logic_vector(sample_idx - 1);
                            end if;

                            if sample_idx = (reg_frame_samples - 1) then
                                tlast_i    <= '1'; -- hold high until handshake
                                sample_idx <= (others => '0');
                                frame_id   <= frame_id + 1;
                            else
                                tlast_i    <= '0';
                                sample_idx <= sample_idx + 1;
                            end if;

                            tvalid_i <= '1';
                        end if;
                    end if;

                else
                    -- idle state when disabled
                    tvalid_i   <= '0';
                    tlast_i    <= '0';
                    sample_idx <= (others => '0');
                    frame_id   <= reg_frame_id_rw;
                end if;
            end if;
        end if;
    end process;

    -----------------------------------------------------------------------------
    -- Drive AXI-Stream outputs
    -----------------------------------------------------------------------------
    m_axis_tdata  <= tdata_i;
    m_axis_tvalid <= tvalid_i;
    m_axis_tlast  <= tlast_i;
    m_axis_tkeep  <= (others => '1'); -- 0xF for 32-bit interface

    -----------------------------------------------------------------------------
    -- Drive AXI-Lite outputs
    -----------------------------------------------------------------------------
    s_axi_awready <= awready_i;
    s_axi_wready  <= wready_i;
    s_axi_bvalid  <= bvalid_i;
    s_axi_bresp   <= bresp_i;

    s_axi_arready <= arready_i;
    s_axi_rvalid  <= rvalid_i;
    s_axi_rresp   <= rresp_i;
    s_axi_rdata   <= rdata_i;

end architecture rtl;
