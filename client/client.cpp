#include <Arduino.h>
#include <Adafruit_ILI9341.h>
#include <SD.h>
#include "consts_and_types.h"
#include "map_drawing.h"

typedef unsigned long long ull;
//#include <vector>
//#include "serialport.h"
//#include <bits/stdc++.h>
//#include <boost/algorithm/string.hpp>
// the variables to be shared across the project, they are declared here!
shared_vars shared;

Adafruit_ILI9341 tft = Adafruit_ILI9341(clientpins::tft_cs, clientpins::tft_dc);

//SerialPort port("/dev/ttyACM0");
// max size of buffer, including null terminator
const uint32_t buf_size = 512;
// current number of chars in buffer, not counting null terminator
uint32_t buf_len = 0;

// input buffer
char* buff = (char *)malloc(buf_size);

void setup() {
  // initialize Arduino
  init();

  // initialize zoom pins
  pinMode(clientpins::zoom_in_pin, INPUT_PULLUP);
  pinMode(clientpins::zoom_out_pin, INPUT_PULLUP);

  // initialize joystick pins and calibrate centre reading
  pinMode(clientpins::joy_button_pin, INPUT_PULLUP);
  // x and y are reverse because of how our joystick is oriented
  shared.joy_centre = xy_pos(analogRead(clientpins::joy_y_pin), analogRead(clientpins::joy_x_pin));

  // initialize serial port
  Serial.begin(9600);
  Serial.flush(); // get rid of any leftover bits

  // initially no path is stored
  shared.num_waypoints = 0;

  // initialize display
  shared.tft = &tft;
  shared.tft->begin();
  shared.tft->setRotation(3);
  shared.tft->fillScreen(ILI9341_BLUE); // so we know the map redraws properly

  // initialize SD card
  if (!SD.begin(clientpins::sd_cs)) {
      Serial.println("Initialization has failed. Things to check:");
      Serial.println("* Is a card inserted properly?");
      Serial.println("* Is your wiring correct?");
      Serial.println("* Is the chipSelect pin the one for your shield or module?");

      while (1) {} // nothing to do here, fix the card issue and retry
  }

  // initialize the shared variables, from map_drawing.h
  // doesn't actually draw anything, just initializes values
  initialize_display_values();

  // initial draw of the map, from map_drawing.h
  draw_map();
  draw_cursor();

  Serial.flush();
  // initial status message
  status_message("FROM?");

  // set up buffer as empty string
    buf_len = 0;
    buff[buf_len] = 0;
}

void process_input() {
  // read the zoom in and out buttons
  shared.zoom_in_pushed = (digitalRead(clientpins::zoom_in_pin) == HIGH);
  shared.zoom_out_pushed = (digitalRead(clientpins::zoom_out_pin) == HIGH);

  // read the joystick button
  shared.joy_button_pushed = (digitalRead(clientpins::joy_button_pin) == LOW);

  // joystick speed, higher is faster
  const int16_t step = 64;

  // get the joystick movement, dividing by step discretizes it
  // currently a far joystick push will move the cursor about 5 pixels
  xy_pos delta(
    // the funny x/y swap is because of our joystick orientation
    (analogRead(clientpins::joy_y_pin)-shared.joy_centre.x)/step,
    (analogRead(clientpins::joy_x_pin)-shared.joy_centre.y)/step
  );
  delta.x = -delta.x; // horizontal axis is reversed in our orientation

  // check if there was enough movement to move the cursor
  if (delta.x != 0 || delta.y != 0) {
    // if we are here, there was noticeable movement

    // the next three functions are in map_drawing.h
    erase_cursor();       // erase the current cursor
    move_cursor(delta);   // move the cursor, and the map view if the edge was nudged
    if (shared.redraw_map == 0) {
      // it looks funny if we redraw the cursor before the map scrolls
      draw_cursor();      // draw the new cursor position
    }
  }
}

/* The following two functions are identical to the ones provided in the
assignment description. These functions take in the longitude and latitude
(respectively) and return the mapped location onto the screen*/
int16_t  lon_to_x(int32_t  lon, map_box_t mapbox, int16_t width) {
    return  map(lon , mapbox.W , mapbox.E , 0, width);
}


int16_t  lat_to_y(int32_t  lat, map_box_t mapbox, int16_t height) {
    return  map(lat , mapbox.N , mapbox.S , 0, height);
}


void drawWaypoints() {
    for (int i = 0; i < shared.num_waypoints - 1; i++) {
        int x1, x2, y1, y2;
        int8_t num = shared.map_number;
        x1 = lon_to_x(shared.waypoints[i].lat, mapdata::map_box[num], mapdata::map_x_limit[num]);
        x2 = lon_to_x(shared.waypoints[i + 1].lat, mapdata::map_box[num], mapdata::map_x_limit[num]);
        y1 = lat_to_y(shared.waypoints[i].lon, mapdata::map_box[num], mapdata::map_y_limit[num]);
        y2 = lat_to_y(shared.waypoints[i + 1].lon, mapdata::map_box[num], mapdata::map_y_limit[num]);
        shared.tft->drawLine(x1, y1, x2, y2, ILI9341_GREEN);
        shared.tft->drawLine(x1 -1, y1+1, x2 - 1, y2 + 1, ILI9341_GREEN);
        shared.tft->drawLine(x1 + 1, y1 - 1, x2 + 1, y2 - 1, ILI9341_GREEN);

    }
    //draw_map();
    //draw_cursor();
}


