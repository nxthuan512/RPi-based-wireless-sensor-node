#include <stdint.h>


// *******************************************************************************************
// Define the RF pins <-> RPCM pin
// *******************************************************************************************
#define AT86RF212_RST		(23)	// Reset pin connects with GPIO 23 in RPCM	- O
#define AT86RF212_DIG2		(22)	// Miscellaneous pin connects with GPIO 22 in RPCM - I
#define AT86RF212_IRQ		(21)	// Interrupt pin connects with GPIO 21 in RPCM - I
#define AT86RF212_IRQ_STR	("21")	// String form
#define AT86RF212_SLPTR		(20)	// Sleep Transmit pin connects with GPIO 20 in RPCM - O

#define AT86RF212_EN		(16)

