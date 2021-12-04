#include "Watchy.h"
#include <Arduino.h>
#include "ESP32Time.h"

ESP32Time rtc;

hw_timer_t * timer = NULL;
volatile byte state = LOW;



int scanAdress();

Watchy watch;
 
GxEPD2_BW<GxEPD2_154_D67, GxEPD2_154_D67::HEIGHT> Watchy::display(GxEPD2_154_D67(CS, DC, RESET, BUSY));

RTC_DATA_ATTR int guiState;
RTC_DATA_ATTR int menuIndex;
RTC_DATA_ATTR int SensorIndex;
RTC_DATA_ATTR BMA423 sensor;
RTC_DATA_ATTR bool WIFI_CONFIGURED;
RTC_DATA_ATTR bool BLE_CONFIGURED;
RTC_DATA_ATTR weatherData currentWeather;
RTC_DATA_ATTR int weatherIntervalCounter = WEATHER_UPDATE_INTERVAL;



String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

Watchy::Watchy(){} //constructor

void Watchy::init(String datetime){
    Serial.begin(9600);
    esp_sleep_wakeup_cause_t wakeup_reason;
    wakeup_reason = esp_sleep_get_wakeup_cause(); //get wake up reason
    Wire.begin(DIN, SCK); //init i2c

    switch (wakeup_reason)
    {
        case ESP_SLEEP_WAKEUP_TIMER: //ESP Internal RTC
            if(guiState == WATCHFACE_STATE){
                showWatchFace(true); //partial updates on tick
            }
            break;        
        case ESP_SLEEP_WAKEUP_EXT1: //button Press
            handleButtonPress();
            break;
        default: //reset
            _bmaConfig();
            showWatchFace(false); //full update on reset
            break;
    }
    deepSleep();
}

void Watchy::deepSleep(){
  esp_sleep_enable_timer_wakeup(15000000);
  esp_sleep_enable_ext1_wakeup(BTN_PIN_MASK, ESP_EXT1_WAKEUP_ANY_HIGH); //enable deep sleep wake on button press
  esp_deep_sleep_start();
}


void Watchy::_rtcConfig(String datetime){
       
}

void IRAM_ATTR onTimer(){
    state = !state;
}

