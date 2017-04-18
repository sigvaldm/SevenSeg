/*
  SevenSeg 1.2.1
  SevenSeg.h - Library for controlling a 7-segment LCD
  Copyright 2013, 2015, 2017 Sigvald Marholm <marholm@marebakken.com>

  This file is part of SevenSeg.

  SevenSeg is free software: you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  SevenSeg is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with SevenSeg.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef SevenSeg_h
#define SevenSeg_h

#include "Arduino.h"

class SevenSeg
{

  public:

    // Constructor
    SevenSeg(int,int,int,int,int,int,int);

    // Low level functions for initializing hardware
    void setCommonAnode();
    void setCommonCathode();
    void setDigitPins(int,int *);
    void setActivePinState(int,int);
    void setDPPin(int);
    void setColonPin(int);
    void setSymbPins(int,int,int,int);

    // Low level functions for printing to display
    void clearDisp();
    void changeDigit(int);
    void changeDigit(char);
    void writeDigit(int);
    void writeDigit(char);
    void setDP();
    void clearDP();
    void setColon();
    void clearColon();
    void setApos();
    void clearApos();

    // Low level functions for controlling multiplexing
    void setDigitDelay(long int);	// Should I have this function?
    void setRefreshRate(int);
    void setDutyCycle(int);

    // High level functions for printing to display
    void write(long int);
    void write(int);
    void write(long int,int);
    void write(int, int);
    void write(char*);
    void write(String);
    void write(double);
    void write(double num, int point);
    void writeClock(int,int,char);
    void writeClock(int,int);
    void writeClock(int,char);
    void writeClock(int);

    // Timer control functions
    void setTimer(int);
    void clearTimer();
    void interruptAction();
    void startTimer();
    void stopTimer();

    // To clean up
//  void setPinState(int);	// I think this isn't in use. Its called setActivePinState?
//  int getDigitDelay();	// How many get-functions should I make?

  private:

    // The pins for each of the seven segments (eight with decimal point)
    int _A;
    int _B;
    int _C;
    int _D;
    int _E;
    int _F;
    int _G;
    int _DP;	// -1 when decimal point not assigned

    // Variables used for colon and apostrophe symbols
    int _colonState;	// Whether colon is on (_segOn) or off (_segOff).
    int _aposState;	// Whether apostorphe is on (_segOn) or off (_segOff).
    int _colonSegPin;
    int _colonSegLPin;
    int _aposSegPin;
    int _symbDigPin;

    /* The colon/apostrophe handling needs some further explanation:
     *
     * colonSegPin is the segment pin for colon. I.e. some displays have a separate segment for colon on one of the digits.
     * others have colon split across two digits: i.e. the upper part has a separate segment on one digit, whereas the lower
     * part has uses the same segment pin but on another digit. It is assumed that this segment pin is only used for colon,
     * and it is stored into colonSegPin by setColonPin(int). The functions setColon() and clearColon() turns on/off this pin,
     * respectively.
     *
     * On some displays, colon is one or two separate free-standing LED(s) with its own cathode and anode. In case of common
     * cathode, ground the cathod and treat the anode(s) as a segment pin. The other way around in case of common anode. This
     * should make the method described above applicable.
     *
     * On other displays, the upper colon part, the lower colon part, as well as an apostrophe, shares segments with the usual
     * segments (i.e. segments A, B and C) but is treated as a separate symbol digit that must be multiplexed along with the
     * other digits. In this case the function setSymbPins(int,int,int,int) is used to assign a pin to that digit, stored in
     * symbDigPin. The pin corresponding to the upper colon segment is stored in colonSegPin, whereas the lower colon segment
     * is stored in colonSegLPin. aposSegPin holds the segment pin for the apostrophe. symbDigPin being unequal to -1 is an
     * indication for multiplexing-related functions that it must multiplex over _numOfDigits+1 digit pins. In this case, the
     * setColon(), clearColon(), setApos() and clearApos() does not directly influence the pin, but the variable colonState and
     * aposState. In this case, the digit must be changed to the symbol digit by issuing changeDigit('s') in order to show the
     * symbols.
     */

    // The pins for each of the digits
    int *_dig;
    int _numOfDigits;

    // Timing variables. Stored in microseconds.
    long int _digitDelay;		// How much time spent per display during multiplexing.
    long int _digitOnDelay;		// How much on-time per display (used for dimming), i.e. it could be on only 40% of digitDelay
    long int _digitOffDelay;		// digitDelay minus digitOnDelay
    int _dutyCycle;		// The duty cycle (digitOnDelay/digitDelay, here in percent)
    // Strictly speaking, _digitOnDelay and _digitOffDelay holds redundant information, but are stored so the computations only
    // needs to be made once. There's an internal update function to update them based on the _digitDelay and _dutyCycle

    void updDelay();
    void execDelay(int);	// Executes delay in microseconds
    char iaExtractDigit(long int,int,int);
    long int iaLimitInt(long int);

    // Sets which values (HIGH or LOW) pins should have to turn on/off segments or digits.
    // This depends on whether the display is Common Anode or Common Cathode.
    int _digOn;
    int _digOff;
    int _segOn;
    int _segOff;

    // Variables used by interrupt service routine to keep track of stuff
    int _timerDigit;		// What digit interrupt timer should update next time
    int _timerPhase;		// What phase of the cycle it is to update, i.e. phase 1 (on), or phase 0 (off). Needed for duty cycling.
    int _timerID;		// Values 0,1,2 corresponds to using timer0, timer1 or timer2.
    long int _timerCounter;		// Prescaler of 64 is used since this is available on all timers (0, 1 and 2).
				// Timer registers are not sufficiently large. This counter variable will extend upon the original timer.
				// and increment by one each time.
    long int _timerCounterOnEnd;	// How far _timerCounter should count to provide a delay approximately equal to _digitOnDelay
    long int _timerCounterOffEnd;	// How far _timerCounter should count to provide a delay approximately equal to _digitOffDelay

    // What is to be printed by interruptAction is determined by these variables
    long int _writeInt;		// Holds the number to be written in case of int, fixed point, or clock
    int _writePoint;		// Holds the number of digits to use as decimals in case of fixed point
//    float _writeFloat;		// Holds the float to write in case of float. OBSOLETE: Float are converted to fixed point
    char *_writeStr;		// Holds a pointer to a string to write in case of string
    char _writeMode;		// 'p' for fixed point, 'i' for integer, 'f' for float, ':'/'.'/'_' for clock with according divisor symbol
    String _writeStrObj;

};

#endif
