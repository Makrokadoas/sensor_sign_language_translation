#include <SPI.h>

// =========================
// Пины
// =========================
#define CS_MPU 10
#define CS_BMP 9

#define FLEX1_PIN A0
#define FLEX2_PIN A1
#define FLEX3_PIN A2
#define FLEX4_PIN A3
#define FLEX5_PIN A4

// =========================
// Регистры MPU-6500
// =========================
#define REG_SMPLRT_DIV     0x19
#define REG_CONFIG         0x1A
#define REG_GYRO_CONFIG    0x1B
#define REG_ACCEL_CONFIG   0x1C
#define REG_ACCEL_CONFIG2  0x1D
#define REG_ACCEL_XOUT_H   0x3B
#define REG_PWR_MGMT_1     0x6B
#define REG_PWR_MGMT_2     0x6C
#define REG_WHO_AM_I       0x75

// =========================
// Настройки SPI
// =========================
SPISettings spiSettings(1000000, MSBFIRST, SPI_MODE3);

// =========================
// Калибровка гироскопа
// =========================
float gyroBiasX = 0.0f;
float gyroBiasY = 0.0f;
float gyroBiasZ = 0.0f;

// =========================
// Масштабы
// =========================
const float accelScale = 16384.0f; // ±2g
const float gyroScale  = 131.0f;   // ±250 dps

// Частота вывода
// 10000 мкс = 10 мс = 100 Гц
// Для HC-06 это стабильнее, чем 200 Гц
const uint32_t LOOP_PERIOD_US = 10000;

uint32_t loopTimer = 0;

// =========================
// SPI helpers
// =========================
void writeReg(uint8_t reg, uint8_t data) {
  SPI.beginTransaction(spiSettings);
  digitalWrite(CS_MPU, LOW);
  SPI.transfer(reg & 0x7F);
  SPI.transfer(data);
  digitalWrite(CS_MPU, HIGH);
  SPI.endTransaction();
}

uint8_t readReg(uint8_t reg) {
  SPI.beginTransaction(spiSettings);
  digitalWrite(CS_MPU, LOW);
  SPI.transfer(reg | 0x80);
  uint8_t data = SPI.transfer(0x00);
  digitalWrite(CS_MPU, HIGH);
  SPI.endTransaction();
  return data;
}

void readRegs(uint8_t startReg, uint8_t *buffer, uint8_t len) {
  SPI.beginTransaction(spiSettings);
  digitalWrite(CS_MPU, LOW);
  SPI.transfer(startReg | 0x80);
  for (uint8_t i = 0; i < len; i++) {
    buffer[i] = SPI.transfer(0x00);
  }
  digitalWrite(CS_MPU, HIGH);
  SPI.endTransaction();
}

int16_t makeInt16(uint8_t highByte, uint8_t lowByte) {
  return (int16_t)((highByte << 8) | lowByte);
}

// =========================
// Инициализация MPU-6500
// =========================
void initMPU6500() {
  delay(100);

  // Выход из sleep
  writeReg(REG_PWR_MGMT_1, 0x00);
  delay(100);

  // Тактирование от PLL
  writeReg(REG_PWR_MGMT_1, 0x01);
  delay(10);

  // Включить все оси
  writeReg(REG_PWR_MGMT_2, 0x00);
  delay(10);

  // При включённом DLPF внутренняя частота 1 кГц
  // 1000 / (1 + 4) = 200 Гц внутренняя частота выборки
  writeReg(REG_SMPLRT_DIV, 0x04);

  // DLPF гироскопа ~41 Гц
  writeReg(REG_CONFIG, 0x03);

  // Гироскоп ±250 dps
  writeReg(REG_GYRO_CONFIG, 0x00);

  // Акселерометр ±2g
  writeReg(REG_ACCEL_CONFIG, 0x00);

  // DLPF акселерометра ~44 Гц
  writeReg(REG_ACCEL_CONFIG2, 0x03);

  delay(50);
}

