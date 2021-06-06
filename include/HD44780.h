/** TODO
 * without PCF module
 * size_t -> uint8_t fn: write, command

 * !!! PROJECT CRUSTAL !!!

 **/

#if !defined(HD44780_H)
#define HD44780_H

#include <PCF8574T.h>
#include <Print.h>
#include <SQueue.h>

/**
 * Note
 *
 * in serial and calc
 * [D7][D6][D5][D4][BL][E][RW][RS]
 **/

/**
 * Registers
 **/
#define HD_WRITE_COMMAND 0x0U
#define HD_WRITE_DATA 0x1U
#define HD_READ_STATUS 0x2U // read busy flag and index position
#define HD_READ_DATA 0x3U
#define HD_E 0x4U
#define HD_BACK_LIGHT 0x8U

/**
 * General commands
 **/
#define HD_CLEAR 0x1U
#define HD_HOME 0x2U
#define HD_SHIFT_RIGHT 0x1CU // Shift dispaly
#define HD_SHIFT_LEFT 0x18U
#define HD_MOVE_CURSOR_RIGHT 0x14U
#define HD_MOVE_CURSOR_LEFT 0x10U
#define HD_WRITE_TO_POSITION 0x80U

/**
 * Entry mode set
 * command = HD_ENTRY_MODE | HD_EM_INCREMENT_INDEX_CURSOR
 **/
#define HD_ENTRY_MODE 0x4U
#define HD_EM_INCREMENT_INDEX_CURSOR 0x2U
#define HD_EM_DECREMENT_INDEX_CURSOR 0x0U
#define HD_EM_SHIFT_DSPL_AND_CURSOR 0x1U
#define HD_EM_NSHIFT_DSPL_AND_CURSOR 0x0U

/**
 * Cursor and dispaly control
 * command = HD_CONTROL | HD_SHIFT
 **/
#define HD_CONTROL 0x8U
#define HD_C_DSPL_ON 0x4U
#define HD_C_CURSOR_ON 0x2U
#define HD_C_CURSOR_BLINK_ON 0x1U

/**
 * Function set
 * command = HD_CONTROL | HD_SHIFT
 **/
#define HD_SETTINGS 0x20U
#define HD_S_BUS_INTERFACE_8 0x10U
#define HD_S_BUS_INTERFACE_4 0x0U
#define HD_S_NUM_LINES_2 0x8U
#define HD_S_NUM_LINE_1 0x0U
#define HD_S_FONT_5X10 0x4U
#define HD_S_FONT_5X8 0x0U

// #define HD_LINE_LENGTH 40u // lenght of one line in two lines mode

#define HD_CMNDS_NUM 6U // A one char converted in HD_CMNDS_NUM bytes

#define HD_ERR_PCF 99
#define HD_ERR_DO_CMNDS 98
#define HD_ERR_FREEZ 97

struct Box {
  uint8_t *data;
  uint16_t size;
};

struct HDState {
  bool isBusy;
  uint8_t cursorPos;
};

void freeBox(Box *b);
inline uint8_t lowBits(uint8_t byte) { return byte & 0xF; }
inline uint8_t highBits(uint8_t byte) { return byte & 0xF0; }

class HD44780 : public Print {
private:
  PCF8574T *_pcf = nullptr;
  SQueue<Box> *_mQueue;
  uint8_t _chars;
  uint8_t _rows;
  uint8_t backLight;
  uint8_t _address;
  uint8_t cursorIndex;
  int8_t errorStatus;

  void setup();
  size_t write(byte) override;                               // from print
  size_t write(const uint8_t *buffer, size_t size) override; // from print

  /**
   * The fanction read a data throu PCF chip
   * @param *data pointer an data
   * @param isEnd is last a transaction?
   * @return 1 if readed, 0 - if error
   **/
  uint8_t hdRead(int8_t *data, bool isEnd = true);

  HDState hdState();

  Box *getBox(uint16_t);

public:
  HD44780() {}
  ~HD44780() {}

  /**
   * Begin with PCF8574T
   * 
   * @param pcfInstance  
   * @param chars
   * @param rows
   **/
  explicit HD44780(PCF8574T *, byte = 16, byte = 2);

  /**
   * Checks the status of the internal state
   *
   * @note Non blocking operation
   * 
   * @return busy state
   **/
  bool isBusy();

  /**
   * To wait while HD done internal operation
   *
   * @note After waiting 1000 mcs, we considered that thr HD is freez
   **/
  void waitingHd();

  /**
   * Diract write 1 byte (8 bits)
   *
   * @param data 1 byte of a data
   * @param isEnd Free the i2c bus?
   **/
  uint8_t hdWrite(uint8_t data, bool isEnd = true);

  /**
   * Write 1 byte (8 bits)
   *
   * @param data[] array of data
   * @param length data length
   * @param isEnd Free the i2c bus? defaul true
   **/
  uint8_t hdWrite(uint8_t data[], uint16_t length, bool isEnd = true);

  /**
   * Write 1 byte (8 bits)
   *
   * @note A data shoud be packed into a Box struct
   *
   * @param *item data package for queue
   **/
  uint8_t hdWrite(Box *item);

  /**
   * Prepare data for write in HD.
   *
   * @note 1 byte (uint8_t) = HD_CMNDS_NUM bytes
   *
   * @param data payload for write in HD
   * @param boxArr pointer on array for converted bytes. Array mast have to
   *contain HD_CMNDS_NUM length
   * @param cmnd HD register. Default = HD_WRITE_COMMAND
   **/
  void doCommands(uint8_t data, uint8_t boxArr[],
                  uint8_t cmnd = HD_WRITE_COMMAND);

  bool enqueue(Box *);
  void checkQueue();
  uint16_t queueSize();
  void clean();
  void printBeginPosition(uint8_t, const char[], uint8_t); // printAt()
  void setCursor(uint8_t col, uint8_t row);

  inline void clear(bool isEnd = true){};

  inline void on(bool isEnd = true) {}

  inline void off(bool isEnd = true) {}

  inline void home(bool isEnd = true){};

  inline void backspace(bool isEnd = true){};

  inline void moveCursorRight(bool isEnd = true){};

  inline void moveCursorLeft(bool isEnd = true){};

  inline void moveDisplayRight(bool isEnd = true) {}

  inline void moveDisplayLeft(bool isEnd = true) {}

  inline void WRITE_TO_POSOTION(uint8_t posotion, bool isEnd = true){};

  inline uint8_t getWidth() const { return _chars; }

  inline uint8_t getHeight() const { return _rows; }

  inline uint8_t getCursorIndex() { return cursorIndex; };
  inline uint8_t isError() { return errorStatus; }
  inline void setError(int8_t err) {
    errorStatus == 0 ? errorStatus = err : errorStatus = err;
  };
  // inline void clearError() { this->errorStatus = 0; };
};

#endif // HD44780_H