void Watchy::handleButtonPress(){
  uint64_t wakeupBit = esp_sleep_get_ext1_wakeup_status();
  //Menu Button
  if (wakeupBit & MENU_BTN_MASK){
    if(guiState == WATCHFACE_STATE){//enter menu state if coming from watch face
      showMenu(menuIndex, false);
    }else if(guiState == MAIN_MENU_STATE){//if already in menu, then select menu item
      switch(menuIndex)
      {
        case 0:
          showBattery();
          break;
        case 1:
          scanSensor();
          break;          
        case 2:
          showSensorMenu(menuIndex, false);
          break;
        case 3:
          setTime();
          break;
        case 4:
          setupWifi();
          break;                    
        case 5:
          showUpdateFW();
          break;
        default:
          break;                              
      }
    }else if(guiState == FW_UPDATE_STATE){
      updateFWBegin();
    }else if(guiState == SD_MENU_STATE){
       switch(SensorIndex)
        {
        case 0:
        showHeartrate();
        break;
        case 1:
        showAccelerometer();
        break;          
        case 2:
        showTemp();
        break;
        default:
        break;
        }
    }
  }
  //Back Button
  else if (wakeupBit & BACK_BTN_MASK){
    if(guiState == MAIN_MENU_STATE){//exit to watch face if already in menu
      //RTC.alarm(ALARM_2); //resets the alarm flag in the RTC
      showWatchFace(false);
    }else if(guiState == APP_STATE){
      showMenu(menuIndex, false);//exit to menu if already in app
    }else if(guiState == FW_UPDATE_STATE){
      showMenu(menuIndex, false);//exit to menu if already in app
    }else if(guiState == SD_MENU_STATE){
      showFastMenu(menuIndex);//exit to menu if already in app
    }   
  }
  //Up Button
  else if (wakeupBit & UP_BTN_MASK){
    if(guiState == MAIN_MENU_STATE){                      
      menuIndex--;
      if(menuIndex < 0){
        menuIndex = MENU_LENGTH - 1;
      }    
      showMenu(menuIndex, true);
    }
    else if(guiState == SD_MENU_STATE){
      SensorIndex--;
      if(SensorIndex < 0){
        SensorIndex = SENSOR_MENU_LENGTH - 1;
      }      
      showSensorMenu(SensorIndex, false);
      }
    }

  //Down Button
  else if (wakeupBit & DOWN_BTN_MASK){
    if(guiState == MAIN_MENU_STATE){                      
      menuIndex++;
      if(menuIndex < MENU_LENGTH - 1){
        menuIndex = 0;
      }    
      showMenu(menuIndex, true);
    }
    else if(guiState == SD_MENU_STATE){
      SensorIndex++;
      if(SensorIndex < SENSOR_MENU_LENGTH - 1){
        SensorIndex = 0;
      }      
      showSensorMenu(SensorIndex, false);
      }
  }
  
  /***************** fast menu *****************/
  bool timeout = false;
  long lastTimeout = millis();
  pinMode(MENU_BTN_PIN, INPUT_PULLDOWN);
  pinMode(BACK_BTN_PIN, INPUT_PULLDOWN);
  pinMode(UP_BTN_PIN, INPUT_PULLDOWN);
  pinMode(DOWN_BTN_PIN, INPUT_PULLDOWN);
  while(!timeout){
      if(millis() - lastTimeout > 5000){
          timeout = true;
      }else{
          if(digitalRead(MENU_BTN_PIN) == 1){
            lastTimeout = millis();  
            if(guiState == MAIN_MENU_STATE){//if already in menu, then select menu item
                switch(menuIndex)
                {
                    case 0:
                    showBattery();
                    break;
                    case 1:
                    scanSensor();
                    break;          
                    case 2:
                    showSensorMenu(0, false);
                    break;
                    case 3:
                    setTime();
                    break;
                    case 4:
                    setupWifi();
                    break;                    
                    case 5:
                    showUpdateFW();
                    break;
                    default:
                    break;                              
                }
            }else if(guiState == FW_UPDATE_STATE){
                updateFWBegin();
            }
            else if(guiState == SD_MENU_STATE){
                switch(SensorIndex)
                {
                    case 0:
                    showHeartrate();
                    break;
                    case 1:
                    showAccelerometer();
                    break;          
                    case 2:
                    showTemp();
                    break;
                    default:
                    break;
                }
            }
          }else if(digitalRead(BACK_BTN_PIN) == 1){
            lastTimeout = millis();
            if(guiState == MAIN_MENU_STATE){//exit to watch face if already in menu
            //RTC.alarm(ALARM_2); //resets the alarm flag in the RTC
            showWatchFace(false);
            break; //leave loop
            }else if(guiState == APP_STATE){
            showMenu(menuIndex, false);//exit to menu if already in app
            }else if(guiState == FW_UPDATE_STATE){
            showMenu(menuIndex, false);//exit to menu if already in app
            }    
            else if(guiState == SD_MENU_STATE){
            showFastMenu(menuIndex);//exit to menu if already in app
            }        
          }else if(digitalRead(UP_BTN_PIN) == 1){
            lastTimeout = millis();
            if(guiState == MAIN_MENU_STATE){                      
            menuIndex--;
            if(menuIndex < 0){
            menuIndex = MENU_LENGTH - 1;
            }    
            showFastMenu(menuIndex);
            }
            else if(guiState == SD_MENU_STATE){
            SensorIndex--;
            if(SensorIndex < 0){
            SensorIndex = SENSOR_MENU_LENGTH - 1;
            }      
            showSensorMenu(SensorIndex, false);
            }

          }else if(digitalRead(DOWN_BTN_PIN) == 1){
            lastTimeout = millis();
            if(guiState == MAIN_MENU_STATE){                      
            menuIndex++;
            if(menuIndex > MENU_LENGTH - 1){
            menuIndex = 0;
            }    
            showFastMenu(menuIndex);
            }
            else if(guiState == SD_MENU_STATE){
            SensorIndex++;
            if(SensorIndex > SENSOR_MENU_LENGTH - 1){
            SensorIndex = 0;
            }      
            showSensorMenu(SensorIndex, false);
            }
          }
      }
  }
  display.hibernate();    
}

