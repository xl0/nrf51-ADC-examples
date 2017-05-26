/* Copyright (c) 2009 Nordic Semiconductor. All Rights Reserved.
 *
 * The information contained herein is property of Nordic Semiconductor ASA.
 * Terms and conditions of usage are described in detail in NORDIC
 * SEMICONDUCTOR STANDARD SOFTWARE LICENSE AGREEMENT.
 *
 * Licensees are granted free, non-transferable use of the information. NO
 * WARRANTY of ANY KIND is provided. This heading must NOT be removed from
 * the file.
 *
 */

#include <stdbool.h>
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "app_util_platform.h"

#include "app_uart.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"


#define LFCLK_FREQUENCY           (32768UL)                 /*!< LFCLK frequency in Hertz, constant */
#define RTC_FREQUENCY             (234UL)                     /*!< required RTC working clock RTC_FREQUENCY Hertz. Changable */
#define COUNTER_PRESCALER         ((LFCLK_FREQUENCY/RTC_FREQUENCY) - 1)  /* f = LFCLK/(prescaler + 1) */

#define GPIO_SDN 24
#define GPIO_LON 22
#define GPIO_LOP 23


void uart_error_handle(app_uart_evt_t * p_event)
{}

static void hfclk_config(void)
{
  /* Start 16 MHz crystal oscillator */
  NRF_CLOCK->EVENTS_HFCLKSTARTED = 0;
  NRF_CLOCK->TASKS_HFCLKSTART = 1;

  /* Wait for the external oscillator to start up */
  while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0) {}
}

static void lfclk_config(void)
{
  NRF_CLOCK->LFCLKSRC = (CLOCK_LFCLKSRC_SRC_RC << CLOCK_LFCLKSRC_SRC_Pos);
  NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
  NRF_CLOCK->TASKS_LFCLKSTART = 1;

  // Wait for the low frequency clock to start
  while (NRF_CLOCK->EVENTS_LFCLKSTARTED == 0) {}
  NRF_CLOCK->EVENTS_LFCLKSTARTED = 0;
}

/** Configure and start RTC
 */
static void rtc_config(void)
{
  NRF_RTC0->PRESCALER = COUNTER_PRESCALER;                   // Set prescaler to a TICK of RTC_FREQUENCY
  NRF_RTC0->EVTENSET = RTC_EVTENSET_TICK_Msk;                // Enable TICK event
	NRF_RTC0->TASKS_START = 1;                                 // Start RTC0
}

static void ppi_init(void)
{
  // Configure PPI channel 0 to start ADC task
  NRF_PPI->CH[0].EEP = (uint32_t)&NRF_RTC0->EVENTS_TICK;
  NRF_PPI->CH[0].TEP = (uint32_t)&NRF_ADC->TASKS_START;

  // Enable PPI channel 0
  NRF_PPI->CHEN = (PPI_CHEN_CH0_Enabled << PPI_CHEN_CH0_Pos);
}

/** Configures and enables the ADC
 */
void ADC_init(void)
{
	/* Enable interrupt on ADC sample ready event*/
	NRF_ADC->INTENSET = ADC_INTENSET_END_Msk;
	NVIC_EnableIRQ(ADC_IRQn);

	// config ADC
	NRF_ADC->CONFIG	= (ADC_CONFIG_EXTREFSEL_None << ADC_CONFIG_EXTREFSEL_Pos) 								/* Bits 17..16 : ADC external reference pin selection. */
										| (ADC_CONFIG_PSEL_AnalogInput3 << ADC_CONFIG_PSEL_Pos)									/*!< Use analog input 6 as analog input (P0.05). */
										| (ADC_CONFIG_REFSEL_VBG << ADC_CONFIG_REFSEL_Pos)											/*!< Use internal 1.2V bandgap voltage as reference for conversion. */
										| (ADC_CONFIG_INPSEL_AnalogInputOneThirdPrescaling << ADC_CONFIG_INPSEL_Pos) 	/*!< Analog input specified by PSEL with no prescaling used as input for the conversion. */
										| (ADC_CONFIG_RES_10bit << ADC_CONFIG_RES_Pos);													/*!< 10bit ADC resolution. */ 


	/* Enable ADC*/
	NRF_ADC->ENABLE = ADC_ENABLE_ENABLE_Enabled;
}

/* Interrupt handler for ADC data ready event. It will be executed when ADC sampling is complete */
void ADC_IRQHandler(void)
{
	/* Clear dataready event */
        NRF_ADC->EVENTS_END = 0;
 	int lop = nrf_gpio_pin_read(GPIO_LOP);
        int lon = nrf_gpio_pin_read(GPIO_LON);


	printf("%u %d %d\r\n", NRF_ADC->RESULT, lop, lon);

	//Print ADC result on UART
//	NRF_LOG_INFO("sample result: %X\r\n", NRF_ADC->RESULT);
//	NRF_LOG_FLUSH();
}


/**
 * main function
 */
int main(void)
{
	uint32_t err_code;


      const app_uart_comm_params_t comm_params =
      {
          11,
          9,
          -1,
          -1,
          0,
          false,
          UART_BAUDRATE_BAUDRATE_Baud115200
      };

      APP_UART_FIFO_INIT(&comm_params,
			64, 64,
                         uart_error_handle,
                         APP_IRQ_PRIORITY_LOWEST,
                         err_code);

      APP_ERROR_CHECK(err_code);

      nrf_gpio_cfg_output(GPIO_SDN);
      nrf_gpio_cfg_input(GPIO_LOP, NRF_GPIO_PIN_NOPULL);
      nrf_gpio_cfg_input(GPIO_LON, NRF_GPIO_PIN_NOPULL);


      nrf_gpio_cfg_output(29);
      nrf_gpio_cfg_output(21);

      nrf_gpio_pin_set(21);
      nrf_gpio_pin_clear(29);

/*
      for(int i = 0; i < 10; i++) {
 
      nrf_gpio_pin_set(29);

      nrf_delay_ms(1000);

      nrf_gpio_pin_clear(29);



      nrf_gpio_pin_set(21);

      nrf_delay_ms(1000);

      nrf_gpio_pin_clear(21);

      }

*/

	printf("Hello!\r\n");

	printf("0\r\n");
	hfclk_config();     //Enable 16MHz crystal for maximum ADC accuracy. Comment out this line to use internal 16MHz RC instead, which saves power.
	lfclk_config();			//Configure 32kHz clock, required by the RTC timer

	printf("1\r\n");

        rtc_config();				//Configure RTC timer
	printf("2\r\n");
	ppi_init();					//Configure PPI channel, which connects RTC TICK event and ADC START task
	printf("3\r\n");
	ADC_init();					//Configure the ADC
	printf("4\r\n");

  while (true)
  {
	//Put CPU to sleep while waiting for interrupt
	__WFI();
  }
}
