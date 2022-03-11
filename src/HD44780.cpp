#include "HD44780.h"

void freeBox(Box *b) {
  if (b != nullptr) {
    if (b->data != nullptr)
      delete[] b->data;
    delete b;
  }
}

HD44780::HD44780(PCF8574T *pcfInstance, byte chars, byte rows) {
  _rows = rows;
  _chars = chars;

  _errorStatus = 0;

  _mQueue = new SQueue<Box>(Q_SIZE);

  if (pcfInstance != nullptr) {
    _pcf = pcfInstance;
    setup();
  } else {
    setError(HD_ERR_SETUP);

#if defined(_HD_DEBUG_)

    Serial.println(F("HD44780 constructor error"));

#endif // _HD_DEBUG_
  }
}

void HD44780::doCommands(uint8_t data, uint8_t boxArr[], uint8_t cmnd) {

#if HD_SMART_MODE == true

  boxArr[0] = highBits(data) | _HD_BACKLIGHT | cmnd;
  boxArr[1] = (lowBits(data) << 4) | _HD_BACKLIGHT | cmnd;

#elif HD_SMART_MODE == false

  boxArr[0] = highBits(data) | _HD_BACKLIGHT | cmnd;
  boxArr[1] = highBits(data) | HD_E | _HD_BACKLIGHT | cmnd;
  boxArr[2] = highBits(data) | _HD_BACKLIGHT | cmnd;
  boxArr[3] = (lowBits(data) << 4) | _HD_BACKLIGHT | cmnd;
  boxArr[4] = (lowBits(data) << 4) | HD_E | _HD_BACKLIGHT | cmnd;
  boxArr[5] = (lowBits(data) << 4) | _HD_BACKLIGHT | cmnd;

#if defined(_HD_DEBUG_)

  Serial.println(F("=== Do commands ==="));

  Serial.print(F("Cmnd: "));
  Serial.println(cmnd, BIN);

  Serial.print(F("Data: "));
  Serial.println(data, BIN);

  Serial.print(F("HighBits: "));
  Serial.println(highBits(data));
  Serial.print(F("LowBits: "));
  Serial.println(lowBits(data));

  Serial.print(F("[0] "));
  Serial.println(boxArr[0], BIN);

  Serial.print(F("[1] "));
  Serial.println(boxArr[1], BIN);

  Serial.print(F("[2] "));
  Serial.println(boxArr[2], BIN);

  Serial.print(F("[3] "));
  Serial.println(boxArr[3], BIN);

  Serial.print(F("[4] "));
  Serial.println(boxArr[4], BIN);

  Serial.print(F("[5] "));
  Serial.println(boxArr[5], BIN);

#endif // _HD_DEBUG_

#endif // HD_SMART_MODE
}

uint8_t HD44780::hdWrite(Box *item) {
  if (enqueue(item)) {
    return 1;
  } else {
    freeBox(item);
    setError(HD_Q_ERR);
    return 0;
  }
}

uint8_t HD44780::hdWrite(uint8_t data, bool isEnd) {

  _pcf->send(data, isEnd);

  if (_pcf->isError()) {
    setError(HD_ERR_PCF);
    return 0;
  }

  return 1;
}

uint8_t HD44780::hdWrite(uint8_t data[], uint16_t length, uint8_t type) {
  uint8_t n = 0;
  uint8_t item[HD_CMNDS_NUM];
  uint8_t section[3];

  for (uint16_t i = 0; i < length; i++) {
    if (_pcf->isError())
      break;

    doCommands(data[i], item, type);

#if HD_SMART_MODE == true

    section[0] = item[0];
    section[1] = item[0] | HD_E;
    section[2] = item[0];
    _pcf->send(section, 3, false);

    section[0] = item[1];
    section[1] = item[1] | HD_E;
    section[2] = item[1];
    _pcf->send(section, 3, false);

#elif HD_SMART_MODE == false

    section[0] = item[0];
    section[1] = item[1];
    section[2] = item[2];
    _pcf->send(section, 3, false);

    section[0] = item[3];
    section[1] = item[4];
    section[2] = item[5];
    _pcf->send(section, 3, false);

#endif // HD_SMART_MODE

    waitingHd();
    n++;
  }

  if (_pcf->isError()) {
    setError(HD_ERR_PCF);
    return 0;
  }

  return n;
}