void Watchy::showMenu(byte menuIndex, bool partialRefresh){
    display.init(0, false); //_initial_refresh to false to prevent full update on init
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);

    int16_t  x1, y1;
    uint16_t w, h;
    int16_t yPos;

    const char *menuItems[] = {"Check Battery", "Scan Sensors", "Sensor Data", "Set Time", "Setup WiFi", "Update Firmware"};
    for(int i=0; i<MENU_LENGTH; i++){
    yPos = 30+(MENU_HEIGHT*i);
    display.setCursor(0, yPos);
    if(i == menuIndex){
        display.getTextBounds(menuItems[i], 0, yPos, &x1, &y1, &w, &h);
        display.fillRect(x1-1, y1-10, 200, h+15, GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.println(menuItems[i]);      
    }else{
        display.setTextColor(GxEPD_WHITE);
        display.println(menuItems[i]);
    }   
    }

    display.display(partialRefresh);
    display.hibernate();

    guiState = MAIN_MENU_STATE;    
}

void Watchy::showFastMenu(byte menuIndex){
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);

    int16_t  x1, y1;
    uint16_t w, h;
    int16_t yPos;

    const char *menuItems[] = {"Check Battery", "Scan Sensors", "Sensor Data", "Set Time", "Setup WiFi", "Update Firmware"};
    for(int i=0; i<MENU_LENGTH; i++){
    yPos = 30+(MENU_HEIGHT*i);
    display.setCursor(0, yPos);
    if(i == menuIndex){
        display.getTextBounds(menuItems[i], 0, yPos, &x1, &y1, &w, &h);
        display.fillRect(x1-1, y1-10, 200, h+15, GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.println(menuItems[i]);      
    }else{
        display.setTextColor(GxEPD_WHITE);
        display.println(menuItems[i]);
    }   
    }

    display.display(true);

    guiState = MAIN_MENU_STATE;    
}

void Watchy::showSensorMenu(byte SensorIndex, bool partialRefresh){
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);

    int16_t  x1, y1;
    uint16_t w, h;
    int16_t yPos;

    const char *menuItems[] = {"Heartrate", "Accelerometer", "Temperature"};
    for(int i=0; i<SENSOR_MENU_LENGTH; i++){
    yPos = 30+(MENU_HEIGHT*i);
    display.setCursor(0, yPos);
    if(i == SensorIndex){
        display.getTextBounds(menuItems[i], 0, yPos, &x1, &y1, &w, &h);
        display.fillRect(x1-1, y1-10, 200, h+15, GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);
        display.println(menuItems[i]);      
    }else{
        display.setTextColor(GxEPD_WHITE);
        display.println(menuItems[i]);
    }   
    }

    display.display(true);

    guiState = SD_MENU_STATE;    
}

void Watchy::showBattery(){
    display.init(0, false); //_initial_refresh to false to prevent full update on init
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(20, 30);
    display.println("Battery Voltage:");
    float voltage = getBatteryVoltage();
    display.setCursor(70, 80);
    display.print(voltage);
    display.println("V");
    display.display(false); //full refresh
    display.hibernate();

    guiState = APP_STATE;      
}

void Watchy::scanSensor(){
    int nDevices = scanAdress();

    Wire.begin(DIN, SCK);

    display.init(0, false); //_initial_refresh to false to prevent full update on init
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(30, 80);
    display.println("Scan Sensors");
    
    Serial.print("Ende Scan\n");
    display.setCursor(30, 100);
    display.print("Sensors: ");
    display.println(nDevices);
    display.display(false); //full refresh
    display.hibernate();

    guiState = APP_STATE;
       
}

void Watchy::vibMotor(uint8_t intervalMs, uint8_t length){
    pinMode(VIB_MOTOR_PIN, OUTPUT);
    bool motorOn = false;
    for(int i=0; i<length; i++){
        motorOn = !motorOn;
        digitalWrite(VIB_MOTOR_PIN, motorOn);
        delay(intervalMs);
    }
}

void Watchy::showHeartrate(){
  
}

void Watchy::showAccelerometer(){
  
}

void Watchy::showTemp(){
  
}




