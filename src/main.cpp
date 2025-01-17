/*
 * This sketch shows how nicla can be used in standalone mode.
 * Without the need for an host, nicla can run sketches that
 * are able to configure the bhi sensors and are able to read all
 * the bhi sensors data.
*/

#include "Arduino.h"
#include "sensor_stream.h"

#include "Arduino_BHY2.h"
#include "Nicla_System.h"   //must be placed after "ArduinoJSon.h"

#if USE_BLE
#include "ArduinoBLE.h"

const char* nameOfPeripheral = "Nicla Sense ME";
const char* uuidOfService    = "16480000-0525-4ad5-b4fb-6dd83f49546b";
const char* uuidOfConfigChar = "16480001-0525-4ad5-b4fb-6dd83f49546b";
const char* uuidOfDataChar   = "16480002-0525-4ad5-b4fb-6dd83f49546b";

const bool WRITE_BUFFER_FIXED_LENGTH = false;

// BLE Service
static
BLEService sensorColService(uuidOfService);

// RX / TX Characteristics
BLECharacteristic configChar(uuidOfConfigChar,
                             BLERead,
                             WRITE_BUFFER_SIZE,
                             WRITE_BUFFER_FIXED_LENGTH);
BLEDescriptor     configNameDescriptor("2901", "Sensor Configuration");
BLECharacteristic sensorDataChar(uuidOfDataChar,
                                 BLERead | BLENotify,
                                 20,
                                 WRITE_BUFFER_FIXED_LENGTH);
BLEDescriptor     sensorDataDescriptor("2901", "Sensor Data TX");
#endif  // USE_BLE

static int8_t json_cfg_buffer[WRITE_BUFFER_SIZE];

static unsigned long previousMs;
static uint16_t interval = 1000 / ODR_IMU;
static bool config_received = false;
static bool ble_connected;
static
uint8_t column_index = 0;

static unsigned long ledSetTimeMs;
uint8_t ledColor = red;
bool led_is_on;

DynamicJsonDocument config_message(256);
#if USE_SECOND_SERIAL_PORT_FOR_OUTPUT
auto& dataOutSerial = Serial1;
#else
auto& dataOutSerial = Serial;
#endif //USE_SECOND_SERIAL_PORT_FOR_OUTPUT

static void sendJsonConfig()
{
#if USE_BLE
    serializeJson(config_message, json_cfg_buffer, WRITE_BUFFER_SIZE);
    configChar.writeValue(json_cfg_buffer, WRITE_BUFFER_SIZE);
#else
    serializeJson(config_message, (void *)json_cfg_buffer, WRITE_BUFFER_SIZE);
    dataOutSerial.println((char*)json_cfg_buffer);
    dataOutSerial.flush();
#endif  // USE_BLE
}

void setLedColor(uint32_t color, unsigned long time)
{
    nicla::leds.setColor((RGBColors) color);
    ledSetTimeMs = time;

    led_is_on = (color != off);
}

void ledOff(unsigned long time)
{
    setLedColor(off, time);
}

void connectedLight()
{
#if USE_BLE
    ledColor = green;
#else
    ledColor = yellow;
#endif

    unsigned long time = millis();
    setLedColor(ledColor, time);
    delay(1000);
    ledOff(time + 1000);

}

void disconnectedLight()
{
    unsigned long time = millis();
    setLedColor(blue, time);
    ledColor = blue;
}

uint32_t get_free_memory_size()
{
    const uint32_t START = 16 * 1024;
    uint8_t *mem;
    uint32_t size = START;
    uint32_t floor = 0;

    while (size > 0) {
        mem = (uint8_t *) malloc (size);
        if (mem != NULL) {
            free(mem);
            floor = size;
            size = (size << 1);
            break;
        } else {
            size = size >> 1;
        }
    }

    while (size > floor) {
        mem = (uint8_t *) malloc (size);
        if (mem != NULL) {
            free(mem);
            break;
        } else {
            size--;
        }
    }

    Serial.print("free mem size: ");
    Serial.println(String(size));
    return size;
}

#if USE_BLE
/*
 * LEDS
 */

void onBLEConnected(BLEDevice central)
{
    Serial.print("Connected event, central: ");
    Serial.println(central.address());
    connectedLight();
    ble_connected = true;
}

void onBLEDisconnected(BLEDevice central)
{
    Serial.print("Disconnected event, central: ");
    Serial.println(central.address());
    disconnectedLight();
    BLE.setConnectable(true);
    ble_connected = false;
}


static void setup_ble()
{
    if (!BLE.begin())
    {
        Serial.println("starting BLE failed!");
        while (1)
            ;
    }

    BLE.setLocalName(nameOfPeripheral);
    BLE.setAdvertisedService(sensorColService);
    //BLE.setConnectionInterval(0x0006, 0x0007);  // 1.25 to 2.5ms
    //BLE.noDebug();

    configChar.addDescriptor(configNameDescriptor);
    sensorDataChar.addDescriptor(sensorDataDescriptor);
    sensorColService.addCharacteristic(configChar);
    sensorColService.addCharacteristic(sensorDataChar);

    delay(1000);
    BLE.addService(sensorColService);

    // Bluetooth LE connection handlers.
    BLE.setEventHandler(BLEConnected, onBLEConnected);
    BLE.setEventHandler(BLEDisconnected, onBLEDisconnected);

    BLE.advertise();

    Serial.println("Bluetooth device active, waiting for connections...");
}
#endif  //#if USE_BLE


