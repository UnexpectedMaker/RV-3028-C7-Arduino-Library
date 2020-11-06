/*
  Arduino Library for RV-3028-C7

  Copyright (c) 2020 Macro Yau

  https://github.com/MacroYau/RV-3028-C7-Arduino-Library
*/

#include "RV3028C7.h"

RV3028C7::RV3028C7() {}

bool RV3028C7::begin(TwoWire &wirePort) {
  // Wire.begin() should be called in the application code in advance
  _i2cPort = &wirePort;

  // Reads RESET bit to pseudo-verify that the part is a RV-3028-C7
  uint8_t value = readByteFromRegister(REG_CONTROL_2);
  if (value & 0x01 == 0x00) {
    return true;
  } else {
    return false;
  }
}

char *RV3028C7::getCurrentDateTime() {
  // Updates RTC date time value to array
  readBytesFromRegisters(REG_CLOCK_SECONDS, _dateTime, DATETIME_COMPONENTS);

  // Returns ISO 8601 date time string
  static char iso8601[23];
  sprintf(iso8601, "20%02d-%02d-%02dT%02d:%02d:%02d",
          convertToDecimal(_dateTime[DATETIME_YEAR]),
          convertToDecimal(_dateTime[DATETIME_MONTH]),
          convertToDecimal(_dateTime[DATETIME_DAY_OF_MONTH]),
          convertToDecimal(_dateTime[DATETIME_HOUR]),
          convertToDecimal(_dateTime[DATETIME_MINUTE]),
          convertToDecimal(_dateTime[DATETIME_SECOND]));
  return iso8601;
}

void RV3028C7::setDateTimeFromISO8601(String iso8601) {
  return setDateTimeFromISO8601(iso8601.c_str());
}

void RV3028C7::setDateTimeFromISO8601(const char *iso8601) {
  // Assumes the input is in the format of "2018-01-01T08:00:00" (hundredths and
  // time zone, if applicable, will be neglected)
  char components[3] = {'0', '0', '\0'};
  uint8_t buffer[6];

  for (uint8_t i = 2, j = 0; i < 20; i += 3, j++) {
    components[0] = iso8601[i];
    components[1] = iso8601[i + 1];
    buffer[j] = atoi(components);
  }

  // Since ISO 8601 date string does not indicate day of week, it is set to 0
  // (Sunday) and is no longer correct
  setDateTime(/*year=*/2000 + buffer[0], /*month=*/buffer[1],
              /*dayOfMonth=*/buffer[2], /*dayOfWeek=*/SUN,
              /*hour=*/buffer[3], /*minute=*/buffer[4], /*second=*/buffer[5]);
}

void RV3028C7::setDateTimeFromHTTPHeader(String str) {
  return setDateTimeFromHTTPHeader(str.c_str());
}

void RV3028C7::setDateTimeFromHTTPHeader(const char *str) {
  char components[3] = {'0', '0', '\0'};

  // Checks whether the string begins with "Date: " prefix
  uint8_t counter = 0;
  if (str[0] == 'D') {
    counter = 6;
  }

  // Day of week
  uint8_t dayOfWeek = 0;
  if (str[counter] == 'T') {
    // Tue or Thu
    if (str[counter + 1] == 'u') {
      // Tue
      dayOfWeek = 2;
    } else {
      dayOfWeek = 4;
    }
  } else if (str[counter] == 'S') {
    // Sat or Sun
    if (str[counter + 1] == 'a') {
      dayOfWeek = 6;
    } else {
      dayOfWeek = 0;
    }
  } else if (str[counter] == 'M') {
    // Mon
    dayOfWeek = 1;
  } else if (str[counter] == 'W') {
    // Wed
    dayOfWeek = 3;
  } else {
    // Fri
    dayOfWeek = 5;
  }

  // Day of month
  counter += 5;
  components[0] = str[counter];
  components[1] = str[counter + 1];
  uint8_t dayOfMonth = atoi(components);

  // Month
  counter += 3;
  uint8_t month = 0;
  if (str[counter] == 'J') {
    // Jan, Jun, or Jul
    if (str[counter + 1] == 'a') {
      // Jan
      month = 1;
    } else if (str[counter + 2] == 'n') {
      // Jun
      month = 6;
    } else {
      // Jul
      month = 7;
    }
  } else if (str[counter] == 'F') {
    // Feb
    month = 2;
  } else if (str[counter] == 'M') {
    // Mar or May
    if (str[counter + 2] == 'r') {
      // Mar
      month = 3;
    } else {
      // May
      month = 5;
    }
  } else if (str[counter] == 'A') {
    // Apr or Aug
    if (str[counter + 1] == 'p') {
      // Apr
      month = 4;
    } else {
      // Aug
      month = 8;
    }
  } else if (str[counter] == 'S') {
    // Sep
    month = 9;
  } else if (str[counter] == 'O') {
    // Oct
    month = 10;
  } else if (str[counter] == 'N') {
    // Nov
    month = 11;
  } else {
    month = 12;
  }

  // Year
  counter += 6;
  components[0] = str[counter];
  components[1] = str[counter + 1];
  uint16_t year = 2000 + atoi(components);

  // Time of day
  counter += 3;
  uint8_t buffer[3];
  for (uint8_t i = counter, j = 0; j < 3; i += 3, j++) {
    components[0] = str[i];
    components[1] = str[i + 1];
    buffer[j] = atoi(components);
  }

  setDateTime(year, month, dayOfMonth, static_cast<DayOfWeek_t>(dayOfWeek),
              /*hour=*/buffer[0], /*minute=*/buffer[1], /*second=*/buffer[2]);
}