void Watchy::setTime(){

    guiState = APP_STATE;


    int minute = 0;
    int hour = 0;
    int day = 0;
    int month = 0;
    int year = 2021;

    int8_t setIndex = SET_HOUR;

    int8_t blink = 0;

    pinMode(DOWN_BTN_PIN, INPUT_PULLUP);
    pinMode(UP_BTN_PIN, INPUT_PULLUP);
    pinMode(MENU_BTN_PIN, INPUT_PULLUP);  
    pinMode(BACK_BTN_PIN, INPUT_PULLUP);  

    display.init(0, true); //_initial_refresh to false to prevent full update on init
    display.setFullWindow();

    while(1){

    if(digitalRead(MENU_BTN_PIN) == 1){
        setIndex++;
        if(setIndex > SET_DAY){
        break;
        }
    }
    if(digitalRead(BACK_BTN_PIN) == 1){
        if(setIndex != SET_HOUR){
        setIndex--;
        }
    }      

    blink = 1 - blink;

    if(digitalRead(DOWN_BTN_PIN) == 1){
        blink = 1;
        switch(setIndex){
        case SET_HOUR:
            hour == 23 ? (hour = 0) : hour++;
            break;
        case SET_MINUTE:
            minute == 59 ? (minute = 0) : minute++;
            break;
        case SET_YEAR:
            year == 99 ? (year = 20) : year++;
            break;
        case SET_MONTH:
            month == 12 ? (month = 1) : month++;
            break;
        case SET_DAY:
            day == 31 ? (day = 1) : day++;
            break;                         
        default:
            break;
        }      
    }

    if(digitalRead(UP_BTN_PIN) == 1){
        blink = 1;
        switch(setIndex){
        case SET_HOUR:
            hour == 0 ? (hour = 23) : hour--;
            break;
        case SET_MINUTE:
            minute == 0 ? (minute = 59) : minute--;
            break;
        case SET_YEAR:
            year == 20 ? (year = 99) : year--;
            break;
        case SET_MONTH:
            month == 1 ? (month = 12) : month--;
            break;
        case SET_DAY:
            day == 1 ? (day = 31) : day--;
            break;          
        default:
            break;
        }   
    }    

    display.fillScreen(GxEPD_BLACK);
    display.setTextColor(GxEPD_WHITE);
    display.setFont(&DSEG7_Classic_Bold_53);

    display.setCursor(5, 80);
    if(setIndex == SET_HOUR){//blink hour digits
        display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
    }
    if(hour < 10){
        display.print("0");      
    }
    display.print(hour);

    display.setTextColor(GxEPD_WHITE);
    display.print(":");

    display.setCursor(108, 80);
    if(setIndex == SET_MINUTE){//blink minute digits
        display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
    }
    if(minute < 10){
        display.print("0");      
    }
    display.print(minute);

    display.setTextColor(GxEPD_WHITE);

    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(45, 150);
    if(setIndex == SET_YEAR){//blink minute digits
        display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
    }    
    display.print(year);

    display.setTextColor(GxEPD_WHITE);
    display.print("/");

    if(setIndex == SET_MONTH){//blink minute digits
        display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
    }   
    if(month < 10){
        display.print("0");      
    }     
    display.print(month);

    display.setTextColor(GxEPD_WHITE);
    display.print("/");

    if(setIndex == SET_DAY){//blink minute digits
        display.setTextColor(blink ? GxEPD_WHITE : GxEPD_BLACK);
    }       
    if(day < 10){
        display.print("0");      
    }     
    display.print(day); 
    display.display(true); //partial refresh
    }

    display.hibernate();

    rtc.setTime(0, minute, hour, day, month, year, 0);

    showMenu(menuIndex, false);

}

void Watchy::showWatchFace(bool partialRefresh){

  display.init(0, false); //_initial_refresh to false to prevent full update on init
  display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
  drawWatchFace();
  display.display(partialRefresh); //partial refresh
  display.hibernate();
  guiState = WATCHFACE_STATE;
}

void Watchy::drawWatchFace(){
    
    int hour = rtc.getHour(true);
    int minute = rtc.getMinute();
    
    display.setFont(&DSEG7_Classic_Bold_53);
    display.setCursor(5, 53+60);
    if(hour < 10){
        display.print("0");
    }
    display.print(hour);
    display.print(":");
    if(minute < 10){
        display.print("0");
    }  
    display.println(minute);    
}