/** Reads an uint32_t from Serial3, starting from the least-significant
 * and finishing with the most significant byte. 
 */
ull ll_from_serial() {
  ull num = 0;
  num = num | ((ull) Serial.read()) << 0;
  num = num | ((ull) Serial.read()) << 8;
  num = num | ((ull) Serial.read()) << 16;
  num = num | ((ull) Serial.read()) << 24;  // 32 bits
  num = num | ((ull) Serial.read()) << 32;
  num = num | ((ull) Serial.read()) << 40;
  num = num | ((ull) Serial.read()) << 48;
  num = num | ((ull) Serial.read()) << 56;  // 64 bits = ll
  return num;
}

/*
// following two functions from simpleclient.cpp...
void get_line(lon_lat_32 point, int p) {
  // print what's in the buffer back to server
  //Serial.print("Got: ");
  //Serial.println(buffer);
    if (point.lat == 0) {
        point.lat = (int32_t)buff;
    } else if (point.lon == 0) {
        point.lon = (int32_t)buff;
        shared.waypoints[p] = point;
    }

  // clear the buffer
  buf_len = 0;
  buff[buf_len] = 0;
}

void readLoop(int index) {  // W ||||lat lon \n
  char in_char;
  lon_lat_32 point;
  point.lat = 0;
  point.lon = 0;
  if (Serial.available()) {
      // read the incoming byte:
      in_char = Serial.read();

      // if end of line is received, waiting for line is done:
      if (in_char == ' ') {
          // now we process the buffer
          get_line(point, index);
        } else if(in_char == '\n') {
            get_line(point, index);
            return;
        } else {
          // add character to buffer, provided that we don't overflow.
          // drop any excess characters.
          if ( buf_len < buf_size-1 ) {
              buff[buf_len] = in_char;
              buf_len++;
              buff[buf_len] = 0;
          }
        }
    }
}
*/

String readWaypoint(uint32_t timeout) {
    String word = "";
    char byte;
    int start = millis();

    while(true) {
        if (Serial.available()) {
            byte = Serial.read();
            if (byte != '\n' && byte != ' ') {
                word += byte;
            } else {
                return word;
            }
        } else if (millis() - start > timeout) {
            return "";
        }
    }
}
void sendRequest(lon_lat_32 start, lon_lat_32 end) {
  /*
    port.writeline("R");
    port.writeline(" ");
    port.writeline(start.lat);
    port.writeline(" ");
    port.writeline(start.lon);
    port.writeline(" ");
    port.writeline(end.lat);
    port.writeline(" ");
    port.writeline(end.lon);
    port.writeline("\n");
*/  
    Serial.flush();
    Serial.print("R ");
    Serial.print(start.lat);
    Serial.print(" ");
    Serial.print(start.lon);
    Serial.print(" ");
    Serial.print(end.lat);
    Serial.print(" ");
    Serial.print(end.lon);
    Serial.println();
    Serial.flush();
}

void sendAck() {
    // send the A character followed by a newline
    //port.writeline("A\n");
    Serial.println("A");
}

bool checkTimeout(bool timeout, int time, int startTime) {
    int endTime;
    while (!Serial.available()) {
        endTime = millis();
        if ((endTime-startTime) >= time) {
            timeout = true;
            break;
        } else {
            timeout = false;
        }
    }
    return timeout;
}

