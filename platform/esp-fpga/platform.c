/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * ESP + CVA6 platform integration for OpenSBI
 */

#include <sbi/riscv_asm.h>
#include <sbi/riscv_encoding.h>
// #include <sbi/sbi_console.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_platform.h>
#include <sbi_utils/fdt/fdt_fixup.h>
#include <sbi_utils/fdt/fdt_helper.h>
#include <sbi_utils/ipi/aclint_mswi.h>
#include <sbi_utils/irqchip/plic.h>
#include <sbi_utils/serial/gaisler-uart.h>
#include <sbi_utils/timer/aclint_mtimer.h>

#define ESP_UART_ADDR			0x60000100
#define ESP_BASE_FREQ			(BASE_FREQ_MHZ * 1000000)
#define ESP_UART_BAUDRATE		38400
#define ESP_PLIC_ADDR			0x6c000000
#define ESP_PLIC_SIZE			(0x200000 + (ESP_HART_COUNT * 0x1000))
#define ESP_PLIC_NUM_SOURCES		30
#define ESP_HART_COUNT			16
#define ESP_CLINT_ADDR			0x2000000
#define ESP_ACLINT_MSWI_ADDR		(ESP_CLINT_ADDR + CLINT_MSWI_OFFSET)
#define ESP_ACLINT_MTIMER_ADDR		(ESP_CLINT_ADDR + CLINT_MTIMER_OFFSET)
#define ESP_ACLINT_MTIMER_FREQ	39062500

static struct plic_data plic = {
	.addr = ESP_PLIC_ADDR,
	.size = ESP_PLIC_SIZE,
	.num_src = ESP_PLIC_NUM_SOURCES,
	.flags = PLIC_FLAG_ARIANE_BUG,
    .context_map = {
        [0]  = {  0,  1 },
        [1]  = {  2,  3 },
        [2]  = {  4,  5 },
        [3]  = {  6,  7 },
        [4]  = {  8,  9 },
        [5]  = { 10, 11 },
        [6]  = { 12, 13 },
        [7]  = { 14, 15 },
        [8]  = { 16, 17 },
        [9]  = { 18, 19 },
        [10] = { 20, 21 },
        [11] = { 22, 23 },
        [12] = { 24, 25 },
        [13] = { 26, 27 },
        [14] = { 28, 29 },
        [15] = { 30, 31 },
    },
};

static struct aclint_mswi_data mswi = {
	.addr = ESP_ACLINT_MSWI_ADDR,
	.size = ACLINT_MSWI_SIZE,
	.first_hartid = 0,
	.hart_count = ESP_HART_COUNT,
};

static struct aclint_mtimer_data mtimer = {
	.mtime_freq = ESP_ACLINT_MTIMER_FREQ,
	.mtime_addr = ESP_ACLINT_MTIMER_ADDR + ACLINT_DEFAULT_MTIME_OFFSET,
	.mtime_size = ACLINT_DEFAULT_MTIME_SIZE,
	.mtimecmp_addr = ESP_ACLINT_MTIMER_ADDR +
			 ACLINT_DEFAULT_MTIMECMP_OFFSET,
	.mtimecmp_size = ACLINT_DEFAULT_MTIMECMP_SIZE,
	.first_hartid = 0,
	.hart_count = ESP_HART_COUNT,
	.has_64bit_mmio = true,
};

/*
 * ESP+CVA6 platform early initialization.
 */
static int esp_early_init(bool cold_boot)
{
	int rc;
	// sbi_printf("[CVA6][FINAL INIT] esp_final_init(cold_boot=%d)\n", cold_boot);
	if (!cold_boot)
	return 0;

	rc = gaisler_uart_init(ESP_UART_ADDR, ESP_BASE_FREQ,
				 ESP_UART_BAUDRATE);
	if (rc)
	return rc;

	// sbi_printf("[CVA6][IPI] calling aclint_mswi_warm_init()\n");
	return aclint_mswi_cold_init(&mswi);
}

/*
 * ESP+CVA6 platform final initialization.
 */
static int esp_final_init(bool cold_boot)
{
	void *fdt;
	// sbi_printf("[CVA6][FINAL INIT] esp_final_init(cold_boot=%d)\n", cold_boot);

	if (!cold_boot)
		return 0;

	fdt = fdt_get_address_rw();
	fdt_fixups(fdt);

	// sbi_printf("[CVA6][FINAL INIT] FDT fixups done\n");
	return 0;
}


/*
 * Initialize the ariane interrupt controller during cold boot.
 */
static int esp_irqchip_init(void)
{
	return plic_cold_irqchip_init(&plic);
}

static int esp_timer_init(void)
{
	// sbi_printf("[CVA6][TIMER] cold init: mtime=0x%lx size=0x%lx mtimecmp=0x%lx size=0x%lx freq=%lu hart_count=%u\n",
	// 		mtimer.mtime_addr, mtimer.mtime_size,
	// 		mtimer.mtimecmp_addr, mtimer.mtimecmp_size,
	// 		mtimer.mtime_freq, mtimer.hart_count);

	return aclint_mtimer_cold_init(&mtimer, NULL);
}

const struct sbi_platform_operations platform_ops = {
	.early_init = esp_early_init,
	.final_init = esp_final_init,
	.irqchip_init = esp_irqchip_init,
	.timer_init = esp_timer_init,
};

const struct sbi_platform platform = {
	.opensbi_version = OPENSBI_VERSION,
	.platform_version = SBI_PLATFORM_VERSION(0x0, 0x01),
	.name = "ESP+CVA6 RISC-V",
	.features = SBI_PLATFORM_DEFAULT_FEATURES,
	.hart_count = ESP_HART_COUNT,
	.hart_stack_size = SBI_PLATFORM_DEFAULT_HART_STACK_SIZE,
	.heap_size		= SBI_PLATFORM_DEFAULT_HEAP_SIZE(1),
	.platform_ops_addr = (unsigned long)&platform_ops,
};
