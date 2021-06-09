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

  errorStatus = 0;

  _mQueue = new SQueue<Box>(Q_SIZE);

  if (pcfInstance != nullptr) {
    _pcf = pcfInstance;
    setup();
  } else {
    setError(HD_ERR_SETUP);

#if defined(_DEBUG_)

    Serial.println(F("HD44780 constructor error"));

#endif // _DEBUG_

  }
}

void HD44780::doCommands(uint8_t data, uint8_t boxArr[], uint8_t cmnd) {

  boxArr[0] = highBits(data) | HD_BACK_LIGHT | cmnd;
  boxArr[1] = highBits(data) | HD_E | HD_BACK_LIGHT | cmnd;
  boxArr[2] = highBits(data) | HD_BACK_LIGHT | cmnd;
  boxArr[3] = (lowBits(data) << 4) | HD_BACK_LIGHT | cmnd;
  boxArr[4] = (lowBits(data) << 4) | HD_E | HD_BACK_LIGHT | cmnd;
  boxArr[5] = (lowBits(data) << 4) | HD_BACK_LIGHT | cmnd;

#if defined(_DEBUG_)

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

#endif // _DEBUG_
}

uint8_t HD44780::hdWrite(Box *item) {
  if (enqueue(item)) {
    return 1;
  } else {
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

uint8_t HD44780::hdWrite(uint8_t data[], uint16_t length) {
  uint8_t n = 0;
  uint8_t ii = 0;
  uint8_t dataLen = length / HD_CMNDS_NUM;

  for (size_t i = 0; i < dataLen; i++) {
    if (_pcf->isError())
      break;

    uint8_t d[3];
    d[0] = data[ii++];
    d[1] = data[ii++];
    d[2] = data[ii++];
    _pcf->send(d, 3, false);

    d[0] = data[ii++];
    d[1] = data[ii++];
    d[2] = data[ii++];
    _pcf->send(d, 3, false);

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

#if defined(_DEBUG_)

  Serial.print("Date readed: ");
  Serial.println(res, BIN);

#endif // _DEBUG_

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
  hdWrite(PCF_BEFORE_RECEIVE | HD_READ_STATUS | HD_BACK_LIGHT, false);
  hdWrite(PCF_BEFORE_RECEIVE | HD_READ_STATUS | HD_BACK_LIGHT | HD_E, false);
  hdRead(&data, false);
  hdWrite(PCF_BEFORE_RECEIVE | HD_READ_STATUS | HD_BACK_LIGHT, false);

  data = highBits((uint8_t)data);

  // Reading low bytes
  int8_t lowBits = 0;
  hdWrite(PCF_BEFORE_RECEIVE | HD_READ_STATUS | HD_BACK_LIGHT | HD_E, false);
  hdRead(&lowBits, false);
  hdWrite(PCF_BEFORE_RECEIVE | HD_READ_STATUS | HD_BACK_LIGHT, true);

  data |= highBits((uint8_t)lowBits) >> 4;

  bool busyFlag = bool(bitRead(data, 7));

  cursorIndex = (uint8_t)bitWrite(data, 7, 0);

  st->cursorPos = cursorIndex;

  if (isError())
    st->isBusy = 0;
  else
    st->isBusy = busyFlag;

#if defined(_DEBUG_)

  Serial.println(F("== HD status =="));
  Serial.println((uint8_t)data, BIN);
  Serial.print(F("CurInx: "));
  Serial.println(cursorIndex);
  Serial.print(F("IsBusy: "));
  Serial.println(busyFlag);

#endif // _DEBUG_
}

bool HD44780::isBusy() {

#if defined(_DEBUG_)

  Serial.println(F("------ isBusy -"));

#endif // _DEBUG_

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

#if defined(_DEBUG_)
        Serial.println(F("!HD is freez"));
#endif // _DEBUG_
      }
    }
  }
}

size_t HD44780::write(byte data) {
  Box *box = getBox(HD_CMNDS_NUM);
  doCommands(data, box->data, HD_WRITE_DATA);

  if (!hdWrite(box)) {
    freeBox(box);
    return 0;
  }

  return 1;
}

