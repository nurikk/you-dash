// #include <avr/io.h>
// #include <avr/interrupt.h>
// #include <avr/signal.h>

#include <Wire.h>

#define ADDR 0x00
void I2C_init(void)
{
	Wire.begin();
}

// Arbitration becomes host and sends START signal
// return value: 1 for success, 0 for failure
uint8_t I2C_start(void)
{
	Wire.beginTransmission(ADDR);
}

// stop communication and send STOP signal)`
void I2C_stop(void)
{
	Wire.endTransmission();
}

// Issued from the machine address and write command, that is, SLA + W, into the MT mode
// return value: 1 for success, 0 for failure
uint8_t I2C_to_write(void)
{
}

// send data to the slave
// return value: 0 for failure, 1 for ACK, 2 for NOT ACK
uint8_t I2C_send(uint8_t data)
{
	Wire.write(data);
}
