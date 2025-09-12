/***********************************************************************************************************
This example Arduino sketch was developed to work with Anabit's RangeRite ADC open source reference design. 
The RangeRite ADC gets its name from the fact that it supports 9 different voltage ranges: 5 bipolar and 4 unipolar
all generated from a single input power source between 6V and 18V. The RangeRite ADC comes in two resolution 
versions 16 and 18 bit versions. It also comes in two sample rate versions: 100kSPS and 500kSPS. This example
sketch shows you how to set the RangeRite's input votlage range and make a bumch of continous measurements as
fast as possible that are stored in a buffer. You then have the option to print the measurements in the buffer
to the serial plotter or to print some of the measurements to the serial monitor along with metric on how long
it took to make the measurements. The rate the measurements are made depends on the version of RangeRite you are
using, the SPI clock rate of the Arduino board you are using, and how fast it can control the chip select pin. 
Be sure to look at the initial settings for this sketch including SPI chip select pin, RangeRite resolutio
(16 or 18), voltage range, and if you want to use the reset pin

Product link: https://anabit.co/products/rangerite-18-bit-adc

This example sketch demonstrates how to set the input voltage range and make a set of continous ADC reading
From Texas Instruments ADS868x 16 bit ADC IC family or the ADS869x 18 bit ADC IC family.

Please report any issue with the sketch to the Anabit forum: https://anabit.co/community/forum/analog-to-digital-converters-adcs

Example code developed by Your Anabit LLC © 2025
Licensed under the Apache License, Version 2.0.
*************************************************************************************************************/
#include <Arduino.h>
#include <SPI.h>

// ================= Device / build-time options =================
#define ADSX_BITS 18        // 18 for ADS869x, 16 for ADS868x
#define VOLT_REF (float)4.096 //internal voltage reference of ADC
//ensure valid bit setting
#if (ADSX_BITS != 16) && (ADSX_BITS != 18)
  #error "ADSX_BITS must be 16 or 18"
#endif

#define SAMPLE_COUNT 256    // readings buffer size, adjust as needed
#define ADSX_SPI_HZ 40000000  // safe default; try 20000000..40000000 on faster MCUs

// RVS polarity, RVS = HIGH when result valid.
#define ADSX_RVS_READY_HIGH 1   

// ================= Pins (edit to your wiring) =================
// Set pins for chip select, RVS, and RST (optional)
#define PIN_CS 10               
#define PIN_RVS 4             // any free GPIO; change as needed
#define PIN_RST -1             //can use free GPIO or Arduino RST pin or nothing

// ================= SPI settings & ADC fields ==================
SPISettings adsSPI(ADSX_SPI_HZ, MSBFIRST, SPI_MODE0);

#define ADSX_REG_RANGE_SEL   0x14
#define ADSX_INTREF_ENABLE   0x0000  // INTREF_DIS = 0 (internal 4.096 V ref ON)
//Constants to set the input voltage range of the ADC. copy and paste desired range to global "vRange" variable
#define ADSX_RANGE_BIPOLAR_3X       0x0   // ±3.000 × VREF = +/- 12.288V
#define ADSX_RANGE_BIPOLAR_2P5X     0x1   // ±2.500 × VREF = +/- 10.24V
#define ADSX_RANGE_BIPOLAR_1P5X     0x2   // ±1.500 × VREF = +/- 6.144V
#define ADSX_RANGE_BIPOLAR_1P25X    0x3   // ±1.250 × VREF = +/- 5.12V
#define ADSX_RANGE_BIPOLAR_0P625X   0x4   // ±0.625 × VREF = +/- 2.56V
#define ADSX_RANGE_UNIPOLAR_3X      0x8   // 3.000 × VREF = 12.288V
#define ADSX_RANGE_UNIPOLAR_2P5X    0x9   // 2.500 × VREF = 10.24V
#define ADSX_RANGE_UNIPOLAR_1P5X    0xA   // 1.500 × VREF = 6.144V
#define ADSX_RANGE_UNIPOLAR_1P25X   0xB   // 1.250 × VREF = 5.12V --> Default

uint16_t vRange = ADSX_RANGE_BIPOLAR_2P5X; //global variable that is used to set ADC voltage range