// =========================
// Калибровка гироскопа
// =========================
void calibrateGyro(uint16_t samples = 2000) {
  long sumX = 0;
  long sumY = 0;
  long sumZ = 0;

  uint8_t buf[14];

  Serial.println("# Calibrating gyro... Keep sensor still.");

  for (uint16_t i = 0; i < samples; i++) {
    readRegs(REG_ACCEL_XOUT_H, buf, 14);

    int16_t gx = makeInt16(buf[8],  buf[9]);
    int16_t gy = makeInt16(buf[10], buf[11]);
    int16_t gz = makeInt16(buf[12], buf[13]);

    sumX += gx;
    sumY += gy;
    sumZ += gz;

    delay(2);
  }

  gyroBiasX = (float)sumX / samples;
  gyroBiasY = (float)sumY / samples;
  gyroBiasZ = (float)sumZ / samples;

  Serial.println("# Gyro calibration done.");
  Serial.print("# gyroBiasX = "); Serial.println(gyroBiasX, 2);
  Serial.print("# gyroBiasY = "); Serial.println(gyroBiasY, 2);
  Serial.print("# gyroBiasZ = "); Serial.println(gyroBiasZ, 2);
}

// =========================
// Чтение accel + gyro
// =========================
void readAccelGyro(
  int16_t &axRaw, int16_t &ayRaw, int16_t &azRaw,
  int16_t &gxRaw, int16_t &gyRaw, int16_t &gzRaw
) {
  uint8_t buf[14];
  readRegs(REG_ACCEL_XOUT_H, buf, 14);

  axRaw = makeInt16(buf[0],  buf[1]);
  ayRaw = makeInt16(buf[2],  buf[3]);
  azRaw = makeInt16(buf[4],  buf[5]);

  gxRaw = makeInt16(buf[8],  buf[9]);
  gyRaw = makeInt16(buf[10], buf[11]);
  gzRaw = makeInt16(buf[12], buf[13]);
}

// =========================
// setup
// =========================
void setup() {
  // HC-06 обычно 9600 бод
  Serial.begin(57600);

  SPI.begin();

  pinMode(CS_MPU, OUTPUT);
  pinMode(CS_BMP, OUTPUT);

  digitalWrite(CS_MPU, HIGH);
  digitalWrite(CS_BMP, HIGH);

  pinMode(FLEX1_PIN, INPUT);
  pinMode(FLEX2_PIN, INPUT);
  pinMode(FLEX3_PIN, INPUT);
  pinMode(FLEX4_PIN, INPUT);
  pinMode(FLEX5_PIN, INPUT);

  delay(3000);

  initMPU6500();

  uint8_t who = readReg(REG_WHO_AM_I);
  Serial.print("# WHO_AM_I: 0x");
  Serial.println(who, HEX);

  if (who == 0x70) {
    Serial.println("# MPU-6500 detected");
  } else if (who == 0x71) {
    Serial.println("# MPU-9250 detected");
  } else {
    Serial.println("# Unknown IMU, continue carefully");
  }

  delay(3000);
  calibrateGyro(2000);

  loopTimer = micros();

  // Заголовок CSV
  Serial.println("t_ms,flex1,flex2,flex3,flex4,flex5,ax,ay,az,gx,gy,gz");
}

// =========================
// loop
// =========================
void loop() {
  int16_t axRaw, ayRaw, azRaw;
  int16_t gxRaw, gyRaw, gzRaw;

  readAccelGyro(axRaw, ayRaw, azRaw, gxRaw, gyRaw, gzRaw);

  float ax = axRaw / accelScale;
  float ay = ayRaw / accelScale;
  float az = azRaw / accelScale;

  float gx = (gxRaw - gyroBiasX) / gyroScale;
  float gy = (gyRaw - gyroBiasY) / gyroScale;
  float gz = (gzRaw - gyroBiasZ) / gyroScale;

  int flex1 = analogRead(FLEX1_PIN);
  int flex2 = analogRead(FLEX2_PIN);
  int flex3 = analogRead(FLEX3_PIN);
  int flex4 = analogRead(FLEX4_PIN);
  int flex5 = analogRead(FLEX5_PIN);

  Serial.print(millis()); Serial.print(",");
  Serial.print(flex1);    Serial.print(",");
  Serial.print(flex2);    Serial.print(",");
  Serial.print(flex3);    Serial.print(",");
  Serial.print(flex4);    Serial.print(",");
  Serial.print(flex5);    Serial.print(",");
  Serial.print(ax, 4);    Serial.print(",");
  Serial.print(ay, 4);    Serial.print(",");
  Serial.print(az, 4);    Serial.print(",");
  Serial.print(gx, 4);    Serial.print(",");
  Serial.print(gy, 4);    Serial.print(",");
  Serial.println(gz, 4);

  while (micros() - loopTimer < LOOP_PERIOD_US) {
  }
  loopTimer = micros();
}