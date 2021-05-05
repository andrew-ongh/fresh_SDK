/**
 ****************************************************************************************
 *
 * @file demo_irgen.c
 *
 * @brief IR generator demo (hw_irgen driver)
 *
 * Copyright (C) 2015-2016 Dialog Semiconductor.
 * This computer program includes Confidential, Proprietary Information
 * of Dialog Semiconductor. All Rights Reserved.
 *
 ****************************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <hw_irgen.h>
#include "common.h"

static const uint8_t nec_address = 0x68;
static const uint8_t nec_command = 0x00;

static volatile int intr_count = 0;

static void irgen_intr_cb(void)
{
        /* if intr_count is less than 0 then we set generator to repeat until stopped manually */
        if (intr_count >= 0) {
                if (!intr_count) {
                        hw_irgen_stop();
                        hw_irgen_unregister_interrupt();
                }

                intr_count--;
        }

        /* clear interrupt in handler function to avoid triggering it again */
        hw_irgen_clear_interrupt();
}

void menu_irgen_send_nec_func(const struct menu_item *m, bool checked)
{
        uint8_t addr, cmd;

        /*
         * Reset IR generator block to default values.
         * An optional parameter with configuration can be specified. In such case IR generator will
         * be configured immediately - see example later in code, after manual configuration.
         */
        hw_irgen_init(NULL);

        /*
         * NEC protocol uses 38kHz carrier frequency with recommended 1/3 duty cycle.
         * Carrier frequency should be [kHz] 16000 / 38 =~ 421 cycles with 1/3 duty cycle, i.e.
         * 140 clock cycles high state and 281 clock cycles low state.
         */
        hw_irgen_set_carrier_freq(140, 281);

        /*
         * NEC protocol uses pulse distance encoding with pulse length of 562.5uS. Logic 0 is single
         * pulse followed with space of the same time, logic 1 is single pulse followed by space
         * 3 times longer.
         *
         * At 38kHz carrier, pulse length is approximately 21 carries cycles long (21.375), thus
         * logic 0 should be 21 cycles pulse and 21 cycles space and logic 1 - 21 cycles pulse and
         * 64 cycles space.
         */
        hw_irgen_set_logic0_param(HW_IRGEN_LOGIC_FORMAT_MARK, 21, 21);
        hw_irgen_set_logic1_param(HW_IRGEN_LOGIC_FORMAT_MARK, 21, 64);

        /*
         * NEC protocol uses custom code for repeating commands thus repeat FIFO needs to be enabled.
         * In case of protocols which simply transmit the same command over and over, code FIFO
         * should be used. Repeat code is sent every 110ms (4180 carrier cycles).
         */
        hw_irgen_set_repeat_time(4180);
        hw_irgen_set_repeat_fifo(HW_IRGEN_FIFO_REPEAT);

        /*
         * IR LED's cathode is controllable on sensors board.
         */
        hw_irgen_set_output_type(HW_IRGEN_OUTPUT_INVERTED);

        /*
         * Equivalent configuration can be also defined in irgen_config structure:
         *
         * irgen_config cfg = {
         *         .carrier_hi = 140,
         *         .carrier_lo = 281,
         *
         *         .logic0 = {
         *                 .format = HW_IRGEN_LOGIC_FORMAT_MARK,
         *                 .mark_time = 21,
         *                 .space_time = 21,
         *         },
         *
         *         .logic1 = {
         *                 .format = HW_IRGEN_LOGIC_FORMAT_MARK,
         *                 .mark_time = 21,
         *                 .space_time = 21,
         *         },
         *
         *         .repeat_fifo = HW_IRGEN_FIFO_REPEAT,
         *         .repeat_time = 4180,
         * };
         *
         * Such configuration can be applied using init or configure APIs, i.e.:
         * 1.
         * hw_irgen_init(&cfg); // this calls hw_irgen_configure internally
         * 2.
         * hw_irgen_init(NULL);
         * hw_irgen_configure(&cfg);
         *
         */

        /*
         * In this case flushing FIFOs is not actually necessary since they were already flushed
         * when IR generator block was reset. In other case, it may be convinient to just reset
         * FIFOs and program new messages without having to redo all the configuration.
         */
        hw_irgen_flush_fifo(HW_IRGEN_FIFO_CODE);
        hw_irgen_flush_fifo(HW_IRGEN_FIFO_REPEAT);

        /*
         * Both address and command are transmitted with least significant bit first which is
         * opposite to actual value used in code. It can be easily adjusted using helper function.
         */
        addr = hw_irgen_reverse_bit_order(nec_address, 8);
        cmd = hw_irgen_reverse_bit_order(nec_command, 8);

        /*
         * Regular message starts with custom 9ms pulse burst (342 carrier cycles) followed by 4.5ms
         * space which can be programmed using paint messages. Address and command are constructed
         * using pre-programmed logic 0 and 1 thus can be programmed using digital messages. First
         * address, then inverted address, command and inverted command are transmitted.
         */
        hw_irgen_insert_paint_message(HW_IRGEN_FIFO_CODE, HW_IRGEN_PAINT_MARK, 342);
        hw_irgen_insert_paint_message(HW_IRGEN_FIFO_CODE, HW_IRGEN_PAINT_SPACE, 171);
        hw_irgen_insert_digital_message(HW_IRGEN_FIFO_CODE, 8, addr);
        hw_irgen_insert_digital_message(HW_IRGEN_FIFO_CODE, 8, ~addr & 0xFF);
        hw_irgen_insert_digital_message(HW_IRGEN_FIFO_CODE, 8, cmd);
        hw_irgen_insert_digital_message(HW_IRGEN_FIFO_CODE, 8, ~cmd & 0xFF);

        /*
         * Repeat message is custom one - 9ms pulse burst, followed by 2.25ms space and 562.5us
         * pulse (see above for carrier cycle calculations).
         */
        hw_irgen_insert_paint_message(HW_IRGEN_FIFO_REPEAT, HW_IRGEN_PAINT_MARK, 342);
        hw_irgen_insert_paint_message(HW_IRGEN_FIFO_REPEAT, HW_IRGEN_PAINT_SPACE, 85);
        hw_irgen_insert_paint_message(HW_IRGEN_FIFO_REPEAT, HW_IRGEN_PAINT_MARK, 21);

        /*
         * Interrupt is generated every time command (regular or repeated) is transmitted. Once
         * IR generator is started, it will immediately transmit command.
         */
        intr_count = (int) m->param;
        hw_irgen_register_interrupt(irgen_intr_cb);
        hw_irgen_start();
}

void menu_irgen_stop_func(const struct menu_item *m, bool checked)
{
        hw_irgen_stop();
        hw_irgen_unregister_interrupt();
}