#if ADSX_BITS == 18
  #define ADSX_CODE_SHIFT 14
  #define ADSX_CODE_MASK  0x3FFFFu
#else
  #define ADSX_CODE_SHIFT 16
  #define ADSX_CODE_MASK  0xFFFFu
#endif

// ================= Sample buffer =================
// Use 32-bit to keep one code path for 16/18-bit (mask later).
static uint32_t samples[SAMPLE_COUNT];

//SPI helper function to send and read 32 bit SPI package
//input arguments are 4x uint8_t that together equal 32 bit word to send to ADC
//returns 32 bit word read from ADC
static inline uint32_t xfer32(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
  digitalWrite(PIN_CS, LOW);
  uint8_t r0 = SPI.transfer(b0);
  uint8_t r1 = SPI.transfer(b1);
  uint8_t r2 = SPI.transfer(b2);
  uint8_t r3 = SPI.transfer(b3);
  digitalWrite(PIN_CS, HIGH);
  return ((uint32_t)r0 << 24) | ((uint32_t)r1 << 16) | ((uint32_t)r2 << 8) | r3;
}

//NOP write data function to send to ADC, reads back uint32_t value from ADC
static inline uint32_t frameNOP() { return xfer32(0x00, 0x00, 0x00, 0x00); }

//function that checks RVS pin to see if new ADC data is ready to be read
//If RVS is high data is ready so return true, otherwise returns false
static inline bool rvsReady() {
    return digitalRead(PIN_RVS) == HIGH;
}

//fucntion that Writes 16 bits to a register (WRITE HWORD opcode 0xD0)
//first input argument is register address and second is 16 bits of data
static void writeRegHWord(uint8_t addr, uint16_t data) {
  (void)xfer32(0xD0, addr, (uint8_t)(data >> 8), (uint8_t)data);
}

// Minimal ADC config: internal 4.096 V ref ON, range set by global "vRange".
// Flush the first invalid conversion using RVS.
static void adsxConfigureBasic() {
  SPI.beginTransaction(adsSPI);
  writeRegHWord(ADSX_REG_RANGE_SEL, ADSX_INTREF_ENABLE | (vRange & 0x0F));
  // start a conversion
  (void)frameNOP();
  SPI.endTransaction();

  // wait for first result valid and discard it
  while (!rvsReady()) { /* spin */ }
  SPI.beginTransaction(adsSPI);
  (void)frameNOP();   // read & discard; also starts next conversion
  SPI.endTransaction();
}

// Capture N samples as fast as possible, paced by RVS and SPI clock speed.
// Returns elapsed time in microseconds (from micros()).
static uint32_t capture_fast(uint32_t N) {
  // Start first conversion
  SPI.beginTransaction(adsSPI);
  (void)frameNOP();
  SPI.endTransaction();

  // Wait for first result ready
  while (!rvsReady()) { /* spin */ }

  uint32_t t0 = micros();

  // Keep SPI settings locked during the burst
  SPI.beginTransaction(adsSPI);
  for (uint32_t i = 0; i < N; ++i) {
    while (!rvsReady()) { /* spin */ }
    samples[i] = frameNOP();   // store raw 32-bit word; also starts next conversion
  }
  SPI.endTransaction();

  return micros() - t0;
}

// Post-process samples after fast read: right-shift & mask AFTER capture
static void decode_samples_in_place(uint32_t N) {
  for (uint32_t i = 0; i < N; ++i) {
    samples[i] = (samples[i] >> ADSX_CODE_SHIFT) & ADSX_CODE_MASK;
  }
}

