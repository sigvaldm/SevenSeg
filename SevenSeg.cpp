/*
  SevenSeg 1.2.1
  SevenSeg.cpp - Library for controlling a 7-segment LCD
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


#include "Arduino.h"
#include "SevenSeg.h"

// Constructor
SevenSeg::SevenSeg(int A,int B,int C,int D,int E,int F,int G){

  // Assume Common Anode (user must change this if false)
  setCommonAnode();

  // Set segment pins
  _A=A;
  _B=B;
  _C=C;
  _D=D;
  _E=E;
  _F=F;
  _G=G;
  _DP=-1;	// DP initially not assigned

  // Set all segment pins as outputs
  pinMode(_A, OUTPUT);
  pinMode(_B, OUTPUT);
  pinMode(_C, OUTPUT);
  pinMode(_D, OUTPUT);
  pinMode(_E, OUTPUT);
  pinMode(_F, OUTPUT);
  pinMode(_G, OUTPUT);

  // Assume no digit pins are used (i.e. it's only one hardwired digit)
  _numOfDigits=0;

  _colonState=_segOff;	// default off
  _aposState=_segOff;	// default off
  _colonSegPin=-1;	// -1 when not assigned
  _colonSegLPin=-1;	// -1 when not assigned
  _aposSegPin=-1;	// -1 when not assigned
  _symbDigPin=-1;	// -1 when not assigned

  // When no pins are used you need not multiplex the output and the delay is superfluous
  // TBD: Needed for duty cycle control. Add option to differentiate between 0 and 1 digit pins
  _digitDelay=0;
  _digitOnDelay=0;
  _digitOffDelay=0;
  _dutyCycle=100;

  // Timer data (default values when no timer is assigned)
  _timerDigit=0;
  _timerPhase=1;
  _timerID=-1;
  _timerCounter=0;
  _timerCounterOnEnd=0;
  _timerCounterOffEnd=0;

  _writeInt=0;
  _writePoint=0;
  _writeStr=0;
  _writeMode=' ';

  // Clear display
  clearDisp();
}

void SevenSeg::setCommonAnode(){
  _digOn=HIGH;
  _digOff=LOW;
  _segOn=LOW;
  _segOff=HIGH;
}

void SevenSeg::setCommonCathode(){
  _digOn=LOW;
  _digOff=HIGH;
  _segOn=HIGH;
  _segOff=LOW;
}

void SevenSeg::clearDisp(){

  for(int i=0;i<_numOfDigits;i++){
    digitalWrite(_dig[i], _digOff);
  }
  digitalWrite(_A, _segOff);
  digitalWrite(_B, _segOff);
  digitalWrite(_C, _segOff);
  digitalWrite(_D, _segOff);
  digitalWrite(_E, _segOff);
  digitalWrite(_F, _segOff);
  digitalWrite(_G, _segOff);

  if(_DP!=-1){	// Clear DP too if assigned
    digitalWrite(_DP, _segOff);
  }

  if(_symbDigPin!=-1){
    digitalWrite(_symbDigPin, _digOff);
  }

}

/* OLD METHOD
   This method works, but you have to make a (static) array outside the object and it
   will only be pointed to by this object. That's not very pretty since the object doesn't
   actually contain all the information about the display. Further on you rely on the array
   being declared in a persistent scope and that the user doesn't change it.
*/
void SevenSeg::setDigitPins(int numOfDigits, int *pDigitPins){
  _dig=pDigitPins;
  _numOfDigits=numOfDigits;

  for(int i=0;i<_numOfDigits;i++){
    pinMode(_dig[i],OUTPUT);
  }

  clearDisp();

  // Set the default refresh rate of 100 Hz. If the user wants another refresh rate this
  // would have to be set after the setDigitPins function.
  setRefreshRate(100);
}

void SevenSeg::setDigitDelay(long int delay){
  _digitDelay=delay;
  updDelay();
}

void SevenSeg::setDutyCycle(int dc){
  _dutyCycle=dc;
  updDelay();
}

void SevenSeg::setActivePinState(int segActive, int digActive){
  _digOn = digActive;
  _digOff = !digActive;
  _segOn = segActive;
  _segOff = !segActive;
}

void SevenSeg::setRefreshRate(int freq){
  long int period = 1000000L/freq;
  long int digitDelay = period/_numOfDigits;

  if(_symbDigPin!=-1){	// Separate symbol pin in use. One more digit to multiplex across.
    digitDelay = period/(_numOfDigits+1);
  }

  setDigitDelay(digitDelay);
}