bool RV3028C7::setDateTime(uint16_t year, uint8_t month, uint8_t dayOfMonth,
                           DayOfWeek_t dayOfWeek, uint8_t hour, uint8_t minute,
                           uint8_t second) {
  // Year 2000 AD is the earliest allowed year in this implementation
  if (year < 2000) {
    return false;
  }
  // Century overflow is not considered yet (i.e., only supports year 2000 to
  // 2099)
  _dateTime[DATETIME_YEAR] = convertToBCD(year - 2000);

  if (month < 1 || month > 12) {
    return false;
  }
  _dateTime[DATETIME_MONTH] = convertToBCD(month);

  if (dayOfMonth < 1 || dayOfMonth > 31) {
    return false;
  }
  _dateTime[DATETIME_DAY_OF_MONTH] = convertToBCD(dayOfMonth);

  if (dayOfWeek > 6) {
    return false;
  }
  _dateTime[DATETIME_DAY_OF_WEEK] = convertToBCD(dayOfWeek);

  // Uses 24-hour notation by default
  if (hour > 23) {
    return false;
  }
  _dateTime[DATETIME_HOUR] = convertToBCD(hour);

  if (minute > 59) {
    return false;
  }
  _dateTime[DATETIME_MINUTE] = convertToBCD(minute);

  if (second > 59) {
    return false;
  }
  _dateTime[DATETIME_SECOND] = convertToBCD(second);

  return true;
}

void RV3028C7::setDateTimeComponent(DateTimeComponent_t component,
                                    uint8_t value) {
  // Updates RTC date time value to array
  readBytesFromRegisters(REG_CLOCK_SECONDS, _dateTime, DATETIME_COMPONENTS);

  _dateTime[component] = convertToBCD(value);
}

bool RV3028C7::synchronize() {
  return writeBytesToRegisters(REG_CLOCK_SECONDS, _dateTime,
                               DATETIME_COMPONENTS);
}

uint8_t RV3028C7::convertToDecimal(uint8_t bcd) {
  return (bcd / 16 * 10) + (bcd % 16);
}

uint8_t RV3028C7::convertToBCD(uint8_t decimal) {
  return (decimal / 10 * 16) + (decimal % 10);
}

bool RV3028C7::readBytesFromRegisters(uint8_t startAddress,
                                      uint8_t *destination, uint8_t length) {
  _i2cPort->beginTransmission(RV3028C7_ADDRESS);
  _i2cPort->write(startAddress);
  _i2cPort->endTransmission(false);

  _i2cPort->requestFrom((uint8_t)RV3028C7_ADDRESS, length);
  for (uint8_t i = 0; i < length; i++) {
    destination[i] = _i2cPort->read();
  }
  return (_i2cPort->endTransmission() == 0);
}

bool RV3028C7::writeBytesToRegisters(uint8_t startAddress, uint8_t *values,
                                     uint8_t length) {
  _i2cPort->beginTransmission(RV3028C7_ADDRESS);
  _i2cPort->write(startAddress);
  for (uint8_t i = 0; i < length; i++) {
    _i2cPort->write(values[i]);
  }
  return (_i2cPort->endTransmission() == 0);
}

uint8_t RV3028C7::readByteFromRegister(uint8_t address) {
  uint8_t value = 0;

  _i2cPort->beginTransmission(RV3028C7_ADDRESS);
  _i2cPort->write(address);
  _i2cPort->endTransmission(false);

  _i2cPort->requestFrom(RV3028C7_ADDRESS, 1);
  value = _i2cPort->read();
  _i2cPort->endTransmission();

  return value;
}

bool RV3028C7::writeByteToRegister(uint8_t address, uint8_t value) {
  _i2cPort->beginTransmission(RV3028C7_ADDRESS);
  _i2cPort->write(address);
  _i2cPort->write(value);
  return (_i2cPort->endTransmission() == 0);
}
