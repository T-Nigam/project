#include "dvi.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "pico/stdlib.h"

// TMDS constants these are fixed values defined by the DVI specification
#define TMDS_CTRL_00 0x354u
#define TMDS_CTRL_01 0x0abu
#define TMDS_CTRL_10 0x154u
#define TMDS_CTRL_11 0x2abu

// Pack three TMDS control symbols (one per color channel) into one 32-bit word. Blue channel carries sync signals.
#define SYNC_V0_H0 (TMDS_CTRL_00 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V0_H1 (TMDS_CTRL_01 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H0 (TMDS_CTRL_10 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))
#define SYNC_V1_H1 (TMDS_CTRL_11 | (TMDS_CTRL_00 << 10) | (TMDS_CTRL_00 << 20))

// HSTX command word types (top 4 bits of each command word)
#define HSTX_CMD_RAW (0x0u << 12) 
#define HSTX_CMD_RAW_REPEAT (0x1u << 12)
#define HSTX_CMD_TMDS (0x2u << 12)  
#define HSTX_CMD_NOP (0xfu << 12) 

// DMA channel numbers
#define DMACH_PING 0
#define DMACH_PONG 1

static uint32_t vblank_line_vsync_off[] = {
    HSTX_CMD_RAW_REPEAT | DVI_H_FRONT_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_RAW_REPEAT | DVI_H_SYNC_WIDTH,
    SYNC_V1_H0,
    HSTX_CMD_RAW_REPEAT | (DVI_H_BACK_PORCH + DVI_H_ACTIVE_PIXELS),
    SYNC_V1_H1,
    HSTX_CMD_NOP
};

static uint32_t vblank_line_vsync_on[] = {
    HSTX_CMD_RAW_REPEAT | DVI_H_FRONT_PORCH,
    SYNC_V0_H1,
    HSTX_CMD_RAW_REPEAT | DVI_H_SYNC_WIDTH,
    SYNC_V0_H0,
    HSTX_CMD_RAW_REPEAT | (DVI_H_BACK_PORCH + DVI_H_ACTIVE_PIXELS),
    SYNC_V0_H1,
    HSTX_CMD_NOP
};

static uint32_t vactive_line[] = {
    HSTX_CMD_RAW_REPEAT | DVI_H_FRONT_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | DVI_H_SYNC_WIDTH,
    SYNC_V1_H0,
    HSTX_CMD_NOP,
    HSTX_CMD_RAW_REPEAT | DVI_H_BACK_PORCH,
    SYNC_V1_H1,
    HSTX_CMD_TMDS | DVI_H_ACTIVE_PIXELS
};


static volatile uint8_t *frame_display_ptr = NULL; 
static volatile bool dma_pong = false; 
static volatile uint v_scanline = 2;
static volatile bool vactive_cmdlist_posted = false; 
static volatile bool vsync_flag = false; 


static void __scratch_x("dvi") dma_irq_handler(void) {
    uint ch_num = dma_pong ? DMACH_PONG : DMACH_PING;
    dma_channel_hw_t *ch = &dma_hw->ch[ch_num];

    dma_hw->intr = 1u << ch_num;

    dma_pong = !dma_pong;

    if (v_scanline >= DVI_V_FRONT_PORCH &&
        v_scanline <  DVI_V_FRONT_PORCH + DVI_V_SYNC_WIDTH) {
        ch->read_addr = (uintptr_t)vblank_line_vsync_on;
        ch->transfer_count = count_of(vblank_line_vsync_on);

    } else if (v_scanline < DVI_V_FRONT_PORCH + DVI_V_SYNC_WIDTH + DVI_V_BACK_PORCH) {
        ch->read_addr = (uintptr_t)vblank_line_vsync_off;
        ch->transfer_count = count_of(vblank_line_vsync_off);

    } else if (!vactive_cmdlist_posted) {
        
        ch->read_addr = (uintptr_t)vactive_line;
        ch->transfer_count = count_of(vactive_line);
        vactive_cmdlist_posted = true;

    } else {
        int active_line = v_scanline - (DVI_V_TOTAL_LINES - DVI_V_ACTIVE_LINES);
        ch->read_addr = (uintptr_t)(frame_display_ptr + active_line * DVI_H_ACTIVE_PIXELS);
        ch->transfer_count = DVI_H_ACTIVE_PIXELS / sizeof(uint32_t);
        vactive_cmdlist_posted = false;
    }

    if (!vactive_cmdlist_posted) {
        v_scanline = (v_scanline + 1) % DVI_V_TOTAL_LINES;
        if (v_scanline == 0) {
            vsync_flag = true;
        }
    }
}