size_t HD44780::write(const uint8_t *data, size_t len) {
  Box *item;
  uint16_t dataIndex = 0;
  uint8_t bufferIndex = 0;
  uint8_t boxIndex = 0;
  uint8_t boxSize = 0;

  for (uint8_t i = 0; i < len; i++) {

    if (bufferIndex == 0) {

      if (len - dataIndex >= 5) { // 5 (5*6 = 30) - a one box of size 32 bit
        boxSize = 30;
      } else {
        boxSize = (len - dataIndex) * HD_CMNDS_NUM;
      }

#if defined(_DEBUG_)

      Serial.print(F("#Box size: "));
      Serial.println(boxSize);

#endif // _DEBUG_

      item = getBox(boxSize);
      boxIndex++;
    }

    uint8_t boxs[HD_CMNDS_NUM];
    doCommands(data[i], boxs, HD_WRITE_DATA);

    item->data[bufferIndex++] = boxs[0];
    item->data[bufferIndex++] = boxs[1];
    item->data[bufferIndex++] = boxs[2];
    item->data[bufferIndex++] = boxs[3];
    item->data[bufferIndex++] = boxs[4];
    item->data[bufferIndex++] = boxs[5];

    dataIndex++;

    if (bufferIndex == boxSize) {
      bufferIndex = 0;

      if (!hdWrite(item)) {
        freeBox(item);
        break;
      }
    }
  }

#if defined(_DEBUG_)

  uint16_t dataLength = len * HD_CMNDS_NUM;

  // the max bits in a one box = 5 * 6 = 30
  uint8_t boxsNum = (dataLength / 30) + 1;

  Serial.println(F("=== command array ===="));
  Serial.print(F("Data len "));
  Serial.println(len);
  Serial.print(F("Total bytes "));
  Serial.println(dataLength);
  Serial.print(F("Done cmnd: "));
  Serial.println(dataIndex);
  Serial.print(F("Total boxs : "));
  Serial.println(boxsNum);
  Serial.print(F("Maked boxs: "));
  Serial.println(boxIndex);
  Serial.println(F("***"));

#endif // _DEBUG_

  return dataIndex;
}

void HD44780::setup() {
#if defined(_DEBUG_)

  Serial.println(F("HD44780 setup"));

#endif // _DEBUG_

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

  uint8_t boxs[HD_CMNDS_NUM];

  doCommands(HD_SETTINGS | HD_S_BUS_INTERFACE_4 | HD_S_FONT_5X8 |
                 HD_S_NUM_LINES_2,
             boxs);
  hdWrite(boxs, HD_CMNDS_NUM);
  waitingHd();

  doCommands(HD_CONTROL | HD_C_CURSOR_ON | HD_C_DSPL_ON, boxs);
  hdWrite(boxs, HD_CMNDS_NUM);
  waitingHd();

  doCommands(HD_ENTRY_MODE | HD_EM_INCREMENT_INDEX_CURSOR, boxs);
  hdWrite(boxs, HD_CMNDS_NUM);
  waitingHd();

  doCommands(HD_CLEAR, boxs);
  hdWrite(boxs, HD_CMNDS_NUM);
  waitingHd();

  doCommands(HD_HOME, boxs);
  hdWrite(boxs, HD_CMNDS_NUM);
  waitingHd();

#if defined(_DEBUG_)

  if (isError())
    Serial.println(F("HD44780 setup error"));
  else
    Serial.println(F("HD44780 setup ok"));

#endif // _DEBUG_
}

void HD44780::printBeginPosition(uint8_t position, const char cst[],
                                 uint8_t len) {
  // WRITE_TO_POSOTION(position, false);
  // isBusy();

  /* for (uint8_t i = 0; i < len; i++)
    command(HD_WRITE_DATA, cst[i]);
  setError(_pcf->commit()); */

  // command(HD_WRITE_DATA, (uint8_t *)cst, len);
}

void HD44780::setCursor(uint8_t col, uint8_t row) {
  // row_offsets[] = { 0x00, 0x40, 0x14, 0x54 };
  if (row >= 2) {
    WRITE_TO_POSOTION(col + 64, false);
  } else {
    WRITE_TO_POSOTION(col, false);
  }

  isBusy();
}

Box *HD44780::getBox(uint16_t size) {
  Box *newBox = new Box;
  newBox->data = new uint8_t[size];
  newBox->size = size;

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

void HD44780::clean() {
  while (!_mQueue->isEmpty())
    freeBox(_mQueue->dequeue());
  _mQueue->clean();
}

//todo error handler
void HD44780::checkQueue() {
  if (!_mQueue->isEmpty()) {
    Box *data = _mQueue->dequeue();

    if (data != nullptr) {
      if (data->size > 1) {
        hdWrite(data->data, data->size);
      } else {
        hdWrite(data->data[0]);
      }
      freeBox(data);
    }
  }
}