//function returns multiplier to calculate measureed voltage based on ADC input voltage range setting
//first input argument is the ADC's range code and the second argument is for if the range is bipolar
//returns range multiplier as a float
static inline float adsxRangeMultiplier(uint16_t code, bool &isBipolar) {
  switch (code & 0xF) {
    case ADSX_RANGE_BIPOLAR_3X:     isBipolar = true;  return 3.0f;
    case ADSX_RANGE_BIPOLAR_2P5X:   isBipolar = true;  return 2.5f;
    case ADSX_RANGE_BIPOLAR_1P5X:   isBipolar = true;  return 1.5f;
    case ADSX_RANGE_BIPOLAR_1P25X:  isBipolar = true;  return 1.25f;
    case ADSX_RANGE_BIPOLAR_0P625X: isBipolar = true;  return 0.625f;
    case ADSX_RANGE_UNIPOLAR_3X:    isBipolar = false; return 3.0f;
    case ADSX_RANGE_UNIPOLAR_2P5X:  isBipolar = false; return 2.5f;
    case ADSX_RANGE_UNIPOLAR_1P5X:  isBipolar = false; return 1.5f;
    case ADSX_RANGE_UNIPOLAR_1P25X: isBipolar = false; return 1.25f;
    default:                        isBipolar = false; return 1.25f;
  }
}

// Function to convert a raw ADC code to volts for the currently selected range.
// Returns voltage as a float for both unipolar and bipolar ranges.
// LSB = FSR / 2^N; Volts = NFS + code * LSB.
//input argument is measured ADC code
float adsxCodeToVolts(uint32_t codeN) {
  if (codeN > ADSX_CODE_MASK) codeN = ADSX_CODE_MASK;

  bool bipolar;
  const float mult = adsxRangeMultiplier(vRange, bipolar);
  const float vref = VOLT_REF;
  const float PFS  = mult * vref;           // +full-scale
  const float NFS  = bipolar ? -PFS : 0.0f; // -full-scale (or 0 for unipolar)
  const float FSR  = PFS - NFS;             // span
  const float LSB  = FSR / (float)(1UL << ADSX_BITS);

  return NFS + (float)codeN * LSB;
}

// ================= Sketch =================
void setup() {
  Serial.begin(115200); //start serial communication
  delay(2000);

  //setup CS, RVS, and RST pins
  pinMode(PIN_CS, OUTPUT);  digitalWrite(PIN_CS, HIGH);
  pinMode(PIN_RVS, INPUT);
  if (PIN_RST >= 0) {
    pinMode(PIN_RST, OUTPUT);
    digitalWrite(PIN_RST, HIGH);
    digitalWrite(PIN_RST, LOW); delay(1);
    digitalWrite(PIN_RST, HIGH);
  }
  delay(20);   // power-up settle

  SPI.begin(); // use board's default hardware SPI pins
  adsxConfigureBasic(); //configure ADC
  //print out ADC settings
  Serial.print(F("ADS86"));
  Serial.print(ADSX_BITS);
  Serial.print(F(" burst capture, SPI="));
  Serial.print(ADSX_SPI_HZ / 1e6, 1);
  Serial.println(F(" MHz, using RVS pacing."));
}

void loop() {
  uint32_t dt_us = capture_fast(SAMPLE_COUNT);
  float sps = (float)SAMPLE_COUNT / (dt_us * 1e-6f);
  decode_samples_in_place(SAMPLE_COUNT); //decode samples after fast capature (bit-shift & mask)

  //This serial print out section is meant to be used with Arduino serial plotter tool
  //it just outputs raw voltage values stored in the buffer
  /*
  for(uint16_t j=0; j<SAMPLE_COUNT; j++) {
    float volts   = adsxCodeToVolts(samples[j]); //convert ADC code to voltage
    Serial.println(volts); //print to serial plotter
  }
  */

  // uncomment if you want to print timing and sample inforamtaion to serial monitor
  Serial.print(F("Captured "));
  Serial.print(SAMPLE_COUNT);
  Serial.print(F(" samples in "));
  Serial.print(dt_us);
  Serial.print(F(" us  (~"));
  Serial.print(sps, 1);
  Serial.println(F(" Sa/s)"));

  // Show a few samples (avoid printing the entire buffer each second)
  Serial.print(F("First 4: "));
  for (int i = 0; i < min(4, (int)SAMPLE_COUNT); ++i) { Serial.print(samples[i]); Serial.print(' '); }
  Serial.print(F(" ... Last 4: "));
  for (int i = max(0, (int)SAMPLE_COUNT - 4); i < (int)SAMPLE_COUNT; ++i) { Serial.print(samples[i]); Serial.print(' '); }
  Serial.println(); 

  delay(2000);
}