uint8_t HD44780::hdRead(int8_t *data, bool isEnd) {
  int8_t res = _pcf->read(1, isEnd);

#if defined(_HD_DEBUG_)

  Serial.print("Date readed: ");
  Serial.println(res, BIN);

#endif // _HD_DEBUG_

  if (!_pcf->isError()) {
    *data = res;
    return 1;
  } else {
    setError(HD_ERR_PCF);
    return 0;
  }
}

void HD44780::hdState(HDState *st) {
  // Reading high bytes
  int8_t data = 0;
  hdWrite(PCF_BEFORE_RECEIVE | HD_READ_STATUS | _HD_BACKLIGHT, false);
  hdWrite(PCF_BEFORE_RECEIVE | HD_READ_STATUS | _HD_BACKLIGHT | HD_E, false);
  hdRead(&data, false);
  hdWrite(PCF_BEFORE_RECEIVE | HD_READ_STATUS | _HD_BACKLIGHT, false);

  data = highBits((uint8_t)data);

  // Reading low bytes
  int8_t lowBits = 0;
  hdWrite(PCF_BEFORE_RECEIVE | HD_READ_STATUS | _HD_BACKLIGHT | HD_E, false);
  hdRead(&lowBits, false);
  hdWrite(PCF_BEFORE_RECEIVE | HD_READ_STATUS | _HD_BACKLIGHT, true);

  data |= highBits((uint8_t)lowBits) >> 4;

  bool busyFlag = bool(bitRead(data, 7));

  _cursorIndex = (uint8_t)bitWrite(data, 7, 0);

  st->cursorPos = _cursorIndex;

  if (isError())
    st->isBusy = 0;
  else
    st->isBusy = busyFlag;

#if defined(_HD_DEBUG_)

  Serial.println(F("== HD status =="));
  Serial.println((uint8_t)data, BIN);
  Serial.print(F("CurInx: "));
  Serial.println(_cursorIndex);
  Serial.print(F("IsBusy: "));
  Serial.println(busyFlag);

#endif // _HD_DEBUG_
}

bool HD44780::isBusy() {

#if defined(_HD_DEBUG_)

  Serial.println(F("------ isBusy -"));

#endif // _HD_DEBUG_

  HDState st;
  hdState(&st);

  return st.isBusy;
}

void HD44780::waitingHd() {

  for (uint8_t i = 0; i < 100; i++) {
    if (!isBusy())
      break;
    else {

      delayMicroseconds(5);

      if (i == 99) {
        setError(HD_ERR_FREEZ);

#if defined(_HD_DEBUG_)
        Serial.println(F("!HD is freez"));
#endif // _HD_DEBUG_
      }
    }
  }
}

size_t HD44780::write(byte data) {
  Box *box = getDataBox();
  box->data[0] = data;

  if (!hdWrite(box))
    return 0;

  return 1;
}

size_t HD44780::write(const uint8_t *data, size_t len) {
  if (len == 0 || data == nullptr)
    return 0;

  Box *box = getDataBox(len);
  for (size_t i = 0; i < len; i++)
    box->data[i] = data[i];

  if (!hdWrite(box))
    return 0;

  return len;
}

