// ***********************************************************
// Redefine the wiringPi library
// ***********************************************************
#define LibSetup()						wiringPiSetup()
// SPI 0
#define hal_SPI0Setup(a1)				wiringPiSPISetup(LOW, a1)				// a1: clock speed in Hz	
#define hal_SPI0DataRW(a1, a2)			wiringPiSPIDataRW(LOW, a1, a2)			// a1: pointer to data, a2: length
// SPI 1
#define hal_SPI1Setup(a1)				wiringPiSPISetup(HIGH, a1)				// a1: clock speed in Hz	
#define hal_SPI1DataRW(a1, a2)			wiringPiSPIDataRW(HIGH, a1, a2)			// a1: pointer to data, a2: length

// GPIO
#define hal_GPIOSetPin(a1)				digitalWrite(a1, HIGH)					// a1: pin number
#define hal_GPIOClearPin(a1)			digitalWrite(a1, LOW)					// a1: pin number
#define hal_GPIOGetPin(a1)				digitalRead(a1)							// a1: pin number
#define hal_GPIOInputPin(a1)			pinMode(a1, INPUT)						// a1: pin number
#define hal_GPIOOutputPin(a1)			pinMode(a1, OUTPUT)						// a1: pin number
// ISR
#define hal_GPIOISRRisingEdge(a1, a2)	wiringPiISR(a1, INT_EDGE_RISING, a2)	// a1: pin number, a2: pointer to function
#define hal_GPIOISRFallingEdge(a1, a2)	wiringPiISR(a1, INT_EDGE_FALLING, a2)	// a1: pin number, a2: pointer to function
#define hal_GPIOISRAnyEdge(a1, a2)		wiringPiISR(a1, INT_EDGE_BOTH, a2)		// a1: pin number, a2: pointer to function
//
#define hal_delay_ms(a1)				usleep(1000*a1)
#define hal_delay_us(a1)				usleep(a1)
#define hal_delay_ns(a1) \
    { \
        unsigned char a1_div_16 = a1/16; \
		while (a1_div_16 > 0) \
			a1_div_16 = a1_div_16 - 1; \
    }
	
