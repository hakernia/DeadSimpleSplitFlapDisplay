/*
  (C) 2017 Piotr Karpiewski
  An example driver for the Dead Simple Split Flap Display
        thingiverse.com/thing:2369832

  Features:
      - Initial acceleration of the rolling action
      - Brake steps at the end of the rolling action
      - Multiple units ready (shift register output TBD)
      - Does not freeze the loop()
      - Text rotation convenience function for single unit setup

  Usage:
      Modify loop() function for your needs. See example at the bottom of this code.
      General rules:
        Set your text in the message[] variable. Watch its size! max chars = NUM_UNITS
        Use codeset below to set the required flap.
      
          flap num:  0         1         2         3
                     01234567890123456789012345678901
          character: ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_    31 = space (hardcoded)
        
        The text will be loaded to display when the variable rolling is false.
        When rolling variable is true, use short delay not to overstep the motors.
        When rolling is false use longer delay to hold the text steady as long as necessary for reading.
        Each call to move_rotors() processes one "tick".
          The tick may move the rotors one step or return idle to exercise delay.
            The driver logic handles the necessary actions automatically.
          Always call move_rotors() at the beginning of the loop().

  Initial setup of the unit: 
      1. Manually roll to the space flap, the one preceding 'A' character.
      2. Turn on the driver.
      3. Watch the display going out of sync sooner or later ;-)


  To do:
      - Shift register output
      - Zero sensor support
*/
 
#define INITIAL_ACCEL 15  // initial acceleration (the higher the slower the start is)
#define BRAKE_STEPS   50  // for how many steps the last rotor state remains energized
#define GEAR_RATIO    64  // motor type specific; 24BYJ-48 = 64
#define NUM_FLAPS     32
#define NUM_UNITS     10  // number of display units (character positions) in a line

// Stepper motor pins
// this assumes driving the single motor; four pins of it
  int pin_1 = 5;
  int pin_2 = 6;
  int pin_3 = 7;
  int pin_4 = 8;

// Shift register pins
// this assumes driving multiple motors with a shift register (TBD)
  int pin_data = 5;
  int pin_clk = 6;



// Internal variables

// Current state of the rotor pins.
// Each index of the array keeps data of two rotors!
  char rotors[NUM_UNITS / 2] = {0x11};  
// Number of remaining steps to do, per rotor
  long int steps[NUM_UNITS] = {0};
// Acceleration countdown; actually delay (higher=slower)
// The value decreases with each flip to increase speed
  int acc = 0;
// The acc temp countdown; flips go only if 0
  int acc_t = 0;
// Last stable display contents (0='A', 1='B',... 31=space)
  char last_display[NUM_UNITS];  
// Informs if any display is still rolling
  char rolling = false;
// Internal message buffer
  char message[NUM_UNITS+1];
// Multi purpose variable
  int ff;



// The setup routine runs once when you press reset
void setup() {                
  // Initialize output pins.
  pinMode(pin_1, OUTPUT);
  pinMode(pin_2, OUTPUT);
  pinMode(pin_3, OUTPUT);
  pinMode(pin_4, OUTPUT);
  
  memset(message, 0, sizeof(message));                     // initial clear the msg buffer
  memset(rotors, 0x11, sizeof(rotors));                    // init single bit per nibble
  memset(last_display, NUM_FLAPS-1, sizeof(last_display)); // initial spaces
}


// This function is supposed to send entire buffer to shift registers
// For now it only sets four outputs for single stepper motor.
void rotors_out() {
  int rotor_num = 0;  // choose motor number (0..NUM_UNITS-1]
  char rotor;
  
  if(steps[rotor_num] > -BRAKE_STEPS) {
    rotor = rotors[rotor_num / 2];
    if(rotor_num % 2)
      rotor = rotor << 4;  // move lower bits to read positions
    digitalWrite(pin_1, (rotor & 0x80));
    digitalWrite(pin_2, (rotor & 0x40));
    digitalWrite(pin_3, (rotor & 0x20));
    digitalWrite(pin_4, (rotor & 0x10));
  }
  else
  {  // deenergize the rotor
    digitalWrite(pin_1, 0);
    digitalWrite(pin_2, 0);
    digitalWrite(pin_3, 0);
    digitalWrite(pin_4, 0);
  }
}

