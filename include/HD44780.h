/**
 * Author: Ross Rich
 * Email:  RossRich@bk.ru
 *
 * Project Crystal
 *
 * Note
 * in serial and calc
 * [D7][D6][D5][D4][BL][E][RW][RS]
 *
 * TODO
 *  "Set block" for F("asdasdasd")
 **/

#if !defined(HD44780_H)
#define HD44780_H

#include <PCF8574T.h>
#include <Print.h>
#include <SQueue.h>

/**
 * Registers
 **/
#define HD_WRITE_COMMAND 0x0U
#define HD_WRITE_DATA 0x1U
#define HD_READ_STATUS 0x2U // read busy flag and index position
#define HD_READ_DATA 0x3U
#define HD_E 0x4U
#define HD_BACKLIGHT 0x8U

/**
 * General commands
 **/
#define HD_CLEAR 0x1U
#define HD_HOME 0x2U
#define HD_SHIFT_DSP_RIGHT 0x1CU // Shift dispaly
#define HD_SHIFT_DSP_LEFT 0x18U
#define HD_MOVE_CURSOR_RIGHT 0x14U
#define HD_MOVE_CURSOR_LEFT 0x10U
#define HD_SET_CUR_INDEX 0x80U

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

#define HD_CMNDS_NUM 6U // One char converted in HD_CMNDS_NUM bytes

#define HD_ERR_PCF 99
#define HD_Q_ERR 89
#define HD_ERR_DO_CMNDS 79
#define HD_ERR_FREEZ 78
#define HD_ERR_SETUP 77

/**
 * Struct for queue
 **/
struct Box {
  uint8_t *data;
  uint16_t size;
};

/**
 * Struct for store HD state
 **/
struct HDState {
  bool isBusy;
  uint8_t cursorPos;
};

void freeBox(Box *b); // del box from memory
inline uint8_t lowBits(uint8_t byte) { return byte & 0xF; }
inline uint8_t highBits(uint8_t byte) { return byte & 0xF0; }

class HD44780 : public Print {
private:
  PCF8574T *_pcf = nullptr;
  SQueue<Box> *_mQueue;
  uint8_t _chars;
  uint8_t _rows;
  uint8_t _cursorIndex;
  int8_t _errorStatus;
  uint8_t _HD_CONTROL = HD_CONTROL | HD_C_CURSOR_ON | HD_C_DSPL_ON;
  uint8_t _HD_BACKLIGHT = HD_BACKLIGHT;

  HD44780() {}

  /**
   * Main setup HD
   **/
  void setup();

  /**
   * Prepares data for write into HD and add it to a queue
   * @note Its override func from Print.h
   *
   * @param byte 8 bit of data for write into HD
   * @return 0 if error, else 1
   **/
  size_t write(byte) override;

  /**
   * Prepares array of data for write into HD and add it to a queue
   * @note Its override func from Print.h
   *
   * @param buffer array of data for write
   * @param size buffer size
   *
   * @return num of bytes written
   **/
  size_t write(const uint8_t *buffer, size_t size) override;

  /**
   * The function read a data through PCR chip
   *
   * @param *data pointer to data
   * @param isEnd is last a transaction?
   *
   * @return 1 if readed, 0 - if error
   **/
  uint8_t hdRead(int8_t *data, bool isEnd = true);

  /**
   * Read the internal status of HD
   *
   * @param *st HDState pointer
   **/
  void hdState(HDState *st);

  /**
   * Prepare box for store converted data
   * @see struct Box
   * @note size = num chars * HD_CMNDS_NUM
   *
   * @param size box size
   *
   * @return pointer on struct 'Box'
   **/
  Box *getBox(uint16_t size);

  /**
   * @brief Backlight control
   * @param state true - on backlight, false - off backlight
   */
  inline void backlight(bool state) { state ? bitSet(_HD_BACKLIGHT, 3) : bitClear(_HD_BACKLIGHT, 3); }

  /**
   * @brief Set display error 
   */
  inline void setError(int8_t err) {
    if (_errorStatus == 0)
      _errorStatus = err;
  };

public:
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
   * Checks a status of internal state
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
   *
   * @return 0 if error, else 1
   **/
  uint8_t hdWrite(uint8_t data, bool isEnd = true);

  /**
   * Write 1 byte (8 bits)
   *
   * @param data[] array of data
   * @param length data length
   *
   * @return 0 if error, else number of writed bytes
   **/
  uint8_t hdWrite(uint8_t data[], uint16_t length);

  /**
   * Add Box into queue for write to HD
   *
   * @note A data shoud be packed into a Box struct
   *
   * @param *item data package for queue
   *
   * @return 0 if error, else 1
   **/
  uint8_t hdWrite(Box *item);

  /**
   * Prepare data for write in HD.
   *
   * @note 1 byte (uint8_t) = HD_CMNDS_NUM bytes
   *
   * @param data payload for write in HD
   * @param boxArr pointer on array for converted bytes.
   *        Array mast have to contain HD_CMNDS_NUM length
   * @param cmnd HD register. Default = HD_WRITE_COMMAND
   **/
  void doCommands(uint8_t data, uint8_t boxArr[],
                  uint8_t cmnd = HD_WRITE_COMMAND);

  /**
   * @brief Move cursor to [col, row] position
   * 
   * @param[in] col Column position
   * @param[in] row Row position 
   */
  void setCursor(uint8_t col, uint8_t row);

  /**
   * @brief Begin print at [col, row] position
   * 
   * @param[in] col Column position
   * @param[in] row Row position
   * @param[in] cst String for write
   */
  void printAt(uint8_t col, uint8_t row, const char cst[]);

  /**
   * @brief Clear display and move cursor to home
   * 
   * @param[in] isEnd 
   */
  void clear(bool isEnd = true);

  /**
   * @brief Turn on display and backlight
   */
  void on(bool isEnd = true);

  /**
   * @brief Turn off display and backlight
   */
  void off(bool isEnd = true);

  /**
   * @brief Move display and cursor to home position
   */
  void home(bool isEnd = true);

  /**
   * @brief Move cursor at one position to right
   */
  void moveCursorRight(bool isEnd = true);

  /**
   * @brief Move cursor at one position to left
   */
  void moveCursorLeft(bool isEnd = true);

  /**
   * @brief Move display at one column to right
   */
  void moveDisplayRight(bool isEnd = true);

  /**
   * @brief Move display at one column to left
   */
  void moveDisplayLeft(bool isEnd = true);

  /**
   * @brief Get display width
   */
  inline uint8_t getWidth() const { return _chars; }

  /**
   * @brief Get display height
   */
  inline uint8_t getHeight() const { return _rows; }

  /**
   * @brief Get current index of cursor
   * 
   * @return current cursor index
   */
  inline uint8_t getCursorIndex() { return _cursorIndex; };

  /**
   * @brief Check error status of display
   * 
   * @return error status
   */
  inline uint8_t isError() { return _errorStatus; }

  /**
   * @brief Reset all errors of dispaly
   */
  inline void clearError() { _errorStatus = 0; };

  /**
   * @brief Add data to display queue
   * 
   * @return true - if data successful enqueue
   */
  bool enqueue(Box *);

  /**
   * @brief Writes data from queue to display if not empty
   */
  void checkQueue();

  /**
   * @brief Get queue size
   * 
   * @return queue size
   */
  inline uint16_t queueSize() { return _mQueue->size(); }

  /**
   * @brief Cleans data from queue if not empty
   */
  void qClean();
};

#endif // HD44780_H