void HD44780::setup() {
#if defined(_HD_DEBUG_)

  Serial.println(F("HD44780 setup"));

#endif // _HD_DEBUG_

  delay(50);

  hdWrite(0x30 | HD_E, false);
  hdWrite(0x30);
  delayMicroseconds(4100);

  hdWrite(0x30 | HD_E, false);
  hdWrite(0x30);
  delayMicroseconds(100);

  hdWrite(0x30 | HD_E, false);
  hdWrite(0x30);

  hdWrite(0x20 | HD_E, false);
  hdWrite(0x20, false);
  waitingHd();

  uint8_t box[] = {0};

  box[0] = HD_SETTINGS | HD_S_BUS_INTERFACE_4 | HD_S_FONT_5X8 | HD_S_NUM_LINES_2;
  hdWrite(box, 1, HD_WRITE_COMMAND);
  waitingHd();

  box[0] = _HD_CONTROL;
  hdWrite(box, 1, HD_WRITE_COMMAND);
  waitingHd();

  box[0] = HD_ENTRY_MODE | HD_EM_INCREMENT_INDEX_CURSOR;
  hdWrite(box, 1, HD_WRITE_COMMAND);
  waitingHd();

  box[0] = HD_CLEAR;
  hdWrite(box, 1, HD_WRITE_COMMAND);
  waitingHd();

  box[0] = HD_HOME;
  hdWrite(box, 1, HD_WRITE_COMMAND);
  waitingHd();

#if defined(_HD_DEBUG_)

  if (isError())
    Serial.println(F("HD44780 setup error"));
  else
    Serial.println(F("HD44780 setup ok"));

#endif // _HD_DEBUG_
}

void HD44780::printAt(uint8_t col, uint8_t row, const char cst[]) {
  setCursor(col, row);
  print(cst);
}

bool HD44780::goToEnd() {

  while (checkQueue());

  if(isError())
    return false;
  
  return true;
}

bool HD44780::goToEnd(const char* str) {
  print(str);

  if(!goToEnd()) return false;

  return true;
}

void HD44780::setCursor(uint8_t col, uint8_t row) {
  // row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };

  uint8_t curPos = col | HD_SET_CUR_INDEX;

  if (row >= 1)
    curPos = (col + 0x40) | HD_SET_CUR_INDEX;

  Box *b = getCommandBox();
  b->data[0] = curPos;

  hdWrite(b);
}

void HD44780::clear() {
  Box *box = getCommandBox();
  box->data[0] = HD_CLEAR;
  hdWrite(box);
};

void HD44780::home() {
  Box *box = getCommandBox();
  box->data[0] = HD_HOME;
  hdWrite(box);
}

void HD44780::moveCursorRight() {
  Box *box = getCommandBox();
  box->data[0] = HD_MOVE_CURSOR_RIGHT;
  hdWrite(box);
}

void HD44780::moveCursorLeft() {
  Box *box = getCommandBox();
  box->data[0] = HD_MOVE_CURSOR_LEFT;
  hdWrite(box);
}

void HD44780::moveDisplayRight() {
  Box *box = getCommandBox();
  box->data[0] = HD_SHIFT_DSP_RIGHT;
  hdWrite(box);
}

void HD44780::moveDisplayLeft() {
  Box *box = getCommandBox();
  box->data[0] = HD_SHIFT_DSP_LEFT;
  hdWrite(box);
}

void HD44780::on() {

  if (bitRead(_HD_CONTROL, 2))
    return;

  backlight(true);
  Box *box = getCommandBox();
  bitSet(_HD_CONTROL, 2);
  box->data[0] = _HD_CONTROL;
  hdWrite(box);
}

void HD44780::off() {

  if (!bitRead(_HD_CONTROL, 2))
    return;

  backlight(false);
  Box *box = getCommandBox();
  bitClear(_HD_CONTROL, 2);
  box->data[0] = _HD_CONTROL;
  hdWrite(box);
}

Box *HD44780::getBox(uint16_t size) {
  Box *newBox = new Box;
  newBox->data = new uint8_t[size];
  newBox->size = size;

  return newBox;
}

Box *HD44780::getBox(uint16_t size, Box::TYPES type) {
  Box *newBox = getBox(size);
  newBox->type = type;

  return newBox;
}

bool HD44780::enqueue(Box *item) {
  if (_mQueue->isFull()) {
    setError(Q_ERR);
    return false;
  }

  _mQueue->enqueue(item);

  return true;
}

void HD44780::qClean() {
  while (!_mQueue->isEmpty())
    freeBox(_mQueue->dequeue());
  _mQueue->clean();
}

bool HD44780::checkQueue() {
  if (!_mQueue->isEmpty()) {
    Box *box = _mQueue->dequeue();

    if (box != nullptr) {
      hdWrite(box->data, box->size, box->type);
      freeBox(box);
      return true;
    }
  }

  return false;
}