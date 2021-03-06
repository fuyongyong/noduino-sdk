/*
 *  Copyright (c) 2016 - 2026 MaiKe Labs
 *
 *  Library for SHT21/SHT2x digital Humidity and Temperature sensor
 *
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
*/
#include "noduino.h"
#include "sht2x.h"

/*
 * Give this function the 2 byte message (measurement) and the check_value byte from the SHT20
 * If it returns 0, then the transmission was good
 * If it returns something other than 0, then the communication was corrupted
 * From: http://www.nongnu.org/avr-libc/user-manual/group__util__crc.html
 * POLYNOMIAL = 0x0131 = x^8 + x^5 + x^4 + 1 : http://en.wikipedia.org/wiki/Computation_of_cyclic_redundancy_checks
 */
#define SHIFTED_DIVISOR 0x988000	//This is the 0x0131 polynomial shifted to farthest left of three bytes

irom static uint8_t check_crc(uint16_t message_from_sensor,
			 uint8_t check_value_from_sensor)
{
	//Test cases from datasheet:
	//message = 0xDC, checkvalue is 0x79
	//message = 0x683A, checkvalue is 0x7C
	//message = 0x4E85, checkvalue is 0x6B

	//Pad with 8 bits because we have to add in the check value
	uint32_t remainder = (uint32_t) message_from_sensor << 8;
	remainder |= check_value_from_sensor;	//Add on the check value

	uint32_t divsor = (uint32_t) SHIFTED_DIVISOR;

	//Operate on only 16 positions of max 24. The remaining 8 are our remainder and should be zero when we're done.
	for (int i = 0; i < 16; i++)
	{
		if (remainder & (uint32_t) 1 << (23 - i))	//Check if there is a one in the left position
			remainder ^= divsor;

		divsor >>= 1;	//Rotate the divsor max 16 times so that we have 8 bits left of a remainder
	}

	return (uint8_t) remainder;
}

irom int sht2x_read_reg(void)
{
	uint8_t reg;

	//Request the user register
	wire_beginTransmission(SHT2X_ADDR);
	wire_write(READ_USER_REG);
	wire_endTransmission();

	delay(85);

	//Read result
	if (wire_requestFrom(SHT2X_ADDR, 1) < 1) {
		return -1;
	}

	reg = wire_read();

	return(reg);  
}

irom uint8_t sht2x_write_reg(uint8_t val)
{
	uint8_t reg;
	int ret;

	//Request the user register
	wire_beginTransmission(SHT2X_ADDR);
	wire_write(WRITE_USER_REG);
	wire_write(val);
	ret = wire_endTransmission();

	delay(85);
	return ret;
}

irom uint8_t sht2x_reset(void)
{
	uint8_t ret;

	//Request the user register
	wire_beginTransmission(SHT2X_ADDR);
	wire_write(SOFT_RESET);
	ret = wire_endTransmission();

	delay(15);
	return ret;
}

/*
 * Sets the sensor resolution to one of four levels
 * Page 12:
 *  0x00 = 12bit RH, 14bit Temp
 *  0x01 = 8bit RH, 12bit Temp
 *  0x80 = 10bit RH, 13bit Temp
 *  0x81 = 11bit RH, 11bit Temp
 * Power on default is 0/0
 */
irom void sht2x_setResolution(uint8_t resolution)
{
	uint8_t reg = sht2x_read_reg();		//Go get the current register state
	reg &= 0x7E;						//Turn off the resolution bits
	resolution &= 0x81;					//Turn off all other bits but resolution bits
	reg |= resolution;					//Mask in the requested resolution bits

	sht2x_write_reg(reg);
}

irom static uint16_t sht2x_readSensor(uint8_t command)
{
	uint16_t result;
	int ret;

	wire_beginTransmission(SHT2X_ADDR);	//begin
	wire_write(command);				//send the pointer location
	ret = wire_endTransmission();		//end

	delay(85);

	wire_requestFrom(SHT2X_ADDR, 3);

	int count = 0;
	while (wire_available() < 3) {
		count++;
		delay(1);
		if (count > 1000) {
			serial_printf("SHT2x: wire request(read) timeout!\r\n");
			return 1;
		}
	}

	uint8_t checksum;
	//Store the result
	result = ((wire_read()) << 8);
	result += wire_read();
	checksum = wire_read();

	if (check_crc(result, checksum) != 0) {
		serial_printf("SHT2x: crc error!\r\n");
		return 2;	//Error out
	}

	result &= ~0x0003;	// clear two low bits (status bits)
	return result;
}

/*
 * Gets the current humidity from the sensor.
 * @return float - The relative humidity in %RH
 */
irom float sht2x_GetHumidity(void)
{
	return (-6.0 +
		125.0 / 65536.0 * (float)(sht2x_readSensor(RH_NO_HOLD_CMD)));
}

/*
 * Gets the current temperature from the sensor.
 * @return float - The temperature in Deg C
 */
irom float sht2x_GetTemperature(void)
{
	return (-46.85 +
		175.72 / 65536.0 * (float)(sht2x_readSensor(T_NO_HOLD_CMD)));
}
