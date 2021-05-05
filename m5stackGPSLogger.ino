#define M5STACK_MPU6886
#define BIG_TEXT_SIZE 5
#define HEIGHT_UNIT 10
#define CHAR_WIDTH 6

#define DISPLAY_HEIGHT 240
#define DISPLAY_WIDTH 320

#define GPS_XML_TAG "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
#define GPX_START_TAG "<gpx version=\"1.1\" creator=\"m5stack gps logger\" \
xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns=\"http://www.topografix.com/GPX/1/1\" \
xsi:schemaLocation=\"http://www.topografix.com/GPX/1/1 http://www.topografix.com/GPX/1/1/gpx.xsd\">"
#define GPX_END_TAG "</gpx>"

#include <M5Stack.h>

#include <TinyGPS++.h>

TFT_eSprite spr = TFT_eSprite(&M5.Lcd);

TinyGPSPlus gps;
// serial connection for gps
HardwareSerial ss(2);

// for gps logger
File f;
bool isLogging = false;
bool enableLogging = true;
char logfilename[32] = "/unlogging.dat";

TaskHandle_t gpsLoggingTaskHandler = NULL;

// for lap timer
bool isLapTimerOn = false;
struct TIME
{
  uint8_t hour;
  uint8_t minute;
  uint8_t second;
  uint8_t centisecond;
};
uint8_t lapNum;
TIME times[100];
TIME laps[100];
uint8_t fastestLap;

bool LapBtnLongPressed = false;

bool gpsLapBtnLongPressed = false;
bool gpsLapTimerEnable = true;

double lat = 0, lng = 0, altitude = 0, kmph = 0;
// double deg = 0;
uint32_t sat = 0, age = 0;
uint16_t hdop = 0;
uint16_t year = 0;
uint8_t month = 0, day = 0;
TIME gpsTime = {0, 0, 0, 0};

// gps lap timer
double targetLat = 0.0;
double targetLng = 0.0;
double targetDistanceMeter = 999;
uint8_t distanceThreatholdMeter = 30;
bool isInArea = false;

char config_file_path[] = "/config.txt";

void setup()
{
  M5.begin();
  M5.Power.begin();
  M5.IMU.Init();

  // mute noise
  M5.Speaker.begin();
  M5.Speaker.mute();

  startSerial();
  startGPS(115200);

  initSprite();
  loadConfig();
  drawLoading();
  resetLap();

  // create gps logging task
  xTaskCreatePinnedToCore(updateGPSTask, "updateGPSTask", 4096, NULL, 10, &gpsLoggingTaskHandler, tskNO_AFFINITY);
}

void loop()
{
  M5.update();
  fillTFT();

  drawDateTime(HEIGHT_UNIT * 0);
  drawSatellitesInfo(HEIGHT_UNIT * 0);
  drawLatLng(HEIGHT_UNIT * 1);
  drawSpeed(HEIGHT_UNIT * 2);
  drawTimer(HEIGHT_UNIT * 2 + BIG_TEXT_SIZE * HEIGHT_UNIT * 1);
  drawLoggingStatus(DISPLAY_HEIGHT - HEIGHT_UNIT * 2);
  buttonCheck(DISPLAY_HEIGHT - HEIGHT_UNIT * 1);

  flushSprite();
  delay(30);
}

void startSerial()
{
  Serial.begin(115200);
  delay(20);
  Serial.println(F("Start"));
}

void startGPS(int baud)
{
  ss.begin(baud);
}

void updateGPSTask(void *args)
{
  while (1)
  {
    update_gps();
    delay(5);
  }
}

void logGPS()
{
  // logging gpx
  char str[200];
  sprintf(str, "<wpt lat=\"%9.6f\" lon=\"%9.6f\"><ele>%.1f</ele><time>%04d-%02d-%02dT%02d:%02d:%02d.%dZ</time><sat>%d</sat><hdop>%d</hdop></wpt>",
          lat, lng, altitude, year, month, day, gpsTime.hour, gpsTime.minute, gpsTime.second, gpsTime.centisecond, sat, hdop);
  Serial.println(str);

  if (!f || !enableLogging)
  {
    return;
  }
  // </gpx>\n\0 = 8 chars
  if (!f.seek(f.position() - 8))
  {
    // seek miss
    return;
  }

  f.println(str);
  f.println(F(GPX_END_TAG));
  f.flush();

  char buf[40];
  sprintf(buf, "logged! %f", targetDistanceMeter);
  Serial.println(buf);
}