// Changes the rotor pins state in the memory, one step forward.
void step_fwd(int rotor_num) {
  int rotor0;
  int rotor = rotor0 = rotors[rotor_num / 2];
  if(rotor_num % 2 == 0)
    rotor = rotor >> 4;
  else
    rotor &= 0x0F;

  if(rotor & 0x08)
    rotor = 1;
  else
    rotor = rotor << 1;
      
  if(rotor_num % 2 == 0)
    rotors[rotor_num / 2] = ((rotor << 4) | (0x0F & rotor0));  // xxxx1111
  else
    rotors[rotor_num / 2] = (rotor | (0xF0 & rotor0));  // 1111xxxx
}

// Process "ticks"
// Determine if rotors need to step fwd or stay idle for this tick
void move_rotors() {
  int   rotor_num;
  char *msg_p;
  char  chr = 0;

  rolling = false;
  if(acc_t == 0)
  for (ff=0; ff < NUM_UNITS; ff++) {
    if(steps[ff] > -BRAKE_STEPS)
    {
      if(steps[ff] > 0)
          step_fwd(ff);
      steps[ff]--;
      rolling = true;
    }
  }

  // Send data to physical outputs
  rotors_out();  

  // Determine delay of the next rotor "tick" based on acceleration
  if(acc_t == 0) {
    if(acc > 0) {
      acc--;
      acc_t = acc;
    }
  }
  else {
    acc_t--;
    // It is still rolling phase, just as interstep delay
    rolling = true;
  }
  
  if(!rolling) {
    // All units finished rolling
    acc = INITIAL_ACCEL;
      
    // Load new message to steps[] based on last_display[] and required message[]
    if(message[0] == 0)
        return;
    msg_p = message; // go to begin of msg
    for(rotor_num = 0; rotor_num < NUM_UNITS; rotor_num++) {
      if(*msg_p == 0 || *msg_p == ' ')
          chr = NUM_FLAPS-1;     // For eom or space set flap space
      else
          chr = (*msg_p - 'A');  // For a letter set the corresponding flap, A=0, B=1, ...
      steps[rotor_num] = ((chr - last_display[rotor_num] + NUM_FLAPS) % NUM_FLAPS) * GEAR_RATIO;
      steps[rotor_num] = steps[rotor_num] * 319 / 320;   // This ratio may be motor specific; consider using arrays
      last_display[rotor_num] = chr;
      // Clear loaded char and move ahead
      if(*msg_p != 0) {
         *msg_p = 0;
          msg_p++;
      }
    }
  }  
}



/* Modify the code below to your needs */

char example_msg[255] = "HAKERNIA PIOTR KARPIEWSKI ";

// Convenience function, for demo purposes
char rotate_str(char *msg) {
  char first_char;
  
  first_char = *msg;
  while (*(msg+1)) { *msg++ = *(msg+1); }
  *msg = first_char;
}

void loop() {
  
  // Always start loop() with call to this function
  move_rotors();


  if(rolling) {
    // The motors are still rolling
    // Use short delay not to overstep the motors
    delay(2);
  }
  else {
    // Long delay. Keep the text steady long enough to read it
    delay(3000);
    
    // Copy contents of example_msg[] to message[] buffer
    // Use strncpy not to exceed the message buffer size!
    strncpy(message, example_msg, NUM_UNITS);
    
    // Shift left the contents of example_msg for demo purposes
    // This way the single rotor can show the entire text.
    rotate_str(example_msg);
  }
}