/*
 * HIGH LEVEL WRITE-FUNCTIONS
 *
 * High-level write functions should work in the following way:
 *
 * writeClock(int aa, int bb) - writes a time in the following format aa:bb (or aa.bb if no colon, or aabb if no dp).
 * writeClock(int aa, int bb, char c) - where char c specifies decimator ("_" for no decimator)
 * writeClock(int bb) or writeClock(int bb,char c) - writes in the format aa:bb (or similar) where aa is deduced from bb.
 * write(int a)
 * write(float a)
 * write(int a, int point)
 * write(char* a)
 *
 * For the timer interrupt mode, the value must simply be stored to a private variable, and printed by interruptAction. For
 * storing, the following is needed:
 *
 * int _writePoint	- Stores the fixed point in case of writeFixed is used.
 * int _writeInt	- Stores the number for write(int), writeFixed() and writeClock() (convert to only one number bb for clock)
 * float _writeFloat	- Stores the float for write(float)
 * char* _writeStr	- Stores a pointer to a string to be written. This pointer must be maintained outside the class
 * String _writeStrObj	- Stores a string object to be written.
 * char _writeMode	- Describes which writing function/mode is used:
 *				i - write(int)
 *				f - write(float)
 *				p - write()
 *				s - write(char* a)
 *				o - write(String)
 *				: - writeClock() with colon as decimator
 *				. - writeClock() with period as decimator
 *				_ - writeClock() with no decimator
 *
 * Further on, all functions should be using timer if _timerID!=-1. If not, the should multiplex through the
 * display once and rely on the function being placed in a loop.
 *
 * Perhaps the write int/float functions need a separate private parsing function to extract the digits? Here's the algorithm:
 *
 *   if num > (10^_numOfDigits-1) or num < (-10^_(numOfDigits-1)+1)	// Store these values in object during digit pin assignment to save computation?
 *     Display a positive or negative overload
 *
 *   else display can handle number
 *
 *     num = 2468 						// example
 *     digit_0 = num / (10^(_numOfDigits-1))			// 2
 *     digit_1 = (num / (10^(_numOfDigits-2)))%10		// 4
 *     i_th_digit = (num / (10^(_numOfDigits-1-i)))%10		// 6 and 8 for i=2 and 3
 *
 *     IMPROVED:
 *     num = 2468						// exampmle
 *     digit_0 = num % 10;
 *     num /= 10;
 *     digit_1 = num % 10;
 *     num /= 10;
 *
 *	FIXED POINT:
 *	write similar to int, but write fp at correct position.
 *	num=1234, and fp=0 => 1234 (don't show .)
 *	num=1234, and fp=1 => 123.4
 *      num=1234, and fp=4 => 0.123 (option 1, too unpredictable and heavy)
 *	num=1234, and fp=4 => 1234 (simply don't show fp as it's invalid)
 *
 */

void SevenSeg::writeClock(int ss){

  writeClock(ss/60,ss%60);

}

void SevenSeg::writeClock(int ss, char c){

  writeClock(ss/60,ss%60,c);

}

void SevenSeg::writeClock(int mm, int ss){

  // Use ':' if assigned, '.' otherwise, or simply nothing if none assigned

  if(_colonSegPin!=-1){
    writeClock(mm,ss,':');
  } else if(_DP!=-1){
    writeClock(mm,ss,'.');
  } else {
    writeClock(mm,ss,'_');
  }

}

void SevenSeg::writeClock(int mm, int ss, char c){

  if(_timerID==-1){  // No timer assigned. MUX once.

    int num = mm*100+ss;

    // colon through symbpin? 1 if yes.
    int symbColon = (_symbDigPin!=-1);

    for(int i=_numOfDigits-1;i>=0;i--){
      changeDigit(i);
      int nextDigit = num % 10;
      writeDigit(nextDigit);       // Possible future update: don't write insignificant zeroes
      if(c==':' && !symbColon) setColon();
      if((c=='.')&&(i==_numOfDigits-3)) setDP();  // Only set "." in the right place
      num /= 10;
      execDelay(_digitOnDelay);
      if(c==':' && !symbColon) clearColon();
      if(c=='.') clearDP();
      writeDigit(' ');
      execDelay(_digitOffDelay);
    }

    if(symbColon && c==':'){
      changeDigit('s');
      setColon();
      execDelay(_digitOnDelay);
      clearColon();
      execDelay(_digitOffDelay);
    }

  } else {

    _writeMode=c;
    _writeInt=mm*100+ss;

  }

}

void SevenSeg::write(int num,int point){
  write((long int)num, point);
}

