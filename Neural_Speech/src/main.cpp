#include <Arduino.h>
#define SAMPLING_FREQ 32000

volatile uint16_t sample = 0;
volatile uint16_t counter = 0;
uint16_t x0 = 0, x1 = 0, x2 = 0, x3 = 0;
uint16_t filtered = 0;

void setupADC() {
  pmc_enable_periph_clk(ID_ADC);

  ADC->ADC_MR = ADC_MR_PRESCAL(1)
              | ADC_MR_STARTUP_SUT64
              | ADC_MR_TRACKTIM(15)
              | ADC_MR_TRANSFER(2);

  ADC->ADC_CHER = ADC_CHER_CH7;
}

void setupTimer() {
  pmc_set_writeprotect(false);
  pmc_enable_periph_clk(ID_TC0);

  TC_Configure(TC0, 0,
               TC_CMR_TCCLKS_TIMER_CLOCK1 |
               TC_CMR_WAVE |
               TC_CMR_WAVSEL_UP_RC);

  uint32_t rc = VARIANT_MCK / 2 / SAMPLING_FREQ;

  TC_SetRC(TC0, 0, rc);

  TC0->TC_CHANNEL[0].TC_IER = TC_IER_CPCS;
  TC0->TC_CHANNEL[0].TC_IDR = ~TC_IER_CPCS;

  NVIC_EnableIRQ(TC0_IRQn);

  TC_Start(TC0, 0);
}

void setup() {
  Serial.begin(115200);
  pinMode(13, OUTPUT);
  setupADC();
  setupTimer();
}

void loop() {
 if (counter >= 500) {
    Serial.println(filtered);
    counter = 0;
  }
}

void TC0_Handler() {
  TC0->TC_CHANNEL[0].TC_SR;

  ADC->ADC_CR = ADC_CR_START;
  while (!(ADC->ADC_ISR & ADC_ISR_EOC7));

  x3 = x2;
  x2 = x1;
  x1 = x0;
  x0 = ADC->ADC_CDR[7];

  filtered = (x0 + x1 + x2 + x3) / 4;
}