#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include "WeatherIcons.h"
#include "wifi.h"

#define LEDPIN1 18
#define LEDPIN2 19
#define LEDPIN3 17
#define LEDPIN4 16

#define UP 0
#define DOWN 1
#define RIGHT 2
#define LEFT 3
#define brick_count 7

#define FULL 50  //display brightness 0-255

#define PCB_WIDTH 12
#define PCB_HEIGHT 15
#define FIELD_WIDTH 24
#define FIELD_HEIGHT 38
#define TOTAL_HEIGHT 60
#define WEATHER_WIDTH 48
#define WEATHER_HEIGHT 44
#define tick_delay_max 25  //max interval/speed, in ms
#define tick_delay_min 5   //min

//weight given to the highest column for ai
#define HIGH_COLUMN_WEIGHT 5
//weight given to the number of holes for ai
#define HOLE_WEIGHT 3

static PROGMEM const uint16_t bricks[brick_count][4] = {
  { 0b0100010001000100,  //1x4 cyan
    0b0000000011110000,
    0b0100010001000100,
    0b0000000011110000 },
  { 0b0000010011100000,  //T  purple
    0b0000010001100100,
    0b0000000011100100,
    0b0000010011000100 },
  { 0b0000011001100000,  //2x2 yellow
    0b0000011001100000,
    0b0000011001100000,
    0b0000011001100000 },
  { 0b0000000011100010,  //L orange
    0b0000010001001100,
    0b0000100011100000,
    0b0000011001000100 },
  { 0b0000000011101000,  //inverse L blue
    0b0000110001000100,
    0b0000001011100000,
    0b0000010001000110 },
  { 0b0000100011000100,  //S green
    0b0000011011000000,
    0b0000100011000100,
    0b0000011011000000 },
  { 0b0000010011001000,  //Z red
    0b0000110001100000,
    0b0000010011001000,
    0b0000110001100000 }
};  //different types of bricks and 4 rotations

//8 bit RGB colors of blocks
//RRRGGGBB
static const PROGMEM uint8_t brick_colors[brick_count] = {
  0b00011111,  //cyan
  0b10000010,  //purple
  0b11111100,  //yellow
  0b11101000,  //orange?
  0b00000011,  //blue
  0b00011100,  //green
  0b11100000   //red

};

String weatherIconType;
long weatherBlocks[WEATHER_WIDTH][WEATHER_HEIGHT];
unsigned long weather_lastTime = 0;
unsigned long weatherDelay = 900000;
byte tempArray[3] = { 0, 0, 0 };
byte windArray[4] = { 0, 0, 0, 0 };
float wind_degree;

//struct tm timeinfo;

//const char* ntpServer = "pool.ntp.org";
//const long gmtOffset_sec = -25200;
//const int daylightOffset_sec = 0;
//unsigned long previousMills = 0;

int tick_delay = tick_delay_max;
int line_cleared = 0;

byte wall[FIELD_WIDTH][FIELD_HEIGHT];
//The 'wall' is the 2D array that holds all bricks that have already 'fallen' into place

bool aiCalculatedAlready = false;

struct TAiMoveInfo {
  byte rotation;
  int positionX, positionY;
  int weight;
} aiCurrentMove;