void SevenSeg::write(long int num,int point){

  if(_timerID==-1){  // No timer assigned. MUX once.

    // Compute the maximum positive and negative numbers possible to display
    // (TBD: Move this to a computation done on pin assignments?)
    long int maxNegNum=1;
    for(int i=1;i<=_numOfDigits-1;i++) maxNegNum*=10;
    long int maxPosNum=10*maxNegNum-1;
    maxNegNum=-maxNegNum+1;

    // TBD: Change to displaying OL (overload) or ---- or similar?
    if(num>maxPosNum) num=maxPosNum;
    if(num<maxNegNum) num=maxNegNum;

    if(point==0){    // Don't display decimal point if zero decimals used
      point=_numOfDigits;          // value if-sentence won't trigger on
    } else {
      point=_numOfDigits-point-1;  // Map number of decimal points to digit number
    }

    // TBD: Fix minus

    int minus=0;
    if(num<0){
      num*=-1;
      minus=1;
    }

/* USED IN v1.0 - DOESN'T SUPPRESS LEADING ZEROS
    for(int i=_numOfDigits-1;i>=0;i--){
      changeDigit(i);
      int nextDigit = num % 10L;
      if(minus&&i==0) writeDigit('-');
      else writeDigit(nextDigit);       // TBD: Possible future update: don't write insignificant zeroes
      if(point==i) setDP();
      num /= 10;
      execDelay(_digitOnDelay);
      writeDigit(' ');
      clearDP();
      execDelay(_digitOffDelay);
    }
*/

    for(int i=_numOfDigits-1;i>=0;i--){
        changeDigit(i);
        int nextDigit = num % 10L;
        if(num || i>point-1 || i==_numOfDigits-1){
            writeDigit(nextDigit);
        } else if(minus){
            writeDigit('-');
            minus=0;
        } else {
            writeDigit(' ');
        }
        if(point==i) setDP();
        num /= 10;
        execDelay(_digitOnDelay);
        writeDigit(' ');
        clearDP();
        execDelay(_digitOffDelay);
    }

  } else {  // Use timer

    if(point==0){    // Don't display decimal point if zero decimals used
      point=_numOfDigits;          // value if-sentence won't trigger on
    } else {
      point=_numOfDigits-point-1;  // Map number of decimal points to digit number
    }

	_writeMode = 'p';	// Tell interruptAction that write(int,int) was used (fixed point).
	_writeInt = iaLimitInt(num);	// Tell interruptAction to write this number ...
        _writePoint = point;	// ... with this fixed point
  }

}

// Extracts digit number "digit" from "number" for use with ia - interruptAction
char SevenSeg::iaExtractDigit(long int number, int digit, int point){

/* OLD VERSION WITHOU ZERO SUPPRESION (v1.0)
  if(number<0){
    if(digit==0) return '-';
    number*=-1;
  }
  for(int i=0;i<_numOfDigits-digit-1;i++) number/=10L;
  return (char)((number%10L)+48L);
*/

  long int old_number = number;
  int minus = 0;
  if(number<0){
    number*=-1;
    minus = 1;
  }

  if(digit!=_numOfDigits-1){
    for(int i=0;i<_numOfDigits-digit-1;i++) number/=10L;
  }

  if(digit>point-1 || digit==_numOfDigits-1 || number!=0) return (char)((number%10L)+48L);
  else {
    if(iaExtractDigit(old_number,digit+1,point)!='-' && iaExtractDigit(old_number,digit+1,point)!=' ' && minus) return '-';
    else return ' ';
  }

}

// Limits integer similar to how it's done in write(int,int)
long int SevenSeg::iaLimitInt(long int number){


    // Compute the maximum positive and negative numbers possible to display
    // (TBD: Move this to a computation done on pin assignments?)
    long int maxNegNum=1;
    for(int i=1;i<=_numOfDigits-1;i++) maxNegNum*=10;
    long int maxPosNum=10*maxNegNum-1;
    maxNegNum=-maxNegNum+1;

    // TBD: Change to displaying OL (overload) or ---- or similar?
    if(number>maxPosNum) number=maxPosNum;
    if(number<maxNegNum) number=maxNegNum;

    return number;

}

void SevenSeg::write(int num){
  write((long int)num);
}

void SevenSeg::write(long int num){

  if(_timerID==-1){  // No timer assigned. MUX once.

    write(num,0);

  } else {  // Use timer

	_writeMode = 'i';	// Tell interruptAction that write(int) is used.
	_writeInt = iaLimitInt(num);	// Tell interruptAction to write this int
  }

}

void SevenSeg::write(char *str){

  if(_timerID==-1){  // No timer assigned. MUX once.

    int i=0;
    int j=0;
    clearColon();
    while(str[i]!='\0'){
      changeDigit(j);
      writeDigit(str[i]);
      if(str[i+1]=='.'){
        setDP();
        i++;
      }
      execDelay(_digitOnDelay);
      writeDigit(' ');
      clearDP();
      execDelay(_digitOffDelay);
      i++;
      j++;
    }

  } else {  // Use timer
	_writeMode = 's';	// Tell interruptAction that write(char*) is used.
	_writeStr = str;	// Tell interruptAction to write this string
  }


}

void SevenSeg::write(String str){

  if(_timerID==-1){  // No timer assigned. MUX once.

    int i=0;
    int j=0;
    clearColon();
    while(i<str.length()){
      changeDigit(j);
      writeDigit(str[i]);
      if(str[i+1]=='.'){
        setDP();
        i++;
      }
      execDelay(_digitOnDelay);
      writeDigit(' ');
      clearDP();
      execDelay(_digitOffDelay);
      i++;
      j++;
    }

  } else {  // Use timer

	_writeMode = 'o';	// Tell interruptAction that write(String) is used.
	_writeStrObj = str;	// Tell interruptAction to write this string
  }


}

