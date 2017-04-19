# Arduino SevenSeg
## Getting Started

A sample code for a four-digit common anode display is shown below:

```arduino
#include <SevenSeg.h>

SevenSeg disp(11,7,3,5,6,10,2);

const int numOfDigits=4;
int digitPins[numOfDigits]={12,9,8,13};

void setup() {
  
  disp.setDigitPins(numOfDigits, digitPins);

}  

void loop() {

    disp.write(13.28);

}
```

Key functionality includes:

- Supports arbitrary number of digits and multiple displays
- Supports displays with decimal points, colon and apostrophe
- Supports common anode, common cathode and other hardware configurations
- High level printing functions for easily displaying:
  - Numbers (integers, fixed point and floating point)
  - Text strings
  - Time (hh:mm) or (mm:ss)
- Automatic multiplexing with adjustable refresh rate
- Adjustable brightness through duty cycle control
- Use of interrupt timers for multiplexing in order to release resources, allowing the MCU to execute other code
- Leading zero suppression (e.g.\ 123 is displayed as 123 rather than 0123 when using 4 digits)
- No shadow artifact

For further information, please see the attached user guide.