void fillTFT()
{
  spr.fillRect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, TFT_BLACK);
}

void flushSprite()
{
  spr.pushSprite(0, 0);
}

bool updateGPSValue()
{
  while (ss.available() > 0)
  {
    gps.encode(ss.read());
  }

  bool updated = false;

  if (gps.time.isUpdated())
  {
    uint8_t _hour = gps.time.hour();
    uint8_t _minute = gps.time.minute();
    uint8_t _second = gps.time.second();
    uint8_t _centisecond = gps.time.centisecond();

    if (_hour != gpsTime.hour || _minute != gpsTime.minute || _second != gpsTime.second || _centisecond != gpsTime.centisecond)
    {
      gpsTime = {_hour, _minute, _second, _centisecond};
      updated = true;
    }
    else
    {
      return false;
    }
  }
  if (gps.location.isUpdated())
  {
    lat = gps.location.lat();
    lng = gps.location.lng();
  }
  if (gps.altitude.isUpdated())
  {
    altitude = gps.altitude.meters();
  }
  if (gps.satellites.isUpdated())
  {
    sat = gps.satellites.value();
    age = gps.satellites.age();
  }
  if (gps.hdop.isUpdated())
  {
    // hdop = 水平精度低下率
    hdop = gps.hdop.value();
  }
  if (gps.date.isUpdated())
  {
    year = gps.date.year();
    month = gps.date.month();
    day = gps.date.day();
  }
  if (gps.speed.isUpdated())
  {
    kmph = gps.speed.kmph();
  }
  /*
  if (gps.course.isUpdated())
  {
    deg = gps.course.deg();
  }
  */
  return updated && gps.location.isValid();
}

void update_gps()
{
  static int16_t gpsUpdatedCnt = 0;
  if (!updateGPSValue())
  {
    return;
  }
  if (gpsUpdatedCnt < 20)
  {
    // 安定化のために初期のデータは捨てる
    gpsUpdatedCnt++;
    return;
  }

  if (!isLogging && enableLogging)
  {
    Serial.println("log start");
    sprintf(logfilename, "/%04d%02d%02d_%02d%02d%02d.gpx", year, month, day,
            (gpsTime.hour + 9) % 24, gpsTime.minute, gpsTime.second);
    Serial.println(logfilename);
    f = SD.open(logfilename, FILE_WRITE);
    if (f)
    {
      f.println(F(GPS_XML_TAG));
      f.println(F(GPX_START_TAG));
      f.println(F(GPX_END_TAG));
      f.flush();
      isLogging = true;
    }
  }

  if (isLogging)
  {
    logGPS();
  }

  updateGpsLapTimer();
}

void updateGpsLapTimer()
{
  if (!gpsLapTimerEnable)
  {
    return;
  }

  if (targetLat != 0.0 && targetLng != 0.0)
  {
    targetDistanceMeter = calcDistance(lat, lng, targetLat, targetLng);
  }

  // エリア内にいる
  if (targetDistanceMeter < distanceThreatholdMeter)
  {
    isInArea = true;
  }
  // エリアから出た瞬間
  else if (isInArea)
  {
    isInArea = false;

    if (!isLapTimerOn)
    {
      startLap(gpsTime);
    }
    else
    {
      plusLap(gpsTime);
    }
  }
}

void drawSatellitesInfo(uint8_t poY)
{
  char str[30];
  sprintf(str, "%2d GPS(hdop=%3d,age=%2d)", sat, hdop, age);
  spr.drawString(str, DISPLAY_WIDTH - 26 * CHAR_WIDTH, poY);
}

void drawLatLng(uint8_t poY)
{
  char str[30];
  sprintf(str, "%9.6f,%9.6f %4.1fm", lat, lng, altitude);
  spr.drawString(str, 0, poY);
}

void drawSpeed(uint8_t poY)
{
  char str[10];
  // const char *cardinal = TinyGPSPlus::cardinal(deg);
  sprintf(str, "%4.1fkm/s", kmph);
  spr.setTextSize(BIG_TEXT_SIZE);
  spr.drawString(str, DISPLAY_WIDTH - (CHAR_WIDTH * BIG_TEXT_SIZE * 8), poY);
  spr.setTextSize(1);
}