void SevenSeg::write(double num, int point){
    for(int i=0;i<point;i++) num*=10;
    long int intNum = (long int) num;
    double remainder = num - intNum;
    if(remainder>=0.5 && num>0) intNum++;
    if(remainder<=0.5 && num<0) intNum--;
    write(intNum,point);
}

void SevenSeg::write(double num){

    long int intNum;
    int point = 0;

    if(num>-1 && num<1){

        for(point = 0; point<_numOfDigits-1; point++) num *= 10;

        intNum = (long int)(num + 0.5 - (num<0));

        if(intNum < 0){
            num /= 10;
            point--;
            intNum = (long int)(num + 0.5 - (num<0));
        }

    } else {

        int digitsBeforeDecimal = 1;
        for(long int i = (long int) num; i != 0; i /= 10) digitsBeforeDecimal++;

        point = _numOfDigits - digitsBeforeDecimal - (num<0) + 1;
        for(int d=0; d<point; d++) num *= 10;

        intNum = (long int)(num + 0.5 - (num<0));

    }

    if(_timerID==-1){

        write(intNum,point);

    } else { // user timer

        // Adapting to another format
        point=point+1-_numOfDigits;

        _writeMode='p';
        _writePoint=-point;
        _writeInt=(long int)num;

    }

}

void SevenSeg::updDelay(){

  // On-time for each display is total time spent per digit times the duty cycle. The
  // off-time is the rest of the cycle for the given display.

  long int temp = _digitDelay;		// Stored into long int since temporary variable gets larger than 32767
  temp *= _dutyCycle;			// Multiplication in this way to prevent multiplying two "shorter" ints.
  temp /= 100;				// Division after multiplication to minimize round-off errors.
  _digitOnDelay=temp;
  _digitOffDelay=_digitDelay-_digitOnDelay;

  if(_timerID!=-1){
    // Artefacts in duty cycle control appeared when these values changed while interrupts happening (A kind of stepping in brightness appeared)
    cli();
    _timerCounterOnEnd=(_digitOnDelay/16)-1;
    _timerCounterOffEnd=(_digitOffDelay/16)-1;
    if(_digitOnDelay==0) _timerCounterOnEnd=0;
    if(_digitOffDelay==0) _timerCounterOffEnd=0;
//    _timerCounter=0;
    sei();
  }
}

void SevenSeg::interruptAction(){

  // Increment the library's counter
  _timerCounter++;

  // Finished with on-part. Turn off digit, and switch to the off-phase (_timerPhase=0)
  if((_timerCounter>=_timerCounterOnEnd)&&(_timerPhase==1)){
    _timerCounter=0;
    _timerPhase=0;

    writeDigit(' ');

    // If a write mode using . is used it is reasonable to assume that DP exists. Clear it (eventhough it might not be on this digit).
    if(_writeMode=='p'||_writeMode=='.'||_writeMode=='s') clearDP();
    if(_writeMode==':') clearColon();

  }

  // Finished with the off-part. Switch to next digit and turn it on.
  if((_timerCounter>=_timerCounterOffEnd)&&(_timerPhase==0)){
    _timerCounter=0;
    _timerPhase=1;

    _timerDigit++;

    if(_timerDigit>=_numOfDigits){
      if(_symbDigPin!=-1 && _timerDigit==_numOfDigits){  // Symbol pin in use. Let _timerDigit=_numOfDigits be used for symbol mux.
      } else { // Finished muxing symbol digit, or symbol pin not in use
        _timerDigit=0;
      }
    }

    if(_timerDigit==_numOfDigits) changeDigit('s');
    else changeDigit(_timerDigit);

    if(_timerDigit!=_numOfDigits){

        if(_writeMode=='p'){	// Fixed point writing (or float)
          writeDigit(iaExtractDigit(_writeInt,_timerDigit,_writePoint));
          if(_writePoint==_timerDigit && _writePoint!=_numOfDigits-1) setDP();
        }

        if(_writeMode=='i'){	// Integer writing
          writeDigit(iaExtractDigit(_writeInt,_timerDigit,_numOfDigits));
        }

        if(_writeMode==':'||_writeMode=='.'||_writeMode=='_'){

          // colon through symbpin? 1 if yes.
          int symbColon = (_symbDigPin!=-1);

          if(_timerDigit==_numOfDigits){	// Symbol digit
            setColon();
          } else {
            writeDigit(iaExtractDigit(_writeInt,_timerDigit,_numOfDigits));
            if(_writeMode==':' && !symbColon) setColon();
            if((_writeMode=='.')&&(_timerDigit==_numOfDigits-3)) setDP();  // Only set "." in the right place
          }

        }

        if(_writeMode=='s'){

          // This algorithm must count to the correct letter i in _writeStr for digit j, since the two may be unmatched
          // and it is impossible to know which letter to write without counting
          int i=0; // which digit
          int j=0; // which digit have it counted to
          while(_writeStr[i]!='\0' && j<_timerDigit){
            if(_writeStr[i+1]=='.'){
              i++;
            }
            i++;
            j++;
          }
          writeDigit(_writeStr[i]);
          if(_writeStr[i+1]=='.') setDP();

        }

        if(_writeMode=='o'){

          // This algorithm must count to the correct letter i in _writeStr for digit j, since the two may be unmatched
          // and it is impossible to know which letter to write without counting
          int i=0; // which digit
          int j=0; // which digit have it counted to
          while(i<_writeStrObj.length() && j<_timerDigit){
            if(_writeStrObj[i+1]=='.'){
              i++;
            }
            i++;
            j++;
          }
          writeDigit(_writeStrObj[i]);
          if(_writeStrObj[i+1]=='.') setDP();

        }
    }

  }

}