weatherData Watchy::getWeatherData(){
    if(weatherIntervalCounter >= WEATHER_UPDATE_INTERVAL){ //only update if WEATHER_UPDATE_INTERVAL has elapsed i.e. 30 minutes
        if(connectWiFi()){//Use Weather API for live data if WiFi is connected
            HTTPClient http;
            http.setConnectTimeout(3000);//3 second max timeout
            String weatherQueryURL = String(OPENWEATHERMAP_URL) + String(CITY_NAME) + String(",") + String(COUNTRY_CODE) + String("&units=") + String(TEMP_UNIT) + String("&appid=") + String(OPENWEATHERMAP_APIKEY);
            http.begin(weatherQueryURL.c_str());
            int httpResponseCode = http.GET();
            if(httpResponseCode == 200) {
                String payload = http.getString();
                JSONVar responseObject = JSON.parse(payload);
                currentWeather.temperature = int(responseObject["main"]["temp"]);
                currentWeather.weatherConditionCode = int(responseObject["weather"][0]["id"]);            
            }else{
                //http error
            }
            http.end();
            //turn off radios
            WiFi.mode(WIFI_OFF);
            btStop();
        }else{//No WiFi, use RTC Temperature
            //uint8_t temperature = RTC.temperature() / 4; //celsius
            if(strcmp(TEMP_UNIT, "imperial") == 0){
                //temperature = temperature * 9. / 5. + 32.; //fahrenheit
            }
            //currentWeather.temperature = temperature;
            currentWeather.weatherConditionCode = 800;
        }
        weatherIntervalCounter = 0;
    }else{
        weatherIntervalCounter++;
    }
    return currentWeather;
}

float Watchy::getBatteryVoltage(){
    // Battery voltage goes through a 1/2 divider.
    return analogReadMilliVolts(ADC_PIN) / 1000.0f;
}

uint16_t Watchy::_readRegister(uint8_t address, uint8_t reg, uint8_t *data, uint16_t len)
{
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.endTransmission();
    Wire.requestFrom((uint8_t)address, (uint8_t)len);
    uint8_t i = 0;
    while (Wire.available()) {
        data[i++] = Wire.read();
    }
    return 0;
}

uint16_t Watchy::_writeRegister(uint8_t address, uint8_t reg, uint8_t *data, uint16_t len)
{
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(data, len);
    return (0 !=  Wire.endTransmission());
}

