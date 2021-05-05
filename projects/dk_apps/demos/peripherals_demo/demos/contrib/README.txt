THIRD PARTY DRIVERS
---------------------------------------
Peripherals demo can access devices from DA14680 Sensor Board.
Some sensors have drivers provided by third party, those drivers are not
included in SDK.
To test those sensors drivers must me downloaded from the internet.

BME280 - Combined humidity and pressure sensor
  to get driver needed for demo go to:

    https://github.com/BoschSensortec/BME280_driver

  download bme280.c and bme280.h and put them in contrib folder

  have project define CFG_DEMO_SENSOR_BME280 set to 1

 BMM150 - 3-axis digital geomagnetic sensor
  to get driver needed for demo go to:

    https://github.com/BoschSensortec/BMM050_driver

  download bmm050.c and bmm050.h and put them in contrib folder

  have project define CFG_DEMO_SENSOR_BMM150 set to 1

 BMG160 - triaxial gyroscope sensor
  to get driver needed for demo go to:

    https://github.com/BoschSensortec/BMG160_driver

  download bmg160.c and bmg160.h and put them in contrib folder

  have project define CFG_DEMO_SENSOR_BMG160 set to 1