void SevenSeg::changeDigit(int digit){


  // Turn off all digits/segments first.
  // If you swith on a new digit before turning off the segments you will get
  // a slight shine of the "old" number in the "new" digit.
  clearDisp();
  digitalWrite(_dig[digit], _digOn);

}

void SevenSeg::changeDigit(char digit){

  if(digit=='s'){
    // change to the symbol digit
    clearDisp();
    digitalWrite(_symbDigPin, _digOn);
    digitalWrite(_colonSegPin, _colonState);
    digitalWrite(_colonSegLPin, _colonState);
    digitalWrite(_aposSegPin, _aposState);
  }

  if(digit==' '){
    clearDisp();
  }

}

void SevenSeg::setDPPin(int DPPin){

  _DP=DPPin;
  pinMode(_DP, OUTPUT);

}

void SevenSeg::setDP(){

  digitalWrite(_DP, _segOn);

}

void SevenSeg::clearDP(){

  digitalWrite(_DP, _segOff);

}
/*
    void setDPPin(int);
    void setDP();
    void clearDP();

Characters: Colon, apostrophe, comma(DP), randomly assignable symbols?
Most symbols can be confined to one character, but colon should be able to assign in two parts (UC and LC).
Yet two colons are also present on some displays. And on some displays, colons and apostorphes are treated as a separate digit using an additional common cathode/anode.

I've decided to treat the symbols in the following way

  DP is assigned as an eight segment for each digit, as this is the only way I've seen it done. Simple.
  I want the functions setDPPin(int), setDP(), clearDP(). DP should be cleared at each changeDigit (i.e. in ClearDisp?)
  In the parsing function it should be possible to write "1.2.3.4.".

  Colon are treated in many different ways on many different displays. I want a function setColonPin() that are overloaded
  and take most of the case. setColon() and clearColon() should turn it on or off. I'll explain the scenarios, and the syntax:

    Colon may be split in two parts UC (upper colon) and LC (lower colon) or colon may be hardwired as one LED

    1. Colon has its own cathodes and anodes. Ground the cathode if common cathode or tie anode to supply in case of common anode.
       Syntax: setColonPin(segPin) where segPin is the other pin. In this case the colon needs not be multiplexed. If split segment pin
       for UC/LC, the user joins them together. setColon()/clearColon() writes directly to segmentPin.
    2. UC/LC is joined together using its own segment pin, and shares common anode/cathode with one of the digits.
       Syntax: setColonPin(segPin,digPin). The function will detect at what digit to type the colon based on digitPin, and store this in colonDigU and
       colonDigL. If no digit is tied to digitPin issue an error. colonState is a private member variable being set or cleared by setColon()/clearColon().
       writeDigit() checks colonState when on colonDigU or colonDigL digits and writes accordingly.
    3. UC shares common anode/cathode digit pin with one of the digits, while LC shares with another digit. They are the same segment pin.
       Syntax: setColonPin(segPin,digLPin,digUPin). This works in the same way as above, except that different values are stored to DigU and DigL.
    4. UC and LC are treated like seperate segments on a new "symbol" digit pin(!). In these cases there is usually also an apostrophe (A) segment.
       The UC and LC segments should be joined together into one segment (C).
       Syntax: setSymbolPin(SegCPin,SegAPin,digPin) digPin will be stored to symbDigPin, and the multiplexing must occur over one additional digit.
       This implies modification to i.e. setRefreshRate, setDigitDelay and maybe other functions. All functions must be checked.

    Members needed in class:

      private:
        colonState	;	// _segOn or _segOff.
	aposState;	// _segOn or _segOff.
        colonDigU;	// Which digits to activate colon at (one digit for UC and one for LC)
	colonDigL;	// colonDigU==colonDigL==-1 means that it is treated as a separate digit, case 1 or 4. (check negative numbers for int)
	symbDigPin;	// If the colon is on a separate digit pin the symbol digit pin number is stored here. Otherwise, it is -1. Non-zero values imply more muxing.
        colonSegPin;
	aposSegPin;
      public:
	setColonPin(int);
        setColonPin(int,int);
	setColonPin(int,int,int);
        setSymbPin(int,int,int,int);
        setColon();
        clearColon();
	setApos();
	clearApos();

  The parsing function should parse colon to off except when any colon present in string. Same with apostorphe. I.e. "34:07". I initially wanted to have support for two
  colons since you need that on a watch. However, I've settled on only one colon since there are almost none display available with two colons. If I find one, and will
  use one, I will simply duplicate the colon stuff in my class.

  Actually, cases 1, 2 and 3 can be joined together! The colon can be turned on irrespective of what digits they are on (at least as long as there are only one colon).
  This simplifies the class:

      private:
        colonState;	// _segOn or _segOff.
	aposState;	// _segOn or _segOff.
	symbDigPin;	// If the colon is on a separate digit pin the symbol digit pin number is stored here. Otherwise, it is -1. Non-zero values imply more muxing.
        colonSegPin;
	aposSegPin;
      public:
	setColonPin(int);
        setSymbPin(int,int,int,int);
        setColon();
        clearColon();
	setApos();
	clearApos();

*/