void Watchy::_bmaConfig(){
 
    if (sensor.begin(_readRegister, _writeRegister, delay) == false) {
        //fail to init BMA
        return;
    }

    // Accel parameter structure
    Acfg cfg;
    /*!
        Output data rate in Hz, Optional parameters:
            - BMA4_OUTPUT_DATA_RATE_0_78HZ
            - BMA4_OUTPUT_DATA_RATE_1_56HZ
            - BMA4_OUTPUT_DATA_RATE_3_12HZ
            - BMA4_OUTPUT_DATA_RATE_6_25HZ
            - BMA4_OUTPUT_DATA_RATE_12_5HZ
            - BMA4_OUTPUT_DATA_RATE_25HZ
            - BMA4_OUTPUT_DATA_RATE_50HZ
            - BMA4_OUTPUT_DATA_RATE_100HZ
            - BMA4_OUTPUT_DATA_RATE_200HZ
            - BMA4_OUTPUT_DATA_RATE_400HZ
            - BMA4_OUTPUT_DATA_RATE_800HZ
            - BMA4_OUTPUT_DATA_RATE_1600HZ
    */
    cfg.odr = BMA4_OUTPUT_DATA_RATE_100HZ;
    /*!
        G-range, Optional parameters:
            - BMA4_ACCEL_RANGE_2G
            - BMA4_ACCEL_RANGE_4G
            - BMA4_ACCEL_RANGE_8G
            - BMA4_ACCEL_RANGE_16G
    */
    cfg.range = BMA4_ACCEL_RANGE_2G;
    /*!
        Bandwidth parameter, determines filter configuration, Optional parameters:
            - BMA4_ACCEL_OSR4_AVG1
            - BMA4_ACCEL_OSR2_AVG2
            - BMA4_ACCEL_NORMAL_AVG4
            - BMA4_ACCEL_CIC_AVG8
            - BMA4_ACCEL_RES_AVG16
            - BMA4_ACCEL_RES_AVG32
            - BMA4_ACCEL_RES_AVG64
            - BMA4_ACCEL_RES_AVG128
    */
    cfg.bandwidth = BMA4_ACCEL_NORMAL_AVG4;

    /*! Filter performance mode , Optional parameters:
        - BMA4_CIC_AVG_MODE
        - BMA4_CONTINUOUS_MODE
    */
    cfg.perf_mode = BMA4_CONTINUOUS_MODE;

    // Configure the BMA423 accelerometer
    sensor.setAccelConfig(cfg);

    // Enable BMA423 accelerometer
    // Warning : Need to use feature, you must first enable the accelerometer
    // Warning : Need to use feature, you must first enable the accelerometer
    sensor.enableAccel();

    struct bma4_int_pin_config config ;
    config.edge_ctrl = BMA4_LEVEL_TRIGGER;
    config.lvl = BMA4_ACTIVE_HIGH;
    config.od = BMA4_PUSH_PULL;
    config.output_en = BMA4_OUTPUT_ENABLE;
    config.input_en = BMA4_INPUT_DISABLE;
    // The correct trigger interrupt needs to be configured as needed
    sensor.setINTPinConfig(config, BMA4_INTR1_MAP);

    struct bma423_axes_remap remap_data;
    remap_data.x_axis = 1;
    remap_data.x_axis_sign = 0xFF;
    remap_data.y_axis = 0;
    remap_data.y_axis_sign = 0xFF;
    remap_data.z_axis = 2;
    remap_data.z_axis_sign = 0xFF;
    // Need to raise the wrist function, need to set the correct axis
    sensor.setRemapAxes(&remap_data);

    // Enable BMA423 isStepCounter feature
    sensor.enableFeature(BMA423_STEP_CNTR, true);
    // Enable BMA423 isTilt feature
    sensor.enableFeature(BMA423_TILT, true);
    // Enable BMA423 isDoubleClick feature
    sensor.enableFeature(BMA423_WAKEUP, true);

    // Reset steps
    sensor.resetStepCounter();

    // Turn on feature interrupt
    sensor.enableStepCountInterrupt();
    sensor.enableTiltInterrupt();
    // It corresponds to isDoubleClick interrupt
    sensor.enableWakeupInterrupt();  
}

void Watchy::setupWifi(){
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  wifiManager.setTimeout(WIFI_AP_TIMEOUT);
  wifiManager.setAPCallback(_configModeCallback);
  if(!wifiManager.autoConnect(WIFI_AP_SSID)) {//WiFi setup failed
    display.init(0, false); //_initial_refresh to false to prevent full update on init
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.println("Setup failed &");
    display.println("timed out!");
    display.display(false); //full refresh
    display.hibernate();
  }else{
    display.init(0, false);//_initial_refresh to false to prevent full update on init
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.println("Connected to");
    display.println(WiFi.SSID());
    display.display(false);//full refresh
    display.hibernate();
  }
  //turn off radios
  WiFi.mode(WIFI_OFF);
  btStop();

  guiState = APP_STATE;  
}

void Watchy::_configModeCallback (WiFiManager *myWiFiManager) {
  display.init(0, false); //_initial_refresh to false to prevent full update on init
  display.setFullWindow();
  display.fillScreen(GxEPD_BLACK);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_WHITE);
  display.setCursor(0, 30);
  display.println("Connect to");
  display.print("SSID: ");
  display.println(WIFI_AP_SSID);
  display.print("IP: ");
  display.println(WiFi.softAPIP());
  display.display(false); //full refresh
  display.hibernate();
}

bool Watchy::connectWiFi(){
    if(WL_CONNECT_FAILED == WiFi.begin()){//WiFi not setup, you can also use hard coded credentials with WiFi.begin(SSID,PASS);
        WIFI_CONFIGURED = false;
    }else{
        if(WL_CONNECTED == WiFi.waitForConnectResult()){//attempt to connect for 10s
            WIFI_CONFIGURED = true;
        }else{//connection failed, time out
            WIFI_CONFIGURED = false;
            //turn off radios
            WiFi.mode(WIFI_OFF);
            btStop();
        }
    }
    return WIFI_CONFIGURED;
}