void drawTimer(uint8_t poY)
{
  char str[30];

  uint8_t prevLap = max(0, lapNum - 1);

  spr.setTextSize(2);
  sprintf(str, "FAST%02d", fastestLap);
  spr.drawString(str, 0, poY);
  sprintf(str, "LAP.%02d", prevLap);
  spr.drawString(str, 0, poY + BIG_TEXT_SIZE * HEIGHT_UNIT * 1);
  sprintf(str, "LAP.%02d", lapNum);
  spr.drawString(str, 0, poY + BIG_TEXT_SIZE * HEIGHT_UNIT * 2);

  spr.setTextSize(BIG_TEXT_SIZE);

  TIME laptime = laps[fastestLap];
  sprintf(str, "%02d:%02d.%02d", laptime.minute, laptime.second, laptime.centisecond);
  spr.drawString(str, DISPLAY_WIDTH - (CHAR_WIDTH * BIG_TEXT_SIZE * 8), poY);

  laptime = laps[prevLap];
  sprintf(str, "%02d:%02d.%02d", laptime.minute, laptime.second, laptime.centisecond);
  spr.drawString(str, DISPLAY_WIDTH - (CHAR_WIDTH * BIG_TEXT_SIZE * 8), poY + BIG_TEXT_SIZE * HEIGHT_UNIT * 1);

  laptime = laps[lapNum];
  sprintf(str, "%02d:%02d.%02d", laptime.minute, laptime.second, laptime.centisecond);
  spr.drawString(str, DISPLAY_WIDTH - (CHAR_WIDTH * BIG_TEXT_SIZE * 8), poY + BIG_TEXT_SIZE * HEIGHT_UNIT * 2);

  spr.setTextSize(1);
}

void drawLoggingStatus(uint8_t poY)
{
  char str[80];
  sprintf(str, "%s  %3.1f%s%u %9.6f,%9.6f",
          logfilename, targetDistanceMeter, targetDistanceMeter < distanceThreatholdMeter ? "<" : ">",
          distanceThreatholdMeter, targetLat, targetLng);
  spr.drawString(str, 0, poY);
}

void drawDateTime(uint8_t poY)
{
  char str[30];
  sprintf(str, "%04d-%02d-%02d %02d:%02d:%02d.%02d", year, month, day,
          (gpsTime.hour + 9) % 24, gpsTime.minute, gpsTime.second, gpsTime.centisecond);
  spr.drawString(str, 0, poY);
}

void drawLoading()
{
  M5.Lcd.setBrightness(255);
  M5.Lcd.print("initialize...");
  delay(100);
  M5.Lcd.fillScreen(BLACK);
}

void initSprite()
{
  spr.setColorDepth(8);
  spr.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT);
  spr.setTextSize(1);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
}

void setPoint()
{
  targetLat = lat;
  targetLng = lng;
  saveConfig();
}

void saveConfig()
{
  File config = SD.open(config_file_path, FILE_WRITE);
  if (!config)
  {
    return;
  }
  char buf[20];
  sprintf(buf, "%9.6f", targetLat);
  config.println(buf);
  sprintf(buf, "%9.6f", targetLng);
  config.println(buf);
  sprintf(buf, "%d", distanceThreatholdMeter);
  config.println(buf);
  config.close();
}

void loadConfig()
{
  File config = SD.open(config_file_path, FILE_READ);
  if (!config)
  {
    return;
  }
  targetLat = config.parseFloat();
  targetLng = config.parseFloat();
  distanceThreatholdMeter = config.parseInt();
  config.close();
}

// https://arduino.stackexchange.com/questions/23115/measuring-the-distance-using-gps-programmed-with-arduino
double calcDistance(double lat1, double lng1, double lat2, double lng2)
{
  double X = (lng2 - lng1) * cos(lat1) * 40075000 / 360.0;
  double Y = (lat2 - lat1) * 40009000 / 360.0;
  return sqrt(X * X + Y * Y);
}

void buttonCheck(uint8_t poY)
{
  logButton(poY);
  gpslapButton(poY);
  lapButton(poY);
}

void logButton(uint8_t poY)
{
  char str[100];
  if (M5.BtnA.wasReleased())
  {
    if (enableLogging)
    {
      enableLogging = false;
      isLogging = false;
      delay(100);
      f.close();
    }
    else
    {
      enableLogging = true;
    }
  }
  else
  {
    sprintf(str, "LOG %s(%s)", isLogging ? "ON" : "OFF", enableLogging ? "enable" : "disable");
  }
  spr.drawString(str, 0, poY);
}