void SevenSeg::setColonPin(int colonPin){
  _colonSegPin=colonPin;
  pinMode(_colonSegPin,OUTPUT);
  digitalWrite(_colonSegPin, _colonState);
}

void SevenSeg::setSymbPins(int digPin, int segUCPin, int segLCPin, int segAPin){
  _colonSegPin=segUCPin;
  _colonSegLPin=segLCPin;
  _aposSegPin=segAPin;
  _symbDigPin=digPin;
  _aposState=_segOff;
  _colonState=_segOff;
  pinMode(_colonSegPin,OUTPUT);
  pinMode(_colonSegLPin,OUTPUT);
  pinMode(_aposSegPin,OUTPUT);
  pinMode(_symbDigPin,OUTPUT);
  digitalWrite(_colonSegPin, _colonState);
  digitalWrite(_colonSegLPin, _colonState);
  digitalWrite(_aposSegPin, _aposState);
}

/*
The functions for setColon(), clearColon(), setApos(), clearApos() directly sets or clears the
segment pins if no symbol pin is assigned. Since no symbol pin is assigned colon (apos isn't set) has
a separate segment pin "Colon" and shares a digit pin with one or two other digits
(in case it is split into UC and LC). In this case it makes sense to control it just like other
segments; by setting and clearing the segment pin with setColon() or clearColon() after the correct
digit is selected with changeDigit(). Compare with setDP()/clearDP(). Furthermore, it is not necessary
to identify WHICH digit the colon segments apply for since, if colon is turned on, one may simply switch
on the segment pin for all digits. Nothing will be tied to the colon segment pin for other digits than
those it applies to, hence it is sufficient to initialize this kind of hardware with setColon(int colonPin).
Sometimes, a colon is present as one or two complete stand-alone LEDs. In this case, the can be wired
up into one of these configurations to work.

In the other main case, a separate symbol pin is assigned for colon and apostrophe. This is actually an
additional digit pin which must be muxed across. The segment pins are shared with other segments such as
A-G. This is a compact way of allowing many symbols (colon and apostrophe) while only adding one more pin.
This configuration is programmed with setSymbPins(int digPin, int segUCPin, int segLCPin, int segAPin),
where digPin is the symbol digit pin and the other pins are the pins used for segment UC, LC and apos.
If colon is present as one segment only, segUCPin and segULPin can be the same value. Sometimes, colon
and apostorphe are present as stand-alone diodes with their own cathodes and anodes not being connected
to anything else. In this case, join their cathodes or anodes (in case of common cathode or anode respectively)
and connect their other terminal to one segment pin each to make the mentioned configuration.
The behaviour set/clear behaviour of these digits are a bit different in this case. The set/clear-funciton
only sets a flag to on or off. In order to type the characters you must mux to the symbol digit by issuing
changeDigit('s'). This function will light up the appropriate symbols in accordance with the flags.
*/

void SevenSeg::setColon(){
  _colonState=_segOn;
  if(_symbDigPin==-1){
    digitalWrite(_colonSegPin, _segOn);
  }
}

void SevenSeg::clearColon(){
  _colonState=_segOff;
  if(_symbDigPin==-1){
    digitalWrite(_colonSegPin, _segOff);
  }
}

void SevenSeg::setApos(){
  _aposState=_segOn;
  if(_symbDigPin==-1){
    digitalWrite(_aposSegPin, _segOn);
  }
}

void SevenSeg::clearApos(){
  _aposState=_segOff;
  if(_symbDigPin==-1){
    digitalWrite(_aposSegPin, _segOff);
  }
}