void Watchy::showUpdateFW(){
    display.init(0, false); //_initial_refresh to false to prevent full update on init
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.println("Please visit");
    display.println("watchy.sqfmi.com");
    display.println("with a Bluetooth");
    display.println("enabled device");
    display.println(" ");
    display.println("Press menu button");
    display.println("again when ready");
    display.println(" ");
    display.println("Keep USB powered");
    display.display(false); //full refresh
    display.hibernate();

    guiState = FW_UPDATE_STATE;  
}

void Watchy::updateFWBegin(){
    display.init(0, false); //_initial_refresh to false to prevent full update on init
    display.setFullWindow();
    display.fillScreen(GxEPD_BLACK);
    display.setFont(&FreeMonoBold9pt7b);
    display.setTextColor(GxEPD_WHITE);
    display.setCursor(0, 30);
    display.println("Bluetooth Started");
    display.println(" ");
    display.println("Watchy BLE OTA");
    display.println(" ");
    display.println("Waiting for");
    display.println("connection...");
    display.display(false); //full refresh

    BLE BT;
    BT.begin("Watchy BLE OTA");
    int prevStatus = -1;
    int currentStatus;

    while(1){
    currentStatus = BT.updateStatus();
    if(prevStatus != currentStatus || prevStatus == 1){
        if(currentStatus == 0){
        display.setFullWindow();
        display.fillScreen(GxEPD_BLACK);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(0, 30);
        display.println("BLE Connected!");
        display.println(" ");
        display.println("Waiting for");
        display.println("upload...");
        display.display(false); //full refresh
        }
        if(currentStatus == 1){
        display.setFullWindow();
        display.fillScreen(GxEPD_BLACK);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(0, 30);
        display.println("Downloading");
        display.println("firmware:");
        display.println(" ");
        display.print(BT.howManyBytes());
        display.println(" bytes");
        display.display(true); //partial refresh        
        }
        if(currentStatus == 2){
        display.setFullWindow();
        display.fillScreen(GxEPD_BLACK);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(0, 30);
        display.println("Download");
        display.println("completed!");
        display.println(" ");
        display.println("Rebooting...");
        display.display(false); //full refresh

        delay(2000);
        esp_restart();           
        }
        if(currentStatus == 4){
        display.setFullWindow();
        display.fillScreen(GxEPD_BLACK);
        display.setFont(&FreeMonoBold9pt7b);
        display.setTextColor(GxEPD_WHITE);
        display.setCursor(0, 30);
        display.println("BLE Disconnected!");
        display.println(" ");
        display.println("exiting...");
        display.display(false); //full refresh
        delay(1000);
        break;
        }
        prevStatus = currentStatus;
    }
    delay(100);
    }

    //turn off radios
    WiFi.mode(WIFI_OFF);
    btStop();
    showMenu(menuIndex, false);
}

// time_t compileTime()
// {   
//     const time_t FUDGE(10);    //fudge factor to allow for upload time, etc. (seconds, YMMV)
//     const char *compDate = __DATE__, *compTime = __TIME__, *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
//     char compMon[3], *m;

//     strncpy(compMon, compDate, 3);
//     compMon[3] = '\0';
//     m = strstr(months, compMon);

//     tmElements_t tm;
//     tm.Month = ((m - months) / 3 + 1);
//     tm.Day = atoi(compDate + 4);
//     tm.Year = atoi(compDate + 7) - YEAR_OFFSET; // offset from 1970, since year is stored in uint8_t
//     tm.Hour = atoi(compTime);
//     tm.Minute = atoi(compTime + 3);
//     tm.Second = atoi(compTime + 6);

//     time_t t = makeTime(tm);
//     return t + FUDGE;        //add fudge factor to allow for compile time
// }



int scanAdress(){

    byte error, address;
    int nDevices;
    
    Wire.begin(SDA, SCL);
  
    Serial.println("\nI2C Scanner");
    Serial.println("Scanning...");

    nDevices = 0;
    for(address = 1; address < 127; address++ ) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();
        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address<16) {
                Serial.print("0");
            }
            Serial.println(address,HEX);
            nDevices++;
        }
        else if (error==4) {
            Serial.print("Unknow error at address 0x");
            if (address<16) {
                Serial.print("0");
            }
            Serial.println(address,HEX);
        }    
    }
    if (nDevices == 0) {
        Serial.println("No I2C devices found\n");
    }
    else {
        Serial.println("done\n");
    }
    return nDevices;
}


