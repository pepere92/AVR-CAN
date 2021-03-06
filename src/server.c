#define F_CPU (1000000L)
#include <avr/io.h>
#include <util/delay.h>
#include <inttypes.h>
#include <avr/interrupt.h>
#include <stdlib.h>

#define maxDataLength 8

/* TEST CAN CODE - SERVER
        Listens for changes on INT0, which is connected to a momentary button.
        When changes occur, an interrupt fires and it sends an appropriate message across the CAN bus.
        If the pin is high, it sends 0xFF. If the pin is low, it sends 0x00.
*/

uint8_t receivedData[maxDataLength];
uint8_t data[maxDataLength];

// Set up and enable the CAN bus by initializing the relevant registers
int initCan () {

    //Software reset
    CANGCON = _BV(SWRES);

    //CAN timing prescaler set to 0;
    CANTCON = 0x00;

    // Set baud rate to 1000kb (assuming 8Mhz IOclk)
    CANBT1 = 0x1E;
    CANBT2 = 0x04;
    CANBT3 = 0x13;

    int i = 0;
    for(i=0;i<maxDataLength;i++) {
        receivedData[i] = 0x00;
    }

    // enable interrupts: all, receive
    CANGIE = (_BV(ENIT) | _BV(ENRX));
    // compatibility with future chips
    CANIE1 = 0;
    // enable interrupts on all MObs
    CANIE2 = (_BV(IEMOB0) | _BV(IEMOB1) | _BV(IEMOB2));

    int8_t mob;
    for (mob=0; mob<6; mob++ ) {
        // Selects Message Object 0-5
        CANPAGE = ( mob << 4 );
        // Disable mob
        CANCDMOB = 0x00;
        // Clear mob status register;
        CANSTMOB = 0x00;
    }

    // set up MOb2 for reception
    CANPAGE = _BV(MOBNB1);


    // set compatibility registers to 0, RTR/IDE-mask to 1
    CANIDM4 = (_BV(RTRMSK) | _BV(IDEMSK));
    CANIDM3 = 0x00;
    /*
    // only accept one specific message ID
    CANIDM2 = (_BV(IDMSK2) | _BV(IDMSK1) | _BV(IDMSK0));
    CANIDM1 = 0x01;*/

    // accept all IDs
    CANIDM2 = 0x00;
    CANIDM1 = 0x00;

    // enable reception, DLC8
    CANCDMOB = _BV(CONMOB1) | (8 << DLC0);

    // Enable mode: CAN channel enters in enable mode after 11 recessive bits
    CANGCON |= ( 1 << ENASTB );

// TODO: set ID tags and masks for msgs this MOb can accept

    return(0);
}

// Sends a message across the CAN bus.
// Accepts an int msg. If msg is 0, it sends 0x00. Otherwise, it sends 0xFF.
int sendCANMsg(int msg) {
    if (msg) {
        data[0] = 0xFF; // 0b11111111
    } else {
        data[0] = 0x00; // 0b00000000
    }
    uint8_t dataLength = 1; // only sending one test byte
    // set MOb number (0 for testing) and auto-increment bits in CAN page MOb register
    CANPAGE = ( _BV(AINC));

    //Wait for MOb1 to be free
    while(CANEN2 & (1 << ENMOB0));

    CANEN2 |= (1 << ENMOB0); //Claim MOb1


    //Clear MOb status register
    CANSTMOB = 0x00;

    CANMSG = data[0]; // set data page register

    // set compatibility registers, RTR bit, and reserved bit to 0
    CANIDT4 = 0;
    CANIDT3 = 0;

    // set ID tag registers
    CANIDT2 = 0b10100000; // last 5 bits must be 0
    CANIDT1 = 0b11001100;

    CANCDMOB = (_BV(CONMOB0) | dataLength); // set transmit bit and data length bits of MOb control register

    //TODO: use interrupts for this instead of while loop
    //wait for TXOK
    while((CANSTMOB & (1 << TXOK)) != (1 << TXOK));// & timeout--);

    //Disable Transmission
    CANCDMOB = 0x00;
    //Clear TXOK flag (and all others)
    CANSTMOB = 0x00;

    return(0);
}


// Set up external interrupts for INT0 for any logical change
int initButton() {
    EICRA = _BV(ISC00);
    EIMSK = _BV(INT0);
    return(0);
}

// Interrupt routine for External Interrupt 0 (fires when pin 14 changes)
// Reads the value of pin 14 and sends an appropriate CAN message.
ISR(INT0_vect) {
    char cSREG = SREG; //store SREG
    int val = PIND & _BV(PD6);
    if (val) {
        sendCANMsg(1);
    } else {
        sendCANMsg(0);
    }
    SREG=cSREG; //restore SREG
}

int main (void) {
    DDRB |= 0xFF; // set all PORTB pins for output
    DDRD &= ~(_BV(PD6)); // set pin 14 for input

    sei(); // enable global interrupts    
    initCan(); // initialize CAN bus
    initButton(); // intitialize button interrupts

    for (;;) {
        // listen for button presses forever
    }
}