struct TBrick {
  byte type;                 //This is the current brick shape.
  byte rotation;             //active brick rotation
  byte color;                //active brick color
  int positionX, positionY;  //active brick position
  byte pattern[4][4];        //2D array of active brick shape, used for drawing and collosion detection

} currentBrick;
Adafruit_NeoPixel strip[] = {
  Adafruit_NeoPixel(PCB_WIDTH * PCB_HEIGHT * 2 * 4, LEDPIN1, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(PCB_WIDTH* PCB_HEIGHT * 2 * 4, LEDPIN2, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(PCB_WIDTH* PCB_HEIGHT * 2 * 4, LEDPIN3, NEO_GRB + NEO_KHZ800),
  Adafruit_NeoPixel(PCB_WIDTH* PCB_HEIGHT * 2 * 4, LEDPIN4, NEO_GRB + NEO_KHZ800)
};
// Define the RGB pixel array and controller functions,
void setup() {
  for (int i = 0; i < 15; i++) {
    for (int j = 0; j < 15; j++) {
      for (int k = 0; k < 3; k++) {
        for(int l=0;l<=3;l++){
          arrowIcons[4+l][(j * 15 + i) * 3 + k] = arrowIcons[l][((14 - i) * 15 + j) * 3 + k];//90
          arrowIcons[8+l][(j * 15 + i) * 3 + k] = arrowIcons[l][((14 - j) * 15 + 14 - i) * 3 + k];//180
          arrowIcons[12+l][(j * 15 + i) * 3 + k] = arrowIcons[l][(i * 15 + 14 - j) * 3 + k];//270
        }
        //arrowIcons[2][(j * 15 + i) * 3 + k] = arrowIcons[0][((14 - i) * 15 + j) * 3 + k];
        //arrowIcons[3][(j * 15 + i) * 3 + k] = arrowIcons[1][((14 - i) * 15 + j) * 3 + k];  //rotate 90
        //arrowIcons[4][(j * 15 + i) * 3 + k] = arrowIcons[0][((14 - j) * 15 + 14 - i) * 3 + k];
        //arrowIcons[5][(j * 15 + i) * 3 + k] = arrowIcons[1][((14 - j) * 15 + 14 - i) * 3 + k];  //rotate 180
        //arrowIcons[6][(j * 15 + i) * 3 + k] = arrowIcons[0][(i * 15 + 14 - j) * 3 + k];
        //arrowIcons[7][(j * 15 + i) * 3 + k] = arrowIcons[1][(i * 15 + 14 - j) * 3 + k];  //rotate 270
      }
    }
  }
  pinMode(LEDPIN1, OUTPUT);
  pinMode(LEDPIN2, OUTPUT);
  pinMode(LEDPIN3, OUTPUT);
  pinMode(LEDPIN4, OUTPUT);
  randomSeed(analogRead(22));
  newGame();

  //Serial.begin(115200);
  WiFi.begin(ssid, password);
  //Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    //Serial.print(".");
  }
  //Serial.println("");
  //Serial.print("Connected to WiFi network with IP Address: ");
  //Serial.println(WiFi.localIP());

  ///////////////////////////////config ntpServer time///////////////////////////////////////////////////////
  /*configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return;
  }*/
}

void loop() {
  /*if (millis() - previousMills >= 1000) {
    pulseColor = pulseColor ? 0 : timeColor;
    previousMills = millis();
    if (!getLocalTime(&timeinfo)) {
      Serial.println("Failed to obtain time");
      return;
    }
  }*/
  if (weather_lastTime == 0 || (millis() - weather_lastTime > weatherDelay)) {
    if (WiFi.status() == WL_CONNECTED) {
      /////////////////////////////get weather from openWeatherMap/////////////////////////
      String serverPath = "http://api.openweathermap.org/data/2.5/weather?zip=" + zipcode + "," + countryCode + "&appid=" + openWeatherMapApiKey;
      String weatherString = httpGETRequest(serverPath.c_str());
      DynamicJsonDocument jsonBuffer(1024);
      DeserializationError error = deserializeJson(jsonBuffer, weatherString);
      if (error) {
        //.print(F("deserializeJson() failed: "));
        //Serial.println(error.f_str());
        return;
      }
      float temperature = (float)(jsonBuffer["main"]["temp"]) - 273.15;
      if (temperature < 0) {
        temperature = -temperature;
        tempArray[0] = 11;
        tempArray[1] = (int)temperature;
        tempArray[2] = (int)(temperature * 10 - tempArray[1] * 10);
      } else {
        tempArray[0] = (int)(temperature / 10);
        tempArray[1] = (int)(temperature - tempArray[0] * 10);
        tempArray[2] = (int)(temperature * 10 - tempArray[0] * 100 - tempArray[1] * 10);
      }
      float wind_speed = jsonBuffer["wind"]["speed"];
      windArray[0] = (int)(wind_speed / 10);
      windArray[1] = (int)(wind_speed - windArray[0] * 10);
      windArray[2] = (int)(wind_speed * 10 - windArray[0] * 100 - windArray[1] * 10);
      windArray[3] = (int)(wind_speed * 100 - windArray[0] * 1000 - windArray[1] * 100 - windArray[2] * 10);
      wind_degree = ((int)jsonBuffer["wind"]["deg"] + 180) % 360;//风向是来向
      weatherIconType = jsonBuffer["weather"][0]["icon"].as<String>();
      /*int humidity = jsonBuffer["main"]["humidity"];                    // get humidity
      float pressure = (float)(jsonBuffer["main"]["pressure"]) / 1000;  // get pressure
      float wind_speed = jsonBuffer["wind"]["speed"];                   // get wind speed
      unsigned long unix_time = jsonBuffer["dt"];
      int timezone = (int)jsonBuffer["timezone"] / 3600;*/
    } else {
      //Serial.println("WiFi Disconnected");
    }
    weather_lastTime = millis();
    drawWeather();
  }
  play();
}
String httpGETRequest(const char* serverName) {
  WiFiClient client;
  HTTPClient http;

  // Your Domain name with URL path or IP address with path
  http.begin(client, serverName);

  // Send HTTP POST request
  int httpResponseCode = http.GET();

  String payload = "{}";

  if (httpResponseCode > 0) {
    //.print("HTTP Response code: ");
    //Serial.println(httpResponseCode);
    payload = http.getString();
  } else {
    //Serial.print("Error code: ");
    //Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();

  return payload;
}
//plays the game!
void play() {

  //see how high the wall goes, end game early to save battery power
  if (getHighestColumn() > FIELD_HEIGHT - 2)
    newGame();

  if (currentBrick.positionY < 2) {
    moveDown();
    delay(tick_delay);
  } else {
    if (aiCalculatedAlready == false) {
      performAI();
    } else {
      byte command = getCommand();
      if (command == UP) {
        if (checkRotate(1) == true) {
          rotate(1);
          moveDown();
        } else
          moveDown();
      } else if (command == LEFT) {
        if (checkShift(-1, 0) == true) {
          shift(-1, 0);
          moveDown();
        } else
          moveDown();
      } else if (command == RIGHT) {
        if (checkShift(1, 0) == true) {
          shift(1, 0);
          moveDown();
        } else
          moveDown();
      }
      if (command == DOWN) {
        moveDown();
      }
      delay(tick_delay);
    }
  }
  drawGame();
}

//performs AI player calculations.
void performAI() {
  struct TBrick initialBrick;
  //save position of the brick in its raw state
  memcpy((void*)&initialBrick, (void*)&currentBrick, sizeof(TBrick));
  //stores our 10*FIELD_WIDTH possible AI moves
  struct TAiMoveInfo aiMoves[4 * FIELD_WIDTH];
  //counter keeps track of the current index into our aimoves array
  byte aiMoveCounter = 0;
  //save position of the the brick at the left most rotated position
  struct TBrick aiLeftRotatedBrick;
  //save position of the brick at the rotated position
  struct TBrick aiRotatedBrick;

  //first check the rotations(initial, rotated once, twice, thrice)
  for (int aiRotation = 0; aiRotation < 4; aiRotation++) {
    //rotate if possible
    if (checkRotate(1) == true)
      rotate(1);
    //save the rotated brick
    memcpy((void*)&aiRotatedBrick, (void*)&currentBrick, sizeof(TBrick));
    //shift as far left as possible
    while (checkShift(-1, 0) == true)
      shift(-1, 0);
    //save this leftmost rotated position
    memcpy((void*)&aiLeftRotatedBrick, (void*)&currentBrick, sizeof(TBrick));

    //now check each possible position of X
    for (int aiPositionX = 0; aiPositionX < FIELD_WIDTH; aiPositionX++) {
      //next move down until we can't
      while (checkGround() == false) {
        shift(0, 1);
      }
      //calculate ai weight of this particular final position
      int aiMoveWeight = aiCalculateWeight();
      //save the weight, positions and rotations for this ai move
      aiMoves[aiMoveCounter].weight = aiMoveWeight;
      aiMoves[aiMoveCounter].rotation = currentBrick.rotation;
      aiMoves[aiMoveCounter].positionX = currentBrick.positionX;
      aiMoves[aiMoveCounter].positionY = currentBrick.positionY;
      //move our index up for the next position to save to
      aiMoveCounter++;

      //now restore the previous position and shift it right by the column # we are checking
      memcpy((void*)&currentBrick, (void*)&aiLeftRotatedBrick, sizeof(TBrick));
      if (checkShift(aiPositionX + 1, 0) == true)
        shift(aiPositionX + 1, 0);
    }

    //reload rotated start position
    memcpy((void*)&currentBrick, (void*)&aiRotatedBrick, sizeof(TBrick));
  }

  //at this point we have calculated all the weights of every possible position and rotation of the brick

  //find move with lowest weight
  int lowestWeight = aiMoves[0].weight;
  int lowestWeightIndex = 0;
  for (int i = 1; i < aiMoveCounter; i++) {
    if (aiMoves[i].weight <= lowestWeight) {
      lowestWeight = aiMoves[i].weight;
      lowestWeightIndex = i;
    }
  }
  //save this AI move as the current move
  memcpy((void*)&aiCurrentMove, (void*)&aiMoves[lowestWeightIndex], sizeof(TAiMoveInfo));
  //restore original brick that we started with
  memcpy((void*)&currentBrick, (void*)&initialBrick, sizeof(TBrick));
  //update the brick, set the ai flag so we know that we dont need to recalculate
  updateBrickArray();
  aiCalculatedAlready = true;
}

//calculates the ai weight
//when this function is called, the currentBrick is moved into a final legal position at the bottom of the wall
//which is why we add it to the wall first and then remove it at the end
int aiCalculateWeight() {
  int weights = 0;
  //add to wall first before calculating ai stuffs
  addToWall();
  //get the two weights
  int highestColumn = getHighestColumn();
  int holeCount = getHoleCount();

  //if this position will yield a full completed row then its weight is 0, which is the lowest possible
  //remember the the lowest weight will be the best move to make
  if (getFullLinePossible() == true) {
    weights = 0;
  } else {
    weights = (HIGH_COLUMN_WEIGHT * highestColumn) + (HOLE_WEIGHT * holeCount);
  }
  removeFromWall();  //undo the wall addition when done
  return weights;
}


//returns how high the wall goes
int getHighestColumn() {
  int columnHeight = 0;
  int maxColumnHeight = 0;
  for (int j = 0; j < FIELD_WIDTH; j++) {
    columnHeight = 0;
    for (int k = FIELD_HEIGHT - 1; k != 0; k--) {
      if (wall[j][k] != 0) {
        columnHeight = FIELD_HEIGHT - k;
      }
    }
    if (columnHeight > maxColumnHeight)
      maxColumnHeight = columnHeight;
  }
  return maxColumnHeight;
}

//counts the number of given holes for the ai calculation
int getHoleCount() {
  int holeCount = 0;
  for (int j = 0; j < FIELD_WIDTH; j++) {
    for (int k = currentBrick.positionY + 2; k < FIELD_HEIGHT; k++) {
      if (wall[j][k] == 0) {
        int coveredHole = 1;
        for (int l = k; l >= 0; l--) {
          if (wall[j][l] != 0) {
            coveredHole++;
          }
        }
        holeCount = holeCount + coveredHole;
      }
    }
  }
  return holeCount;
}

//determines if a full line is possible given the current wall (for ai)
bool getFullLinePossible() {
  int lineCheck;
  for (byte i = 0; i < FIELD_HEIGHT; i++) {
    lineCheck = 0;
    for (byte k = 0; k < FIELD_WIDTH; k++) {
      if (wall[k][i] != 0)
        lineCheck++;
    }

    if (lineCheck == FIELD_WIDTH) {
      return true;
    }
  }
  return false;
}
//gets commands according to ai state
byte getCommand() {
  if (currentBrick.rotation != aiCurrentMove.rotation)
    return UP;
  if (currentBrick.positionX > aiCurrentMove.positionX)
    return LEFT;
  if (currentBrick.positionX < aiCurrentMove.positionX)
    return RIGHT;
  if (currentBrick.positionX == aiCurrentMove.positionX)
    return DOWN;
}

//checks if the next rotation is possible or not.
bool checkRotate(bool direction) {
  rotate(direction);
  bool result = !checkCollision();
  rotate(!direction);

  return result;
}

//checks if the current block can be moved by comparing it with the wall
bool checkShift(short right, short down) {
  shift(right, down);
  bool result = !checkCollision();
  shift(-right, -down);

  return result;
}

// checks if the block would crash if it were to move down another step
// i.e. returns true if the eagle has landed.
bool checkGround() {
  shift(0, 1);
  bool result = checkCollision();
  shift(0, -1);
  return result;
}

// checks if the block's highest point has hit the ceiling (true)
// this is only useful if we have determined that the block has been
// dropped onto the wall before!
bool checkCeiling() {
  for (int i = 0; i < 4; i++) {
    for (int k = 0; k < 4; k++) {
      if (currentBrick.pattern[i][k] != 0) {
        if ((currentBrick.positionY + k) < 0) {
          return true;
        }
      }
    }
  }
  return false;
}

//checks if the proposed movement puts the current block into the wall.
bool checkCollision() {
  int x = 0;
  int y = 0;

  for (byte i = 0; i < 4; i++) {
    for (byte k = 0; k < 4; k++) {
      if (currentBrick.pattern[i][k] != 0) {
        x = currentBrick.positionX + i;
        y = currentBrick.positionY + k;

        if (x >= 0 && y >= 0 && wall[x][y] != 0) {
          //this is another brick IN the wall!
          return true;
        } else if (x < 0 || x >= FIELD_WIDTH) {
          //out to the left or right
          return true;
        } else if (y >= FIELD_HEIGHT) {
          //below sea level
          return true;
        }
      }
    }
  }
  return false;  //since we didn't return true yet, no collision was found
}

//updates the position variable according to the parameters
void shift(short right, short down) {
  currentBrick.positionX += right;
  currentBrick.positionY += down;
}

// updates the rotation variable, wraps around and calls updateBrickArray().
// direction: 1 for clockwise (default), 0 to revert.
void rotate(bool direction) {
  if (direction == 1) {
    if (currentBrick.rotation == 0) {
      currentBrick.rotation = 3;
    } else {
      currentBrick.rotation--;
    }
  } else {
    if (currentBrick.rotation == 3) {
      currentBrick.rotation = 0;
    } else {
      currentBrick.rotation++;
    }
  }
  updateBrickArray();
}

void moveDown() {
  if (checkGround()) {
    addToWall();
    drawGame();
    if (checkCeiling()) {
      gameOver();
    } else {
      while (clearLine()) {
        //scoreOneUpLine();
      }
      nextBrick();
      //scoreOneUpBrick();
    }
  } else {
    //grounding not imminent
    shift(0, 1);
  }
  //scoreAdjustLevel();
  //ticks = 0;
}

//put the brick in the wall after the eagle has landed.
void addToWall() {
  for (byte i = 0; i < 4; i++) {
    for (byte k = 0; k < 4; k++) {
      if (currentBrick.pattern[i][k] != 0) {
        wall[currentBrick.positionX + i][currentBrick.positionY + k] = currentBrick.color;
      }
    }
  }
}

//removes brick from wall, used by ai algo
void removeFromWall() {
  for (byte i = 0; i < 4; i++) {
    for (byte k = 0; k < 4; k++) {
      if (currentBrick.pattern[i][k] != 0) {
        wall[currentBrick.positionX + i][currentBrick.positionY + k] = 0;
      }
    }
  }
}

//uses the currentBrick_type and rotation variables to render a 4x4 pixel array of the current block
// from the 2-byte binary reprsentation of the block
void updateBrickArray() {
  unsigned int data = pgm_read_word(&(bricks[currentBrick.type][currentBrick.rotation]));
  for (byte i = 0; i < 4; i++) {
    for (byte k = 0; k < 4; k++) {
      if (bitRead(data, 4 * i + 3 - k))
        currentBrick.pattern[k][i] = currentBrick.color;
      else
        currentBrick.pattern[k][i] = 0;
    }
  }
}
//clears the wall for a new game
void clearWall() {
  for (byte i = 0; i <= FIELD_WIDTH / 2; i++) {
    for (byte j = i; j < FIELD_HEIGHT - i; j++) {
      wall[i][j] = 0;
      wall[FIELD_WIDTH - i - 1][j] = 0;
    }
    for (byte k = i; k < FIELD_WIDTH - i; k++) {
      wall[k][i] = 0;
      wall[k][FIELD_HEIGHT - i - 1] = 0;
    }
    drawWall();
    updateDisplay();
    delay(400);
  }
}

// find the lowest completed line, do the removal animation, add to score.
// returns true if a line was removed and false if there are none.
bool clearLine() {
  int line_check;
  for (byte i = 0; i < FIELD_HEIGHT; i++) {
    line_check = 0;

    for (byte k = 0; k < FIELD_WIDTH; k++) {
      if (wall[k][i] != 0)
        line_check++;
    }

    if (line_check == FIELD_WIDTH) {
      flashLine(i);
      for (int k = i; k >= 0; k--) {
        for (byte m = 0; m < FIELD_WIDTH; m++) {
          if (k > 0) {
            wall[m][k] = wall[m][k - 1];
          } else {
            wall[m][k] = 0;
          }
        }
      }

      return true;  //line removed.
    }
  }
  return false;  //no complete line found
}

//randomly selects a new brick and resets rotation / position.
void nextBrick() {
  currentBrick.rotation = 0;
  currentBrick.positionX = round(FIELD_WIDTH / 2) - 2;
  currentBrick.positionY = -3;

  currentBrick.type = random(0, brick_count);

  currentBrick.color = pgm_read_byte(&(brick_colors[currentBrick.type]));

  aiCalculatedAlready = false;

  updateBrickArray();
}

//effect, flashes the line at the given y position (line) a few times.
void flashLine(int line) {

  bool state = 1;
  for (byte i = 0; i < 6; i++) {
    for (byte k = 0; k < FIELD_WIDTH; k++) {
      if (state)
        wall[k][line] = 0b11111111;
      else
        wall[k][line] = 0;
    }
    state = !state;
    drawWall();
    updateDisplay();
    delay(tick_delay * 2);
  }
  line_cleared++;
  if (line_cleared >= 10) {
    line_cleared = 0;
    tick_delay = (tick_delay >= (tick_delay_min + 5)) ? (tick_delay - 5) : tick_delay_min;
  }
}


//draws wall only, does not update display
void drawWall() {
  for (int j = 0; j < FIELD_WIDTH; j++) {
    for (int k = 0; k < FIELD_HEIGHT; k++) {
      draw(wall[j][k], FULL, j, k);
    }
  }
}

//'Draws' wall and game piece to screen array
void drawGame() {
  //draw the wall first
  drawWall();
  //now draw current piece in play
  for (int j = 0; j < 4; j++) {
    for (int k = 0; k < 4; k++) {
      if (currentBrick.pattern[j][k] != 0) {
        if (currentBrick.positionY + k >= 0) {
          draw(currentBrick.color, FULL, currentBrick.positionX + j, currentBrick.positionY + k);
          //field[ positionX + j ][ p osition_y + k ] = currentBrick_color;
        }
      }
    }
  }
  updateDisplay();
}

void drawWeather() {
  byte x, y, block_x, block_y, r, g, b, iconIndex;
  long color;
  long backgroundColor = 0x200020;
  long temperatureColor = 0x00FF00;
  long windColor = 0x1177D6;
  unsigned short address;
  byte offset_x = 1, offset_y = 1;
  for (int i = 0; i < WEATHER_WIDTH; i++) {
    for (int j = 0; j < WEATHER_HEIGHT; j++) {
      weatherBlocks[i][j] = backgroundColor;
    }
  }
  /////////////////////////////////////weather icon///////////////////////////////
  if (weatherIconType == "01d") {
    iconIndex = 0;
  } else if (weatherIconType == "01n") {
    iconIndex = 1;
  } else if (weatherIconType == "02d") {
    iconIndex = 2;
  } else if (weatherIconType == "02n") {
    iconIndex = 3;
  } else if (weatherIconType == "03d" || weatherIconType == "03n" || weatherIconType == "04d" || weatherIconType == "04n") {
    iconIndex = 4;
  } else if (weatherIconType == "09d" || weatherIconType == "09n" || weatherIconType == "10d" || weatherIconType == "10n") {
    iconIndex = 5;
  } else if (weatherIconType == "11d" || weatherIconType == "11n") {
    iconIndex = 6;
  } else if (weatherIconType == "13d" || weatherIconType == "13n") {
    iconIndex = 7;
  } else if (weatherIconType == "50d" || weatherIconType == "50n") {
    iconIndex = 8;
  }
  for (int i = 0; i < 21; i++) {
    for (int j = 0; j < 21; j++) {
      color = (weatherIcons[iconIndex][(j * 21 + i) * 3] << 16) + (weatherIcons[iconIndex][(j * 21 + i) * 3 + 1] << 8) + weatherIcons[iconIndex][(j * 21 + i) * 3 + 2];
      if (color > 0) {
        weatherBlocks[i + offset_x][j + offset_y] = color;
      }
    }
  }
  /////////////////////////////temperature numbers//////////////////////////////////////

  offset_y = 9;
  if (tempArray[0] > 0) {
    offset_x = 23;
  } else {
    offset_x = 21;
  }
  byte offset_x_1[4];
  ////////////////////////////
  offset_x_1[0] = 0;
  /////////////////////////////
  offset_x_1[1] = offset_x_1[0] + 6;
  if (tempArray[0] == 1) {
    offset_x_1[1]--;
  }
  if (tempArray[1] == 1) {
    offset_x_1[1]--;
    offset_x++;
  }
  /////////////////////////////////
  offset_x_1[2] = offset_x_1[1] + 8;
  if (tempArray[1] == 1) {
    offset_x_1[2]--;
  }
  if (tempArray[2] == 1) {
    offset_x_1[2]--;
    offset_x++;
  }
  //////////////////////////////////
  offset_x_1[3] = offset_x_1[2] + 5;
  if (tempArray[2] == 1) {
    offset_x_1[3]--;
  }
  /////////////////////////////////
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 7; j++) {
      if (tempArray[0] > 0) {
        weatherBlocks[i + offset_x + offset_x_1[0]][j + offset_y] = bitRead(digits[tempArray[0]][i], 6 - j) == 1 ? temperatureColor : backgroundColor;
      }
      weatherBlocks[i + offset_x + offset_x_1[1]][j + offset_y] = bitRead(digits[tempArray[1]][i], 6 - j) == 1 ? temperatureColor : backgroundColor;
      weatherBlocks[i + offset_x + offset_x_1[2]][j + offset_y] = bitRead(digits[tempArray[2]][i], 6 - j) == 1 ? temperatureColor : backgroundColor;
      weatherBlocks[i + offset_x + offset_x_1[3]][j + offset_y] = bitRead(digits[10][i], 6 - j) == 1 ? temperatureColor : backgroundColor;
    }
  }
  weatherBlocks[offset_x + offset_x_1[2] - (tempArray[2] == 1 ? 1 : 2)][offset_y + 6] = temperatureColor;
  //////////////////////////////wind arrow///////////////////////////////////
  offset_x = 4;
  offset_y = 25;
  wind_degree += 11.25;
  if (wind_degree >= 360) {
    wind_degree -= 360;
  }
  byte arrowType = (int)(wind_degree / 22.5);
  for (int i = 0; i < 15; i++) {
    for (int j = 0; j < 15; j++) {
      color = (arrowIcons[arrowType][(j * 15 + i) * 3] << 16) + (arrowIcons[arrowType][(j * 15 + i) * 3 + 1] << 8) + arrowIcons[arrowType][(j * 15 + i) * 3 + 2];
      if (color > 0) {
        weatherBlocks[i + offset_x][j + offset_y] = color;
      }
      if (color > 0) {
        weatherBlocks[i + offset_x][j + offset_y] = color;
      }
    }
  }
  /////////////////////////draw wind speed numbers//////////////////////////
  offset_x = 25;
  offset_y = 25;
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 7; j++) {
      if (windArray[0] > 0) {
        weatherBlocks[i + offset_x][j + offset_y] = bitRead(digits[windArray[0]][i], 6 - j) == 1 ? windColor : backgroundColor;
        weatherBlocks[i + offset_x + 6][j + offset_y] = bitRead(digits[windArray[1]][i], 6 - j) == 1 ? windColor : backgroundColor;
        weatherBlocks[i + offset_x + 14][j + offset_y] = bitRead(digits[windArray[2]][i], 6 - j) == 1 ? windColor : backgroundColor;
      } else {
        weatherBlocks[i + offset_x][j + offset_y] = bitRead(digits[windArray[1]][i], 6 - j) == 1 ? windColor : backgroundColor;
        weatherBlocks[i + offset_x + 8][j + offset_y] = bitRead(digits[windArray[2]][i], 6 - j) == 1 ? windColor : backgroundColor;
        weatherBlocks[i + offset_x + 14][j + offset_y] = bitRead(digits[windArray[3]][i], 6 - j) == 1 ? windColor : backgroundColor;
      }
    }
  }
  if (windArray[0] > 0) {
    weatherBlocks[offset_x + 12][offset_y + 6] = windColor;
  } else {
    weatherBlocks[offset_x + 6][offset_y + 6] = windColor;
  }
  //////////////////////////draw wind speed unit///////////////////////////////////////////////
  offset_x = 26;
  offset_y = 33;
  for (int i = 0; i < 5; i++) {
    for (int j = 0; j < 7; j++) {
      weatherBlocks[i + offset_x][j + offset_y] = bitRead(letters[0][i], 6 - j) == 1 ? windColor : backgroundColor;
      weatherBlocks[i + offset_x + 6][j + offset_y] = bitRead(letters[1][i], 6 - j) == 1 ? windColor : backgroundColor;
      weatherBlocks[i + offset_x + 12][j + offset_y] = bitRead(letters[2][i], 6 - j) == 1 ? windColor : backgroundColor;
    }
  }
  /////////////////////////////draw/////////////////////////////////
  for (int i = 0; i < WEATHER_WIDTH; i++) {
    for (int j = 0; j < WEATHER_HEIGHT; j++) {
      y = FIELD_HEIGHT * 2 + j;
      block_x = i / (PCB_WIDTH * 2);
      block_y = y / (PCB_HEIGHT * 2);
      x = i % (PCB_WIDTH * 2);
      y = y % (PCB_HEIGHT * 2);
      if (y % 2 == 0)
        address = PCB_WIDTH * PCB_HEIGHT * 4 * block_x + PCB_WIDTH * 2 * y + x;
      else
        address = PCB_WIDTH * PCB_HEIGHT * 4 * block_x + ((PCB_WIDTH * 2 * (y + 1)) - 1) - x;
      color = weatherBlocks[i][j];
      b = color & 0x0000FF;
      g = (color & 0x00FF00) >> 8;
      r = (color & 0xFF0000) >> 16;
      strip[block_y].setPixelColor(address, map(r, 0, 255, 0, FULL), map(g, 0, 255, 0, FULL), map(b, 0, 255, 0, FULL));
    }
  }
  ////////////////////////update last panel LEDs/////////////////////////////////////
  strip[3].show();
}