void SevenSeg::writeDigit(int digit){

  // Turn off all LEDs first to avoid running current through too many LEDs at once.
  digitalWrite(_A, _segOff);
  digitalWrite(_B, _segOff);
  digitalWrite(_C, _segOff);
  digitalWrite(_D, _segOff);
  digitalWrite(_E, _segOff);
  digitalWrite(_F, _segOff);
  digitalWrite(_G, _segOff);

  if(digit==1){
    digitalWrite(_B, _segOn);
    digitalWrite(_C, _segOn);
  }

  if(digit==2){
    digitalWrite(_A, _segOn);
    digitalWrite(_B, _segOn);
    digitalWrite(_G, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_D, _segOn);
  }

  if(digit==3){
    digitalWrite(_A, _segOn);
    digitalWrite(_B, _segOn);
    digitalWrite(_G, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_D, _segOn);
  }

  if(digit==4){
    digitalWrite(_F, _segOn);
    digitalWrite(_G, _segOn);
    digitalWrite(_B, _segOn);
    digitalWrite(_C, _segOn);
  }

  if(digit==5){
    digitalWrite(_A, _segOn);
    digitalWrite(_F, _segOn);
    digitalWrite(_G, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_D, _segOn);
  }

  if(digit==6){
    digitalWrite(_A, _segOn);
    digitalWrite(_F, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_D, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_G, _segOn);
  }

  if(digit==7){
    digitalWrite(_A, _segOn);
    digitalWrite(_B, _segOn);
    digitalWrite(_C, _segOn);
  }

  if(digit==8){
    digitalWrite(_A, _segOn);
    digitalWrite(_B, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_D, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_F, _segOn);
    digitalWrite(_G, _segOn);
  }

  if(digit==9){
    digitalWrite(_G, _segOn);
    digitalWrite(_F, _segOn);
    digitalWrite(_A, _segOn);
    digitalWrite(_B, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_D, _segOn);
  }

  if(digit==0){
    digitalWrite(_A, _segOn);
    digitalWrite(_B, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_D, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_F, _segOn);
  }

}

void SevenSeg::writeDigit(char digit){

  // Turn off all LEDs first. Run writeDigit(' ') to clear digit.
  digitalWrite(_A, _segOff);
  digitalWrite(_B, _segOff);
  digitalWrite(_C, _segOff);
  digitalWrite(_D, _segOff);
  digitalWrite(_E, _segOff);
  digitalWrite(_F, _segOff);
  digitalWrite(_G, _segOff);

  if(digit=='-'){
    digitalWrite(_G, _segOn);
  }

  if(digit=='\370'){ // ASCII code 248 or degree symbol: '°'
    digitalWrite(_A, _segOn);
    digitalWrite(_B, _segOn);
    digitalWrite(_F, _segOn);
    digitalWrite(_G, _segOn);
  }

  // Digits are numbers. Write with writeDigit(int)
  if(digit>=48&&digit<=57)  writeDigit(digit-48);

  // Digits are small caps letters. Capitalize.
  if(digit>=97&&digit<=122) digit-=32;

  if(digit=='A'){
    digitalWrite(_A, _segOn);
    digitalWrite(_B, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_F, _segOn);
    digitalWrite(_G, _segOn);
  }

  if(digit=='B'){
    digitalWrite(_C, _segOn);
    digitalWrite(_D, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_F, _segOn);
    digitalWrite(_G, _segOn);
  }

  if(digit=='C'){
    digitalWrite(_A, _segOn);
    digitalWrite(_D, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_F, _segOn);
  }

  if(digit=='D'){
    digitalWrite(_B, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_D, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_G, _segOn);
  }

  if(digit=='E'){
    digitalWrite(_A, _segOn);
    digitalWrite(_D, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_F, _segOn);
    digitalWrite(_G, _segOn);
  }

  if(digit=='F'){
    digitalWrite(_A, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_F, _segOn);
    digitalWrite(_G, _segOn);
  }

  if(digit=='G'){
/*
    digitalWrite(_A, _segOn);
    digitalWrite(_B, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_D, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_G, _segOn);
    // TBD: Really write G like a 9, when it can be written as almost G?
*/
    digitalWrite(_A, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_D, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_F, _segOn);
  }

  if(digit=='H'){
    digitalWrite(_B, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_F, _segOn);
    digitalWrite(_G, _segOn);
  }

  if(digit=='I'){
    digitalWrite(_E, _segOn);
    digitalWrite(_F, _segOn);
  }

  if(digit=='J'){
    digitalWrite(_B, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_D, _segOn);
    digitalWrite(_E, _segOn);
  }

  if(digit=='K'){
    digitalWrite(_B, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_F, _segOn);
    digitalWrite(_G, _segOn);
  }

  if(digit=='L'){
    digitalWrite(_D, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_F, _segOn);
  }

  if(digit=='M'){
    digitalWrite(_A, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_E, _segOn);
  }

  if(digit=='N'){
    digitalWrite(_C, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_G, _segOn);
  }

  if(digit=='O'){
    digitalWrite(_A, _segOn);
    digitalWrite(_B, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_D, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_F, _segOn);
  }

  if(digit=='P'){
    digitalWrite(_A, _segOn);
    digitalWrite(_B, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_F, _segOn);
    digitalWrite(_G, _segOn);
  }

  if(digit=='Q'){
    digitalWrite(_A, _segOn);
    digitalWrite(_B, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_F, _segOn);
    digitalWrite(_G, _segOn);
  }

  if(digit=='R'){
    digitalWrite(_E, _segOn);
    digitalWrite(_G, _segOn);
  }

  if(digit=='S'){
    digitalWrite(_A, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_D, _segOn);
    digitalWrite(_F, _segOn);
    digitalWrite(_G, _segOn);
  }

  if(digit=='T'){
    digitalWrite(_D, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_F, _segOn);
    digitalWrite(_G, _segOn);
  }

 if(digit=='U'){
    digitalWrite(_B, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_D, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_F, _segOn);
  }

  if(digit=='V'){
    digitalWrite(_C, _segOn);
    digitalWrite(_D, _segOn);
    digitalWrite(_E, _segOn);
  }

  if(digit=='W'){
    digitalWrite(_B, _segOn);
    digitalWrite(_D, _segOn);
    digitalWrite(_F, _segOn);
  }

  if(digit=='X'){
    digitalWrite(_B, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_F, _segOn);
    digitalWrite(_G, _segOn);
  }

  if(digit=='Y'){
    digitalWrite(_B, _segOn);
    digitalWrite(_C, _segOn);
    digitalWrite(_D, _segOn);
    digitalWrite(_F, _segOn);
    digitalWrite(_G, _segOn);
  }

  if(digit=='Z'){
    digitalWrite(_A, _segOn);
    digitalWrite(_B, _segOn);
    digitalWrite(_D, _segOn);
    digitalWrite(_E, _segOn);
    digitalWrite(_G, _segOn);
  }
}