void clientCom(lon_lat_32 start, lon_lat_32 end) {
    status_message("Receiving path...");
    while (true) {
    bool timeout = false;
    // TODO: communicate with the server to get the waypoints

        int startTime = millis();
        //Serial.println(startTime);
        while (!timeout && !Serial.available()) {
            // send request
            sendRequest(start, end);
            timeout = checkTimeout(timeout, 10000, startTime);
            Serial.flush();
        }
        //Serial.println("sent...");
        // store path length in shared.num_waypoints
        //timeout = checkTimeout(timeout, 10000, startTime);
        //delay(3000);
        if (Serial.available() && !timeout) {
            status_message("inside loop");
            // string splitting method found from: 
            // geeksforgeeks.org/boostsplit-c-library/
            //char *line = new char[100];
            char letter;
            letter = Serial.read();
            Serial.read();  // read in space
            if (letter == 'N') {
                ull num = ll_from_serial();
                Serial.println();
                Serial.println();
                Serial.println();
                Serial.print("number received from server: ");
                Serial.println((int)num);
                Serial.flush();
                //delay(3000);
                if (num > 0) {
                    shared.num_waypoints = static_cast<int16_t>(num);
                    sendAck();
                    status_message("ACK sent");
                } else {
                    status_message("NO PATH");
                    // add a delay of 2-3 seconds...
                    delay(3000);
                    status_message("FROM?");
                    Serial.println("Broke out of loop");
                    timeout = false;
                    break;  // need to wait for new points
                }
            } else {
                // send request again with the same point
                status_message("TIMEOUT");
                delay(1000);
                timeout = true;
            }

            status_message("got here");
            // store waypoints in shared.waypoints[]
            for (int i = 0; i < shared.num_waypoints && !timeout; i++) {
                startTime = millis();
                timeout = checkTimeout(timeout, 1000, startTime);
                String letter = readWaypoint(1000);
                if (letter == "W") {
                    lon_lat_32 point;
                    point.lat = (int32_t)readWaypoint(1000).toInt();
                    point.lon = (int32_t)readWaypoint(1000).toInt();
                    shared.waypoints[i] = point;
                    status_message("got a waypoint");
                    sendAck();
                } else {
                    status_message("timeout");
                    timeout = true;
                }
                /*
                while(Serial.available() && i < shared.num_waypoints - 1) {
                    char letter = Serial.read();
                    Serial.read();  // for the space
                    if (letter == 'W') {
                        status_message("got a waypoint");
                        readLoop(i);
                        sendAck();
                        /*
                        while(i < shared.num_waypoints - 1) {
                            sendAck();
                            if (Serial.available()) {
                                status_message("broke out boi");
                                break;
                            }
                        }
                        
                    } else {
                        Serial.println("TIMEOUT");
                        // send reqeust again with the same point
                        timeout = true;
                    }
                }
                */
                /*
                char letter = Serial.read();
                Serial.read();
                if (letter == 'W') {
                    ull lat = ll_from_serial();
                    Serial.read();  // reading the space
                    ull lon = ll_from_serial();
                    lon_lat_32 Point;
                    Point.lat = static_cast<int32_t>(lat);
                    Point.lon = static_cast<int32_t>(lon);
                    shared.waypoints[i] = Point;
                    while(!Serial.available()) {
                        sendAck();
                    }
                    //sendAck();
                } else {
                    // send request again with the same point
                    timeout = true;
                }
                */
            }
            status_message("looking for end");
            // Serial.flush();
            startTime = millis();
            timeout = checkTimeout(timeout, 1, startTime);
            if (Serial.available() && !timeout){
                char endChar = Serial.read();
                if (endChar != 'E' && !timeout) {
                    // send request again with the same point
                    timeout = true;
                }
            }
        }
        status_message("drawing line.");
        drawWaypoints();
        //break;
        if (timeout) {
            status_message("TIMEOUT DAWG");
            delay(5000);
            continue;
        } else {
            status_message("drawing line");
            drawWaypoints();
            break;
        }
    }
}


int main() {
  setup();

  // very simple finite state machine:
  // which endpoint are we waiting for?
  enum {WAIT_FOR_START, WAIT_FOR_STOP} curr_mode = WAIT_FOR_START;

  // the two points that are clicked
  lon_lat_32 start, end;

  while (true) {
    // clear entries for new state
    shared.zoom_in_pushed = 0;
    shared.zoom_out_pushed = 0;
    shared.joy_button_pushed = 0;
    shared.redraw_map = 0;

    // reads the three buttons and joystick movement
    // updates the cursor view, map display, and sets the
    // shared.redraw_map flag to 1 if we have to redraw the whole map
    // NOTE: this only updates the internal values representing
    // the cursor and map view, the redrawing occurs at the end of this loop
    process_input();

    // if a zoom button was pushed, update the map and cursor view values
    // for that button push (still need to redraw at the end of this loop)
    // function zoom_map() is from map_drawing.h
    if (shared.zoom_in_pushed) {
      zoom_map(1);
      shared.redraw_map = 1;
    }
    else if (shared.zoom_out_pushed) {
      zoom_map(-1);
      shared.redraw_map = 1;
    }

    // if the joystick button was clicked
    if (shared.joy_button_pushed) {

      if (curr_mode == WAIT_FOR_START) {
        // if we were waiting for the start point, record it
        // and indicate we are waiting for the end point
        start = get_cursor_lonlat();
        curr_mode = WAIT_FOR_STOP;
        status_message("TO?");

        delay(500);
        // wait until the joystick button is no longer pushed
        while (digitalRead(clientpins::joy_button_pin) == LOW) {}
      }
      else {
        // if we were waiting for the end point, record it
        // and then communicate with the server to get the path
        end = get_cursor_lonlat();

        clientCom(start, end);
        // now we have stored the path length in
        // shared.num_waypoints and the waypoints themselves in
        // the shared.waypoints[] array, switch back to asking for the
        // start point of a new request
        curr_mode = WAIT_FOR_START;

        delay(500);
        // wait until the joystick button is no longer pushed
        while (digitalRead(clientpins::joy_button_pin) == LOW) {}
      }
    }

    if (shared.redraw_map) {
      // redraw the status message
      if (curr_mode == WAIT_FOR_START) {
        status_message("FROM?");
      }
      else {
        status_message("TO?");
      }

      // redraw the map and cursor
      draw_map();
      draw_cursor();

      // TODO: draw the route if there is one
    }
  }

  Serial.flush();
  return 0;
}