void draw(byte color, signed int brightness, byte x, byte y) {
  unsigned short address = 0;
  byte r, g, b;

  byte block_x1 = x / PCB_WIDTH;
  byte block_y1 = y / PCB_HEIGHT;
  byte x1 = x % PCB_WIDTH * 2;
  byte y1 = y % PCB_HEIGHT * 2;

  byte block_x2 = (x * 2 + 1) / (PCB_WIDTH * 2);
  byte block_y2 = (y * 2 + 1) / (PCB_HEIGHT * 2);
  byte x2 = x % PCB_WIDTH * 2 + 1;
  byte y2 = y % PCB_HEIGHT * 2 + 1;

  b = color & 0b00000011;
  g = (color & 0b00011100) >> 2;
  r = (color & 0b11100000) >> 5;
  brightness = constrain(brightness, 0, FULL);

  unsigned short address11 = PCB_WIDTH * PCB_HEIGHT * 4 * block_x1 + PCB_WIDTH * 2 * y1 + x1;
  unsigned short address12 = PCB_WIDTH * PCB_HEIGHT * 4 * block_x2 + PCB_WIDTH * 2 * y1 + x2;
  unsigned short address21 = PCB_WIDTH * PCB_HEIGHT * 4 * block_x1 + ((PCB_WIDTH * 2 * (y2 + 1)) - 1) - x1;
  unsigned short address22 = PCB_WIDTH * PCB_HEIGHT * 4 * block_x2 + ((PCB_WIDTH * 2 * (y2 + 1)) - 1) - x2;
  if (color == 0 || brightness < 0) {
    strip[block_y1].setPixelColor(address11, 0, 5, 5);
    strip[block_y1].setPixelColor(address12, 0, 5, 5);
    strip[block_y2].setPixelColor(address21, 0, 5, 5);
    strip[block_y2].setPixelColor(address22, 0, 5, 5);
  } else {
    strip[block_y1].setPixelColor(address11, map(r, 0, 7, 0, brightness), map(g, 0, 7, 0, brightness), map(b, 0, 3, 0, brightness));
    strip[block_y1].setPixelColor(address12, map(r, 0, 7, 0, brightness), map(g, 0, 7, 0, brightness), map(b, 0, 3, 0, brightness));
    strip[block_y2].setPixelColor(address21, map(r, 0, 7, 0, brightness), map(g, 0, 7, 0, brightness), map(b, 0, 3, 0, brightness));
    strip[block_y2].setPixelColor(address22, map(r, 0, 7, 0, brightness), map(g, 0, 7, 0, brightness), map(b, 0, 3, 0, brightness));
  }
}

//obvious function
void gameOver() {
  newGame();
}

//clean up, reset timers, scores, etc. and start a new round.
void newGame() {
  tick_delay = tick_delay_max;
  line_cleared = 0;
  clearWall();
  nextBrick();
}

//Update LED strips
void updateDisplay() {
  strip[0].show();
  strip[1].show();
  strip[2].show();
  //strip[3].show();
}