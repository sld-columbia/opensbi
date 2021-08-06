/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2019 FORTH-ICS/CARV
 *				Panagiotis Peristerakis <perister@ics.forth.gr>
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
#include <sbi/riscv_io.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_platform.h>
#include <sbi_utils/fdt/fdt_fixup.h>
#include <sbi_utils/ipi/aclint_mswi.h>
#include <sbi_utils/irqchip/plic.h>
#include <sbi_utils/serial/gaisler-uart.h>
#include <sbi_utils/timer/aclint_mtimer.h>

#define ESP_UART_ADDR			0x60000100
#define ESP_BASE_FREQ			78000000
#define ESP_UART_BAUDRATE		    38400
#define ESP_PLIC_ADDR			0x6c000000
#define ESP_PLIC_NUM_SOURCES	    30
#define ESP_HART_COUNT			4
#define ESP_CLINT_ADDR			0x2000000
#define ESP_ACLINT_MSWI_ADDR			(ESP_CLINT_ADDR + \
						 CLINT_MSWI_OFFSET)
#define ESP_ACLINT_MTIMER_ADDR		(ESP_CLINT_ADDR + \
						 CLINT_MTIMER_OFFSET)

static struct plic_data plic = {
	.addr = ESP_PLIC_ADDR,
	.num_src = ESP_PLIC_NUM_SOURCES,
};

static struct aclint_mswi_data mswi = {
	.addr = ESP_ACLINT_MSWI_ADDR,
	.size = ACLINT_MSWI_SIZE,
	.first_hartid = 0,
	.hart_count = ESP_HART_COUNT,
};

static struct aclint_mtimer_data mtimer = {
	.addr = ESP_ACLINT_MTIMER_ADDR,
	.size = ACLINT_MTIMER_SIZE,
	.first_hartid = 0,
	.hart_count = ESP_HART_COUNT,
	.has_64bit_mmio = TRUE,
};

/*
 * Ariane platform early initialization.
 */
static int esp_early_init(bool cold_boot)
{
	/* For now nothing to do. */
	return 0;
}

/*
 * Ariane platform final initialization.
 */
static int esp_final_init(bool cold_boot)
{
	void *fdt;

	if (!cold_boot)
		return 0;

	fdt = sbi_scratch_thishart_arg1_ptr();
	fdt_fixups(fdt);

	return 0;
}

/*
 * Initialize the esp console.
 */
static int esp_console_init(void)
{
	return gaisler_uart_init(ESP_UART_ADDR,
			     ESP_BASE_FREQ,
			     ESP_UART_BAUDRATE);
}

static int plic_esp_warm_irqchip_init(int m_cntx_id, int s_cntx_id)
{
	size_t i, ie_words = ESP_PLIC_NUM_SOURCES / 32 + 1;

	/* By default, enable all IRQs for M-mode of target HART */
	if (m_cntx_id > -1) {
		for (i = 0; i < ie_words; i++)
			plic_set_ie(&plic, m_cntx_id, i, 1);
	}
	/* Enable all IRQs for S-mode of target HART */
	if (s_cntx_id > -1) {
		for (i = 0; i < ie_words; i++)
			plic_set_ie(&plic, s_cntx_id, i, 1);
	}
	/* By default, enable M-mode threshold */
	if (m_cntx_id > -1)
		plic_set_thresh(&plic, m_cntx_id, 1);
	/* By default, disable S-mode threshold */
	if (s_cntx_id > -1)
		plic_set_thresh(&plic, s_cntx_id, 0);

	return 0;
}

/*
 * Initialize the interrupt controller for current HART.
 */
static int esp_irqchip_init(bool cold_boot)
{
	u32 hartid = current_hartid();
	int ret;

	if (cold_boot) {
		ret = plic_cold_irqchip_init(&plic);
		if (ret)
			return ret;
	}
	return plic_esp_warm_irqchip_init(2 * hartid, 2 * hartid + 1);
}

/*
 * Initialize IPI for current HART.
 */
static int esp_ipi_init(bool cold_boot)
{
	int ret;

	if (cold_boot) {
		ret = aclint_mswi_cold_init(&mswi);
		if (ret)
			return ret;
	}

	return aclint_mswi_warm_init();
}

/*
 * Initialize timer for current HART.
 */
static int esp_timer_init(bool cold_boot)
{
	int ret;

	if (cold_boot) {
		ret = aclint_mtimer_cold_init(&mtimer, NULL);
		if (ret)
			return ret;
	}

	return aclint_mtimer_warm_init();
}

/*
 * Platform descriptor.
 */
const struct sbi_platform_operations platform_ops = {
	.early_init = esp_early_init,
	.final_init = esp_final_init,
	.console_init = esp_console_init,
	.irqchip_init = esp_irqchip_init,
	.ipi_init = esp_ipi_init,
	.timer_init = esp_timer_init,
};

const struct sbi_platform platform = {
	.opensbi_version = OPENSBI_VERSION,
	.platform_version = SBI_PLATFORM_VERSION(0x0, 0x01),
	.name = "ESP-ARIANE RISC-V",
	.features = SBI_PLATFORM_DEFAULT_FEATURES,
	.hart_count = ESP_HART_COUNT,
	.hart_stack_size = SBI_PLATFORM_DEFAULT_HART_STACK_SIZE,
	.platform_ops_addr = (unsigned long)&platform_ops
};