void gpslapButton(uint8_t poY)
{
  if (M5.BtnB.pressedFor(500))
  {
    if (!gpsLapBtnLongPressed)
    {
      gpsLapBtnLongPressed = true;
      setPoint();
    }
  }
  if (M5.BtnB.wasReleased())
  {
    if (gpsLapBtnLongPressed)
    {
      gpsLapBtnLongPressed = false;
    }
    else
    {
      gpsLapTimerEnable = !gpsLapTimerEnable;
    }
  }
  else
  {
    char str[100];
    sprintf(str, "GPSLap %s(SetPoint)", gpsLapTimerEnable ? "ON " : "OFF");
    spr.drawString(str, 160 - 9 * CHAR_WIDTH, poY);
  }
}

void resetLap()
{
  laps[0] = {0, 0, 0, 0};
  lapNum = 0;
  fastestLap = 0;
}

void startLap(TIME &time_now)
{
  isLapTimerOn = true;
  if (lapNum == 0)
  {
    // first lap
    plusLap(time_now);
  }
  else
  {
    times[lapNum - 1] = timediff(laps[lapNum], time_now);
  }
}

void stopLap()
{
  isLapTimerOn = false;
}

void plusLap(TIME &time_now)
{
  times[lapNum] = time_now;
  updateLap(time_now);

  if (fastestLap == 0 || compare(laps[lapNum], laps[fastestLap]) >= 0)
  {
    fastestLap = lapNum;
  }

  lapNum = (lapNum + 1) % 100;
}

void updateLap(TIME &time_now)
{
  if (isLapTimerOn && lapNum != 0)
  {
    laps[lapNum] = timediff(times[lapNum - 1], time_now);
  }
}

void lapButton(uint8_t poY)
{
  if (M5.BtnC.pressedFor(500))
  {
    if (!LapBtnLongPressed)
    {
      LapBtnLongPressed = true;

      if (isLapTimerOn)
      {
        // ラップタイマー実行中にロングタップで停止
        stopLap();
        spr.drawString("STOP!", DISPLAY_WIDTH - 5 * CHAR_WIDTH, poY);
      }
      else
      {
        // ラップタイマー停止中にロングタップでリセット
        resetLap();
        spr.drawString("RESET!", DISPLAY_WIDTH - 6 * CHAR_WIDTH, poY);
      }
    }
  }

  if (M5.BtnC.wasReleased())
  {
    if (LapBtnLongPressed)
    {
      LapBtnLongPressed = false;
    }
    else
    {
      if (isLapTimerOn)
      {
        // ラップタイマー実行中にタップでラップ計測
        plusLap(gpsTime);
      }
      else
      {
        // ラップタイマー停止中にタップでスタート
        startLap(gpsTime);
      }
    }
  }
  else
  {
    if (isLapTimerOn)
    {
      updateLap(gpsTime);
      spr.drawString("   LAP(STOP)", DISPLAY_WIDTH - 12 * CHAR_WIDTH, poY);
    }
    else
    {
      spr.drawString("START(RESET)", DISPLAY_WIDTH - 12 * CHAR_WIDTH, poY);
    }
  }
}

TIME timediff(TIME &t1, TIME &t2)
{
  int8_t diff_centisecond = t2.centisecond - t1.centisecond;
  int8_t diff_second = t2.second - t1.second;
  int8_t diff_minute = t2.minute - t1.minute;
  int8_t diff_hour = t2.hour - t1.hour;

  if (diff_centisecond < 0)
  {
    diff_second -= 1;
    diff_centisecond += 100;
  }
  if (diff_second < 0)
  {
    diff_minute -= 1;
    diff_second += 60;
  }
  if (diff_minute < 0)
  {
    diff_hour -= 1;
    diff_minute += 60;
  }
  if (diff_hour < 0)
  {
    diff_hour += 60;
  }

  return {(uint8_t)diff_hour, (uint8_t)diff_minute, (uint8_t)diff_second, (uint8_t)diff_centisecond};
}

int compare(TIME &t1, TIME &t2)
{
  uint32_t a = t1.hour * 1000000 + t1.minute * 10000 + t1.second * 100 + t1.centisecond;
  uint32_t b = t2.hour * 1000000 + t2.minute * 10000 + t2.second * 100 + t2.centisecond;

  if (a > b)
  {
    return -1;
  }
  else if (a < b)
  {
    return 1;
  }
  return 0;
}