void dvi_init(void) {
    hstx_ctrl_hw->expand_tmds =
        2 << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB | 
        0 << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB |  
        2 << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB |  
        29 << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB |  
        1 << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB |  
        26 << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB;     

    hstx_ctrl_hw->expand_shift =
        4 << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB |
        8 << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB |
        1 << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB |
        0 << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB;

    hstx_ctrl_hw->csr = 0;
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EXPAND_EN_BITS |
        5u << HSTX_CTRL_CSR_CLKDIV_LSB |
        5u << HSTX_CTRL_CSR_N_SHIFTS_LSB |
        2u << HSTX_CTRL_CSR_SHIFT_LSB |
        HSTX_CTRL_CSR_EN_BITS;

    hstx_ctrl_hw->bit[2] = HSTX_CTRL_BIT0_CLK_BITS;
    hstx_ctrl_hw->bit[3] = HSTX_CTRL_BIT0_CLK_BITS | HSTX_CTRL_BIT0_INV_BITS;

    static const int lane_to_bit[3] = {0, 6, 4};
    for (uint lane = 0; lane < 3; ++lane) {
        int bit = lane_to_bit[lane];
        uint32_t sel =
            (lane * 10 ) << HSTX_CTRL_BIT0_SEL_P_LSB |
            (lane * 10 + 1) << HSTX_CTRL_BIT0_SEL_N_LSB;
        hstx_ctrl_hw->bit[bit] = sel;
        hstx_ctrl_hw->bit[bit + 1] = sel | HSTX_CTRL_BIT0_INV_BITS;
    }

    for (int i = DVI_GPIO_FIRST; i <= DVI_GPIO_LAST; ++i) {
        gpio_set_function(i, 0);
    }

    gpio_init(DOCK_DETECT_PIN);
    gpio_set_dir(DOCK_DETECT_PIN, GPIO_IN);
    gpio_pull_down(DOCK_DETECT_PIN); 

    dma_channel_config c;

    c = dma_channel_get_default_config(DMACH_PING);
    channel_config_set_chain_to(&c, DMACH_PONG);  
    channel_config_set_dreq(&c, DREQ_HSTX);       
    dma_channel_configure(DMACH_PING, &c, &hstx_fifo_hw->fifo, vblank_line_vsync_off, count_of(vblank_line_vsync_off), false);

    c = dma_channel_get_default_config(DMACH_PONG);
    channel_config_set_chain_to(&c, DMACH_PING);  
    channel_config_set_dreq(&c, DREQ_HSTX);
    dma_channel_configure(
        DMACH_PONG, &c,
        &hstx_fifo_hw->fifo,
        vblank_line_vsync_off,
        count_of(vblank_line_vsync_off),
        false
    );

    dma_hw->ints0 = (1u << DMACH_PING) | (1u << DMACH_PONG); 
    dma_hw->inte0 = (1u << DMACH_PING) | (1u << DMACH_PONG); 
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    bus_ctrl_hw->priority =
        BUSCTRL_BUS_PRIORITY_DMA_W_BITS |
        BUSCTRL_BUS_PRIORITY_DMA_R_BITS;
}

void dvi_start(void) {
    v_scanline = 2;
    dma_pong = false;
    vactive_cmdlist_posted = false;
    vsync_flag = false;
    dma_channel_start(DMACH_PING);
}

void dvi_stop(void) {
    irq_set_enabled(DMA_IRQ_0, false);
    dma_channel_abort(DMACH_PING);
    dma_channel_abort(DMACH_PONG);
}

void dvi_set_display_buffer(uint8_t *buf) {
    frame_display_ptr = buf;
}

bool dvi_vsync_occurred(void) {
    if (vsync_flag) {
        vsync_flag = false;
        return true;
    }
    return false;
}