//void checkSensorCfg()
//{
//  struct bhy2_virt_sensor_conf cfg;
//  
//  memset(&cfg, 0, sizeof(cfg));
//  BHY2.getSensorCfg(SENSOR_ID_ACC_RAW, &cfg);  
//  Serial.println("acc:");Serial.println(String(cfg.sensitivity));Serial.println(String(cfg.range));Serial.println(String(cfg.sample_rate));
//
//  
//  memset(&cfg, 0, sizeof(cfg));
//  BHY2.getSensorCfg(SENSOR_ID_GYRO_RAW, &cfg);
//  Serial.println("gyr:");Serial.println(String(cfg.sensitivity));Serial.println(String(cfg.range));Serial.println(String(cfg.sample_rate));
//
//  memset(&cfg, 0, sizeof(cfg));
//  BHY2.getSensorCfg(SENSOR_ID_TEMP, &cfg);
//  Serial.println("temp:");Serial.println(String(cfg.sensitivity));Serial.println(String(cfg.range));Serial.println(String(cfg.sample_rate));
//
//  memset(&cfg, 0, sizeof(cfg));
//  BHY2.getSensorCfg(SENSOR_ID_BARO, &cfg);
//  Serial.println("BARO:");Serial.println(String(cfg.sensitivity));Serial.println(String(cfg.range));Serial.println(String(cfg.sample_rate));
//}


void setup()
{
#if USE_BLE
  Serial.begin(SERIAL_BAUD_RATE_DEFAULT);
#else
  Serial.begin(SERIAL_BAUD_RATE);
#endif
  while(!Serial);

  NiclaSettings niclaSettings(NICLA_I2C, 0, NICLA_VIA_ESLOV, 0);
  BHY2.begin(niclaSettings);

#if USE_BLE
    setup_ble();
#endif  // USE_BLE

#if ENABLE_ACCEL || ENABLE_GYRO || ENABLE_MAG || ENABLE_GAS || ENABLE_TEMP
    column_index = setup_sensors(config_message, column_index);
#endif
    config_message["samples_per_packet"] = MAX_SAMPLES_PER_PACKET;

    delay(1000);
    sendJsonConfig();

#if DEBUG
    //checkSensorCfg();
#endif
    get_free_memory_size();
}


static uint8_t packetNum      = 0;
static uint8_t sensorRawIndex = 0;

void updateLED(unsigned long currentMs)
{
    if (ble_connected) {
        return; //to minimize delay
    }

    if (led_is_on) {
        if ((int32_t)currentMs - (int32_t)ledSetTimeMs >= CFG_LED_ON_DURATION) {
            ledOff(currentMs);
        }
    } else {
        if ((int32_t)currentMs - (int32_t)ledSetTimeMs >= CFG_LED_OFF_DURATION) {
            setLedColor(ledColor, currentMs);
        }
    }
}

void loop()
{
    unsigned long currentMs;
    bool connected_to_host = false;

    BHY2.update();

    currentMs = millis();
#if USE_BLE
    BLEDevice central = BLE.central();
    if (central)
    {
        if (central.connected())
        {
            //connectedLight();
        }
    }
    else
    {
        //disconnectedLight();
    }

    if (ble_connected) {
        connected_to_host = true;
    }
#else
    if (!config_received)
    {
        sendJsonConfig();
        delay(DELAY_BRDCAST_JSON_DESC_SENSOR_CONFIG);
        if (dataOutSerial.available() > 0)
        {
            String rx = dataOutSerial.readString();
            Serial.println(rx);
            if (rx.equals("connect") || rx.equals("cnnect"))
            {
#if USE_SECOND_SERIAL_PORT_FOR_OUTPUT
                Serial.println("Got Connect message");
#else
                connectedLight();
#endif
                config_received = true;
            }
            else
            {
            }
        }

    }
    else
    {
        if (dataOutSerial.available() > 0)
        {
            String rx = dataOutSerial.readString();
            if( rx.equals("disconnect"))
            {
                config_received = false;
                disconnectedLight();
            }
        }
    }

    if (config_received)
    {
        connected_to_host = true;
    }
#endif

    if (connected_to_host)
    {
        //CHECKME
        if ((int32_t)currentMs - (int32_t)previousMs >= interval)
        {
            // save the last time you blinked the LED
            previousMs = currentMs;
#if ENABLE_ACCEL || ENABLE_GYRO || ENABLE_MAG
            sensorRawIndex = update_sensor_data_col(sensorRawIndex);
            packetNum++;
            int16_t* pData = get_sensor_data_buffer();
#if USE_BLE
            sensorDataChar.writeValue((void*) pData, sensorRawIndex * sizeof(int16_t));
            sensorRawIndex = 0;
            memset(pData, 0, MAX_NUMBER_OF_COLUMNS * MAX_SAMPLES_PER_PACKET * sizeof(int16_t));
#else
            if (packetNum == MAX_SAMPLES_PER_PACKET)
            {
                dataOutSerial.write((uint8_t*) pData, sensorRawIndex * sizeof(int16_t));
                dataOutSerial.flush();
                sensorRawIndex = 0;
                memset(pData, 0, MAX_NUMBER_OF_COLUMNS * MAX_SAMPLES_PER_PACKET * sizeof(int16_t));
                packetNum = 0;
            }
#endif  // USE_BLE

#endif  //#if ENABLE_ACCEL || ENABLE_GYRO || ENABLE_MAG
        }
    }

    updateLED(currentMs);
}
