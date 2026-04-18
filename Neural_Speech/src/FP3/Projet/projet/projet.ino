#include <Arduino.h>

#define FS_IN 32000
#define FS_OUT 8000
#define DECIMATION 4
#define ADC_CHANNEL 7
#define FIR_TAPS 21
#define RECORD_TIME_SEC 1
#define BUFFER_SIZE (FS_OUT * RECORD_TIME_SEC)
#define BUTTON_PIN 2

volatile uint16_t sample32k = 0;
volatile uint16_t filtered32k = 0;
volatile uint16_t sample8k = 0;
volatile bool newSample8k = false;
volatile uint8_t decimCount = 0;

volatile bool recording = false;
volatile bool recordDone = false;
volatile uint16_t writeIndex = 0;

uint16_t audioBuffer[BUFFER_SIZE];

// Coefficients FIR passe-bas
const int16_t firCoeff[FIR_TAPS] = {
   -120,  -180,  -260,  -300,    0,
    650,  1700,  3100,  4500,  5400,
   5800,
   5400,  4500,  3100,  1700,
    650,    0,  -300,  -260,  -180,
   -120
};

volatile uint16_t firBuffer[FIR_TAPS];
volatile uint8_t firIndex = 0;

void setupADC() {
  pmc_enable_periph_clk(ID_ADC);

  ADC->ADC_MR = ADC_MR_PRESCAL(1)
              | ADC_MR_STARTUP_SUT64
              | ADC_MR_TRACKTIM(15)
              | ADC_MR_TRANSFER(2);

  ADC->ADC_CHER = (1 << ADC_CHANNEL);
}

void setupTimer() {
  pmc_set_writeprotect(false);
  pmc_enable_periph_clk(ID_TC0);

  TC_Configure(TC0, 0,
               TC_CMR_TCCLKS_TIMER_CLOCK1 |
               TC_CMR_WAVE |
               TC_CMR_WAVSEL_UP_RC);

  uint32_t rc = VARIANT_MCK / 2 / FS_IN;
  TC_SetRC(TC0, 0, rc);

  TC0->TC_CHANNEL[0].TC_IER = TC_IER_CPCS;
  TC0->TC_CHANNEL[0].TC_IDR = ~TC_IER_CPCS;

  NVIC_EnableIRQ(TC0_IRQn);
  TC_Start(TC0, 0);
}

uint16_t applyFIR(uint16_t input) {
  firBuffer[firIndex] = input;

  int32_t acc = 0;
  int16_t idx = firIndex;

  for (uint8_t i = 0; i < FIR_TAPS; i++) {
    acc += (int32_t)firBuffer[idx] * firCoeff[i];
    idx--;
    if (idx < 0) {
      idx = FIR_TAPS - 1;
    }
  }

  firIndex++;
  if (firIndex >= FIR_TAPS) {
    firIndex = 0;
  }

  acc >>= 15;

  if (acc < 0) {
    acc = 0;
  }
  if (acc > 4095) {
    acc = 4095;
  }

  return (uint16_t)acc;
}

void setup() {
  Serial.begin(115200);
  pinMode(13, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  for (uint8_t i = 0; i < FIR_TAPS; i++) {
    firBuffer[i] = 0;
  }

  for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
    audioBuffer[i] = 0;
  }

  setupADC();
  setupTimer();
}

void loop() {
  if (!recording && !recordDone && digitalRead(BUTTON_PIN) == LOW) {
    delay(20);
    if (digitalRead(BUTTON_PIN) == LOW) {
      noInterrupts();
      writeIndex = 0;
      recording = true;
      recordDone = false;
      interrupts();
      digitalWrite(13, HIGH);
    }
  }

  if (recordDone) {
    noInterrupts();
    recordDone = false;
    interrupts();

    digitalWrite(13, LOW);

    for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
      Serial.println(audioBuffer[i]);
    }

    Serial.println("END");
    delay(1000);
  }
}

void TC0_Handler() {
  TC0->TC_CHANNEL[0].TC_SR;

  ADC->ADC_CR = ADC_CR_START;
  while (!(ADC->ADC_ISR & (1 << ADC_CHANNEL)));

  sample32k = ADC->ADC_CDR[ADC_CHANNEL];
  filtered32k = applyFIR(sample32k);

  decimCount++;
  if (decimCount >= DECIMATION) {
    decimCount = 0;
    sample8k = filtered32k;
    newSample8k = true;

    if (recording) {
      audioBuffer[writeIndex] = sample8k;
      writeIndex++;

      if (writeIndex >= BUFFER_SIZE) {
        recording = false;
        recordDone = true;
      }
    }
  }
}