void SevenSeg::execDelay(int usec){

  if(usec!=0){	// delay() and delayMicroseconds() don't handle 0 delay

    if(usec<=16383)	delayMicroseconds(usec);	// maximum value for delayMicroseconds();
    else		delay(usec/1000);

  }

}
/*
 * INTERRUPT TIMER FUNCTIONS (PLATFORM DEPENDENT)
 */

#if defined(__AVR_ATmega168__) ||defined(__AVR_ATmega168P__) ||defined(__AVR_ATmega328P__)

void SevenSeg::setTimer(int timerID){

/*
  Assigns timer0, timer1 or timer2 solely to the task of multiplexing the display (depending on
  the value of timerNumber).

  For an example of a 5 digit display with 100Hz refresh rate and able to resolve duty cycle in
  10%-steps a timing of the following resolution is needed:

    1/( 100Hz * 5 digits * 0.1 ) = 200us

  It is sufficient, but the brightness should be adjustable to more than 10 values. Hence a
  resolution of 16us is selected by setting the prescaler to 64 and the compare register to 3:

    interrupt delay = (64*(3+1))/16MHz = 16us

  The timerCounter variable is of type unsigned int having a maximum value of 65535. Incrementing
  this at each interrupt and taking action upon timerCounterOnEnd or timerCounterOffEnd yields
  a maximum delay for something to happen:

    max delay for something to happen = 16us * (65535+1) = 1.04s

  which should be more than sufficient if you want to be able to look at your display.
*/

  _timerID = timerID;

}

void SevenSeg::clearTimer(){

  stopTimer();
  _timerID = -1;

}

void SevenSeg::startTimer(){

  cli();  // Temporarily stop interrupts

  // See registers in ATmega328 datasheet

  if(_timerID==0){
    TCCR0A = 0;
    TCCR0B = 0;
    TCNT0 = 0;					// Initialize counter value to 0
    OCR0A = 3;					// Set Compare Match Register to 3
    TCCR0A |= (1<<WGM01);			// Turn on CTC mode
    TCCR0B |= (1<<CS01) | (1<<CS00);		// Set prescaler to 64
    TIMSK0 |= (1<<OCIE0A);			// Enable timer compare interrupt
  }

  if(_timerID==1){
    TCCR1A = 0;
    TCCR1B = 0;
    TCNT1  = 0;					// Initialize counter value to 0
    OCR1A = 3;					// Set compare Match Register to 3
    TCCR1B |= (1 << WGM12);			// Turn on CTC mode
    TCCR1B |= (1 << CS11) | (1 << CS10);	// Set prescaler to 64
    TIMSK1 |= (1 << OCIE1A);			// Enable timer compare interrupt
  }

  if(_timerID==2){
    TCCR2A = 0;
    TCCR2B = 0;
    TCNT2  = 0;					// Initialize counter value to 0
    OCR2A = 3;					// Set Compare Match Register to 3
    TCCR2A |= (1 << WGM21);			// Turn on CTC mode
    TCCR2B |= (1 << CS22);			// Set prescaler to 64
    TIMSK2 |= (1 << OCIE2A);			// Enable timer compare interrupt
  }

  sei();  // Continue allowing interrupts

  // update delays to get reasonable values to _timerCounterOn/OffEnd.
  updDelay();
  _timerCounter=0;

}

void SevenSeg::stopTimer(){
  if(_timerID==0){
    TCCR0B = 0;
  }
  if(_timerID==1){
    TCCR1B = 0;
  }
  if(_timerID==2){
    TCCR2B = 0;
  }
}

#else

void SevenSeg::setTimer(int timerID){}
void SevenSeg::clearTimer(){}
void SevenSeg::startTimer(){}
void SevenSeg::stopTimer(){}

#endif
