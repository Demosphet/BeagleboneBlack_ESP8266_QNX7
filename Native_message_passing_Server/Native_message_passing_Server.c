#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/mman.h>      // for mmap_device_io();
#include <sys/neutrino.h>  // for ThreadCtl( _NTO_TCTL_IO_PRIV , NULL)
#include <hw/inout.h>      // for in32() and out32();
#include <stdint.h>        // for unit32 types
#include <fcntl.h>
#include <share.h>
#include <pthread.h>

// Define what the channel is called. It will be located at <hostname>/dev/name/local/myname"
// Change myname to something unique for you (The client should be set to same name)
#define ATTACH_POINT "test_native_message_passing"

#define BUF_SIZE 7000

char key_press_data[7000];

typedef struct {
    struct _pulse hdr; // Our real data comes after this header
    int ClientID; // our data (unique id from client)
    // int data;     // our data
    char data[BUF_SIZE];
}   my_data;

typedef struct {
    struct _pulse hdr;  // Our real data comes after this header
    char buf[BUF_SIZE]; // Message we send back to clients to tell them the messages was processed correctly.
}   my_reply;

#define AM335X_CONTROL_MODULE_BASE   (uint64_t) 0x44E10000
#define AM335X_CONTROL_MODULE_SIZE   (size_t)   0x00001448
#define AM335X_GPIO_SIZE             (uint64_t) 0x00001000
#define AM335X_GPIO1_BASE            (size_t)   0x4804C000

#define LED0          (1<<21)   // GPIO1_21
#define LED1          (1<<22)   // GPIO1_22
#define LED2          (1<<23)   // GPIO1_23
#define LED3          (1<<24)   // GPIO1_24

#define SD0 (1<<28)  // SD0 is connected to GPIO1_28
#define SCL (1<<16)  // SCL is connected to GPIO1_16


#define GPIO_OE        0x134
#define GPIO_DATAIN    0x138
#define GPIO_DATAOUT   0x13C

#define GPIO_IRQSTATUS_SET_1 0x38   // enable interrupt generation
#define GPIO_IRQWAKEN_1      0x48   // Wakeup Enable for Interrupt Line
#define GPIO_FALLINGDETECT   0x14C  // set falling edge trigger
#define GPIO_CLEARDATAOUT    0x190  // clear data out Register
#define GPIO_IRQSTATUS_1     0x30   // clear any prior IRQs

#define GPIO1_IRQ 99  // TRG page 465 list the IRQs for the am335x


#define P9_12_pinConfig 0x878 //  conf_gpmc_ben1 (TRM pp 1364) for GPIO1_28,  P9_12

// GPMC_A1_Configuration
#define PIN_MODE_0   0x00
#define PIN_MODE_1   0x01
#define PIN_MODE_2   0x02
#define PIN_MODE_3   0x03
#define PIN_MODE_4   0x04
#define PIN_MODE_5   0x05
#define PIN_MODE_6   0x06
#define PIN_MODE_7   0x07

// PIN MUX Configuration strut values  (page 1420 from TRM)
#define PU_ENABLE    0x00
#define PU_DISABLE   0x01
#define PU_PULL_UP   0x01
#define PU_PULL_DOWN 0x00
#define RECV_ENABLE  0x01
#define RECV_DISABLE 0x00
#define SLEW_FAST    0x00
#define SLEW_SLOW    0x01

typedef union _CONF_MODULE_PIN_STRUCT   // See TRM Page 1420
{
  unsigned int d32;
  struct {    // name: field size
           unsigned int conf_mmode : 3;       // LSB
           unsigned int conf_puden : 1;
           unsigned int conf_putypesel : 1;
           unsigned int conf_rxactive : 1;
           unsigned int conf_slewctrl : 1;
           unsigned int conf_res_1 : 13;      // reserved
           unsigned int conf_res_2 : 12;      // reserved MSB
         } b;
} _CONF_MODULE_PIN;

void strobe_SCL(uintptr_t gpio_port_add) {
   uint32_t PortData;
   PortData = in32(gpio_port_add + GPIO_DATAOUT);// value that is currently on the GPIO port
   PortData &= ~(SCL);
   out32(gpio_port_add + GPIO_DATAOUT, PortData);// Clock low
   delaySCL();

   PortData  = in32(gpio_port_add + GPIO_DATAOUT);// get port value
   PortData |= SCL;// Clock high
   out32(gpio_port_add + GPIO_DATAOUT, PortData);
   delaySCL();
}


// Thread used to Flash the 4 LEDs on the BeagleBone for 100ms
void *Flash_LED0_ex(void *notused)
{
    pthread_detach(pthread_self());  // no need for this thread to join
    uintptr_t gpio1_port = mmap_device_io(AM335X_GPIO_SIZE, AM335X_GPIO1_BASE);

    uintptr_t val;
    // Write GPIO data output register
    val  = in32(gpio1_port + GPIO_DATAOUT);
    val |= (LED0|LED1|LED2|LED3);
    out32(gpio1_port + GPIO_DATAOUT, val);

    usleep(100000);  // 100 ms wait
    //sched_yield();  // if used without the usleep, this line will flash the LEDS for ~4ms

    val  = in32(gpio1_port + GPIO_DATAOUT);
    val &= ~(LED0|LED1|LED2|LED3);
    out32(gpio1_port + GPIO_DATAOUT, val);

    munmap_device_io(gpio1_port, AM335X_GPIO_SIZE);

}


void delaySCL()  {// Small delay used to get timing correct for BBB
  volatile int i, a;
  for(i=0;i<0x1F;i++) // 0x1F results in a delay that sets F_SCL to ~480 kHz
  {   // i*1 is faster than i+1 (i+1 results in F_SCL ~454 kHz, whereas i*1 is the same as a=i)
     a = i;
  }
  // usleep(1);  //why doesn't this work? Ans: Results in a period of 4ms as
  // fastest time, which is 250Hz (This is to slow for the TTP229 chip as it
  // requires F_SCL to be between 1 kHz and 512 kHz)
}

uint32_t KeypadReadIObit(uintptr_t gpio_base, uint32_t BitsToRead)  {
   volatile uint32_t val = 0;
   val  = in32(gpio_base + GPIO_DATAIN);// value that is currently on the GPIO port

   val &= BitsToRead; // mask bit
   //val = val >> (BitsToRead % 2);
   //return val;
   if(val==BitsToRead)
       return 1;
   else
       return 0;
}

void DecodeKeyValue(uint32_t word)
{
    switch(word)
    {
        case 0x01:
            printf("Key  1 pressed\n");
            pthread_create(NULL, NULL, Flash_LED0_ex, NULL); // flash LED
            strcat(key_press_data,"Key  1:");
            break;
        case 0x02:
            printf("Key  2 pressed\n");
            pthread_create(NULL, NULL, Flash_LED0_ex, NULL); // flash LED
            strcat(key_press_data,"Key  2:");
            break;
        case 0x04:
            printf("Key  3 pressed\n");
            pthread_create(NULL, NULL, Flash_LED0_ex, NULL); // flash LED
            strcat(key_press_data,"Key  3:");
            break;
        case 0x08:
            printf("Key  4 pressed\n");
            pthread_create(NULL, NULL, Flash_LED0_ex, NULL); // flash LED
            strcat(key_press_data,"Key  4:");
            break;
        case 0x10:
            printf("Key  5 pressed\n");
            pthread_create(NULL, NULL, Flash_LED0_ex, NULL); // flash LED
            strcat(key_press_data,"Key  5:");
            break;
        case 0x20:
            printf("Key  6 pressed\n");
            pthread_create(NULL, NULL, Flash_LED0_ex, NULL); // flash LED
            strcat(key_press_data,"Key  6:");
            break;
        case 0x40:
            printf("Key  7 pressed\n");
            pthread_create(NULL, NULL, Flash_LED0_ex, NULL); // flash LED
            strcat(key_press_data,"Key  7:");
            break;
        case 0x80:
            printf("Key  8 pressed\n");
            pthread_create(NULL, NULL, Flash_LED0_ex, NULL); // flash LED
            strcat(key_press_data,"Key  8:");
            break;
        case 0x100:
            printf("Key  9 pressed\n");
            pthread_create(NULL, NULL, Flash_LED0_ex, NULL); // flash LED
            strcat(key_press_data,"Key  9:");
            break;
        case 0x200:
            printf("Key 10 pressed\n");
            pthread_create(NULL, NULL, Flash_LED0_ex, NULL); // flash LED
            strcat(key_press_data,"Key  10:");
            break;
        case 0x400:
            printf("Key 11 pressed\n");
            pthread_create(NULL, NULL, Flash_LED0_ex, NULL); // flash LED
            strcat(key_press_data,"Key  11:");
            break;
        case 0x800:
            printf("Key 12 pressed\n");
            pthread_create(NULL, NULL, Flash_LED0_ex, NULL); // flash LED
            strcat(key_press_data,"Key  12:");
            break;
        case 0x1000:
            printf("Key 13 pressed\n");
            pthread_create(NULL, NULL, Flash_LED0_ex, NULL); // flash LED
            strcat(key_press_data,"Key  13:");
            break;
        case 0x2000:
            printf("Key 14 pressed\n");
            pthread_create(NULL, NULL, Flash_LED0_ex, NULL); // flash LED
            strcat(key_press_data,"Key  14:");
            break;
        case 0x4000:
            printf("Key 15 pressed\n");
            pthread_create(NULL, NULL, Flash_LED0_ex, NULL); // flash LED
            strcat(key_press_data,"Key  15:");
            break;
        case 0x8000:
            printf("Key 16 pressed\n");
            pthread_create(NULL, NULL, Flash_LED0_ex, NULL); // flash LED
            strcat(key_press_data,"Key  16:");
            break;
        case 0x00:  // key release event (do nothing)
            break;
        default:
            printf("Key pressed could not be determined - %lu\n", word);
            strcat(key_press_data,"Unknown:");
    }
}

// // prototypes
// // int server();
void *server();

int main(int argc, char *argv[]) {
    printf("Server running\n");

    pthread_create(NULL, NULL, server, NULL);

    // //--------Keypad Pins--------
    // XC4602 pin     -> BeagleBone Black Pin
    // VCC - VDD_3V3B -> pin P9_03 or P9_04
    // GND - DGND     -> pin P9_01 or P9_02
    // SCL - GPIO1_16 -> pin P9_15
    // SD0 - GPIO1_28 -> pin P9_12
    uintptr_t control_module = mmap_device_io(AM335X_CONTROL_MODULE_SIZE,
                                                               AM335X_CONTROL_MODULE_BASE);
    uintptr_t gpio1_base     = mmap_device_io(AM335X_GPIO_SIZE          , AM335X_GPIO1_BASE);


    if ((control_module)&&(gpio1_base)) {
    ThreadCtl( _NTO_TCTL_IO_PRIV , NULL);// Request I/O privileges;

    volatile uint32_t val = 0;

    // set DDR for LEDs to output and GPIO_28 to input
    val = in32(gpio1_base + GPIO_OE); // read in current setup for GPIO1 port
    val |= 1<<28;                     // set IO_BIT_28 high (1=input, 0=output)
    out32(gpio1_base + GPIO_OE, val); // write value to input enable for data pins
    val &= ~(LED0|LED1|LED2|LED3);    // write value to output enable
    out32(gpio1_base + GPIO_OE, val); // write value to output enable for LED pins

    val = in32(gpio1_base + GPIO_OE);
    val &= ~SCL;                      // 0 for output
    out32(gpio1_base + GPIO_OE, val); // write value to output enable for data pins


    val = in32(gpio1_base + GPIO_DATAOUT);
    val |= SCL;              // Set Clock Line High as per TTP229-BSF datasheet
    out32(gpio1_base + GPIO_DATAOUT, val); // for 16-Key active-Low timing diagram


    in32s(&val, 1, control_module + P9_12_pinConfig );
    printf("Original pinmux configuration for GPIO1_28 = %#010x\n", val);

    // set up pin mux for the pins we are going to use  (see page 1354 of TRM)
    volatile _CONF_MODULE_PIN pinConfigGPMC; // Pin configuration strut
    pinConfigGPMC.d32 = 0;
    // Pin MUX register default setup for input (GPIO input, disable pull up/down - Mode 7)
    pinConfigGPMC.b.conf_slewctrl = SLEW_SLOW;    // Select between faster or slower slew rate
    pinConfigGPMC.b.conf_rxactive = RECV_ENABLE;  // Input enable value for the PAD
    pinConfigGPMC.b.conf_putypesel= PU_PULL_UP;   // Pad pullup/pulldown type selection
    pinConfigGPMC.b.conf_puden = PU_ENABLE;       // Pad pullup/pulldown enable
    pinConfigGPMC.b.conf_mmode = PIN_MODE_7;      // Pad functional signal mux select 0 - 7

    // Write to PinMux registers for the GPIO1_28
    out32(control_module + P9_12_pinConfig, pinConfigGPMC.d32);
    in32s(&val, 1, control_module + P9_12_pinConfig);   // Read it back
    printf("New configuration register for GPIO1_28 = %#010x\n", val);

    // Setup IRQ for SD0 pin ( see TRM page 4871 for register list)
    out32(gpio1_base + GPIO_IRQSTATUS_SET_1, SD0);// Write 1 to GPIO_IRQSTATUS_SET_1
    out32(gpio1_base + GPIO_IRQWAKEN_1, SD0);    // Write 1 to GPIO_IRQWAKEN_1
    out32(gpio1_base + GPIO_FALLINGDETECT, SD0);    // set falling edge
    out32(gpio1_base + GPIO_CLEARDATAOUT, SD0);     // clear GPIO_CLEARDATAOUT
    out32(gpio1_base + GPIO_IRQSTATUS_1, SD0);      // clear any prior IRQs

    struct sigevent event; // fill in "event" structure
    memset(&event, 0, sizeof(event));
    event.sigev_notify = SIGEV_INTR;  // Setup for external interrupt

    int id = 0; // Attach interrupt Event to IRQ for GPIO1B  (upper 16 bits of port)
    id = InterruptAttachEvent (GPIO1_IRQ, &event, _NTO_INTR_FLAGS_TRK_MSK);

    // Main code starts here
    printf( "Waiting For Interrupt 99 - key press on Jaycar (XC4602) keypad\n");
    int i = 0;
    for(;;)   // for loop that correctly decodes key press
    {
        InterruptWait( 0, NULL );   // block this thread until an interrupt occurs
        InterruptDisable();

        volatile uint32_t word = 0;
        //  confirm that SD0 is still low (that is a valid Key press event has occurred)
        val = KeypadReadIObit(gpio1_base, SD0);  // read SD0 (means data is ready)

        if(val == 0)  // start reading key value form the keypad
        {
             word = 0;  // clear word variable

             delaySCL(); // wait a short period of time before reading the data Tw  (10 us)

             for(i=0;i<16;i++)           // get data from SD0 (16 bits)
             {
                strobe_SCL(gpio1_base);  // strobe the SCL line so we can read in data bit
                val = KeypadReadIObit(gpio1_base, SD0); // read in data bit
                val = ~val & 0x01;                      // invert bit and mask out everything but the LSB
                //printf("val[%u]=%u, ",i, val);
                word = word | (val<<i);  // add data bit to word in unique position (build word up bit by bit)
             }
             //printf("word=%u\n",word);
             DecodeKeyValue(word);
        }
        out32(gpio1_base + GPIO_IRQSTATUS_1, SD0); //clear IRQ
        InterruptUnmask(GPIO1_IRQ, id);
        InterruptEnable();
    }



     munmap_device_io(control_module, AM335X_CONTROL_MODULE_SIZE);
     printf("Here? 5\n");
    }

     // int ret = 0;
     // ret     = server();

    printf("Main (Server) Terminated....\n");
    // return ret;
    return EXIT_SUCCESS;
}


/*** Server code ***/
// int server() {
//     name_attach_t *attach;

//     // Create a local name (/dev/name/...)
//     // Add a while loop here to prevent the server from shutting down if no client was found
//     if ((attach = name_attach(NULL, ATTACH_POINT, 0)) == NULL) {
//         printf("\nFailed to name_attach on ATTACH_POINT: %s \n", ATTACH_POINT);
//         printf("\n Possibly another server with the same name is already running !\n");
//         return EXIT_FAILURE;
//     }

//     printf("Server Listening for Clients on ATTACH_POINT: %s \n\n", ATTACH_POINT);

//     /*
//     *  Server Loop
//     */
//     my_data msg;
//     int rcvid       = 0, msgnum = 0;    // no message received yet
//     int stay_alive  = 1, living = 0;    // server stays running (ignores _PULSE_CODE_DISCONNECT request)

//     my_reply replymsg; // replymsg structure for sending back to client
//     replymsg.hdr.type    = 0x01;
//     replymsg.hdr.subtype = 0x00;

//     living = 1;
//     while (living) {
//         // Do your MsgReceive's here now with the chid
//         rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);

//         if (rcvid == -1) { // Error condition, exit
//             printf("\n--->Failed to MsgReceive\n");
//             // break;
//         } else if (rcvid == 0) {  // Pulse received, work out what type
//             // for Pulses:
//             switch (msg.hdr.code) {
//                 // A client disconnected all its connections by running
//                 // name_close() for each name_open()  or terminated
//                 case _PULSE_CODE_DISCONNECT:
//                     if( stay_alive == 0) {
//                         ConnectDetach(msg.hdr.scoid);
//                         printf("\nServer was told to Detach from ClientID:%d ...\n", msg.ClientID);
//                         living = 0; // kill while loop
//                         continue;
//                     } else {
//                         printf("\nServer received Detach pulse from ClientID:%d but rejected it ...\n", msg.ClientID);
//                     }
//                     break;
//                 // REPLY blocked client wants to unblock (was hit by a signal
//                 // or timed out).  It's up to you if you reply now or later.
//                 case _PULSE_CODE_UNBLOCK:
//                     printf("\nServer got _PULSE_CODE_UNBLOCK after %d, msgnum\n", msgnum);
//                     break;

//                 case _PULSE_CODE_COIDDEATH:  // from the kernel
//                     printf("\nServer got _PULSE_CODE_COIDDEATH after %d, msgnum\n", msgnum);
//                     break;

//                 case _PULSE_CODE_THREADDEATH: // from the kernel
//                    printf("\nServer got _PULSE_CODE_THREADDEATH after %d, msgnum\n", msgnum);
//                    break;

//                default:
//                    // Some other pulse sent by one of your processes or the kernel
//                    printf("\nServer got some other pulse after %d, msgnum\n", msgnum);
//                    break;

//             }
//             continue;// go back to top of while loop
//         } else if(rcvid > 0) {  // if true then A message was received
//             msgnum++;

//             // If the Global Name Service (gns) is running, name_open() sends a connect message. The server must EOK it.
//             if (msg.hdr.type == _IO_CONNECT ) {
//                 MsgReply( rcvid, EOK, NULL, 0 );
//                 printf("\nGNS service is running....");
//                 continue;    // go back to top of while loop
//             }

//             // Some other I/O message was received; reject it
//             if (msg.hdr.type > _IO_BASE && msg.hdr.type <= _IO_MAX ) {
//                 MsgError( rcvid, ENOSYS );
//                 printf("\nServer received and IO message and rejected it....");
//                 continue;    // go back to top of while loop
//             }

//             // A message (presumably ours) received

//             // put your message handling code here and assemble a reply message
//             if (!strcmp(msg.data,"First response")) {
//                 sprintf(replymsg.buf, "Hello");
//                 printf("Server received data packet with value of '%s' from client (ID:%d), ", msg.data, msg.ClientID);
//                 fflush(stdout);
//                 delay(250); // Delay the reply by a second (just for demonstration purposes)
//                 printf("\nReplying with: '%s'\n\n",replymsg.buf);
//                 MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));

//             } else if (!strcmp(msg.data,"Second response")) {
//                 sprintf(replymsg.buf, "Hello\n");
//                 printf("Server received data packet with value of '%s' from client (ID:%d), ", msg.data, msg.ClientID);
//                 fflush(stdout);
//                 delay(250); // Delay the reply by a second (just for demonstration purposes)
//                 printf("\nReplying with: '%s'\n\n",replymsg.buf);
//                 MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));

//             } else if (!strcmp(msg.data,"Third response")) {
//                 sprintf(replymsg.buf, "Hello\r");
//                 printf("Server received data packet with value of '%s' from client (ID:%d), ", msg.data, msg.ClientID);
//                 fflush(stdout);
//                 delay(250); // Delay the reply by a second (just for demonstration purposes)
//                 printf("\nReplying with: '%s'\n\n",replymsg.buf);
//                 MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));
                                
//             }  else if (!strcmp(msg.data,"Fourth response")) {
//                 sprintf(replymsg.buf, "Hello\r\n");
//                 printf("Server received data packet with value of '%s' from client (ID:%d), ", msg.data, msg.ClientID);
//                 fflush(stdout);
//                 delay(250); // Delay the reply by a second (just for demonstration purposes)
//                 printf("\nReplying with: '%s'\n\n",replymsg.buf);
//                 MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));
                                
//             }  else if (!strcmp(msg.data,"Fifth response")) {
//                 sprintf(replymsg.buf, "Test Message");
//                 printf("Server received data packet with value of '%s' from client (ID:%d), ", msg.data, msg.ClientID);
//                 fflush(stdout);
//                 delay(250); // Delay the reply by a second (just for demonstration purposes)
//                 printf("\nReplying with: '%s'\n\n",replymsg.buf);
//                 MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));
                                
//             } else if (!strcmp(msg.data,"END")) {
//                 // sprintf(replymsg.buf, "Message %d ... Oh no... Good bye", msgnum);
//                 sprintf(replymsg.buf, "... Oh no... Good bye");
//                 printf("'%s' from client (ID:%d) : Client is terminating communcations ", msg.data, msg.ClientID);
//                 fflush(stdout);
//                 delay(250); // Delay the reply by a second (just for demonstration purposes)
//                 printf("\nReplying with: '%s'\n\n",replymsg.buf);
//                 MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));
//                 stay_alive = 0;

//             } else {
//                 sprintf(replymsg.buf, "Message %d received", msgnum);
//                 printf("Server received data packet with value of '%s' from client (ID:%d), ", msg.data, msg.ClientID);
//                 fflush(stdout);
//                 delay(250); // Delay the reply by a second (just for demonstration purposes)
//                 printf("\nReplying with: '%s'\n\n",replymsg.buf);
//                 MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));
//             }
//             // sprintf(replymsg.buf, "Message %d received", msgnum);
//             // printf("Server received data packet with value of '%s' from client (ID:%d), ", msg.data, msg.ClientID);
//             // fflush(stdout);
//             // sleep(1); // Delay the reply by a second (just for demonstration purposes)

//             // printf("\nReplying with: '%s'\n",replymsg.buf);
//             // MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));

//         } else {
//             printf("\n--->ERROR: Server received something, but could not handle it correctly\n");
//         }
//     }

//     // Remove the attach point name from the file system (i.e. /dev/name/local/<myname>)
//     name_detach(attach, 0);

//     return EXIT_SUCCESS;
// }

void *server() {
    name_attach_t *attach;

    // Create a local name (/dev/name/...)
    // Add a while loop here to prevent the server from shutting down if no client was found
    if ((attach = name_attach(NULL, ATTACH_POINT, 0)) == NULL) {
        printf("\nFailed to name_attach on ATTACH_POINT: %s \n", ATTACH_POINT);
        printf("\n Possibly another server with the same name is already running !\n");
        // return EXIT_FAILURE;
    }

    printf("Server Listening for Clients on ATTACH_POINT: %s \n\n", ATTACH_POINT);

    /*
    *  Server Loop
    */
    my_data msg;
    int rcvid       = 0, msgnum = 0;    // no message received yet
    int stay_alive  = 1, living = 0;    // server stays running (ignores _PULSE_CODE_DISCONNECT request)

    my_reply replymsg; // replymsg structure for sending back to client
    replymsg.hdr.type    = 0x01;
    replymsg.hdr.subtype = 0x00;

    living = 1;
    while (living) {
        // Do your MsgReceive's here now with the chid
        rcvid = MsgReceive(attach->chid, &msg, sizeof(msg), NULL);

        if (rcvid == -1) { // Error condition, exit
            printf("\n--->Failed to MsgReceive\n");
            // break;
        } else if (rcvid == 0) {  // Pulse received, work out what type
            // for Pulses:
            switch (msg.hdr.code) {
                // A client disconnected all its connections by running
                // name_close() for each name_open()  or terminated
                case _PULSE_CODE_DISCONNECT:
                    if( stay_alive == 0) {
                        ConnectDetach(msg.hdr.scoid);
                        printf("\nServer was told to Detach from ClientID:%d ...\n", msg.ClientID);
                        living = 0; // kill while loop
                        continue;
                    } else {
                        printf("\nServer received Detach pulse from ClientID:%d but rejected it ...\n", msg.ClientID);
                    }
                    break;
                // REPLY blocked client wants to unblock (was hit by a signal
                // or timed out).  It's up to you if you reply now or later.
                case _PULSE_CODE_UNBLOCK:
                    printf("\nServer got _PULSE_CODE_UNBLOCK after %d, msgnum\n", msgnum);
                    break;

                case _PULSE_CODE_COIDDEATH:  // from the kernel
                    printf("\nServer got _PULSE_CODE_COIDDEATH after %d, msgnum\n", msgnum);
                    break;

                case _PULSE_CODE_THREADDEATH: // from the kernel
                   printf("\nServer got _PULSE_CODE_THREADDEATH after %d, msgnum\n", msgnum);
                   break;

               default:
                   // Some other pulse sent by one of your processes or the kernel
                   printf("\nServer got some other pulse after %d, msgnum\n", msgnum);
                   break;

            }
            continue;// go back to top of while loop
        } else if(rcvid > 0) {  // if true then A message was received
            msgnum++;

            // If the Global Name Service (gns) is running, name_open() sends a connect message. The server must EOK it.
            if (msg.hdr.type == _IO_CONNECT ) {
                MsgReply( rcvid, EOK, NULL, 0 );
                printf("\nGNS service is running....");
                continue;    // go back to top of while loop
            }

            // Some other I/O message was received; reject it
            if (msg.hdr.type > _IO_BASE && msg.hdr.type <= _IO_MAX ) {
                MsgError( rcvid, ENOSYS );
                printf("\nServer received and IO message and rejected it....");
                continue;    // go back to top of while loop
            }

            // A message (presumably ours) received

            // put your message handling code here and assemble a reply message
            if (!strcmp(msg.data,"First response")) {
                sprintf(replymsg.buf, "Hello");
                printf("Server received data packet with value of '%s' from client (ID:%d), ", msg.data, msg.ClientID);
                fflush(stdout);
                delay(250); // Delay the reply by a second (just for demonstration purposes)
                printf("\nReplying with: '%s'\n\n",replymsg.buf);
                MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));

            } else if (!strcmp(msg.data,"Second response")) {
                sprintf(replymsg.buf, "Hello\n");
                printf("Server received data packet with value of '%s' from client (ID:%d), ", msg.data, msg.ClientID);
                fflush(stdout);
                delay(250); // Delay the reply by a second (just for demonstration purposes)
                printf("\nReplying with: '%s'\n\n",replymsg.buf);
                MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));

            } else if (!strcmp(msg.data,"Third response")) {
                sprintf(replymsg.buf, "Hello\r");
                printf("Server received data packet with value of '%s' from client (ID:%d), ", msg.data, msg.ClientID);
                fflush(stdout);
                delay(250); // Delay the reply by a second (just for demonstration purposes)
                printf("\nReplying with: '%s'\n\n",replymsg.buf);
                MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));
                                
            }  else if (!strcmp(msg.data,"Fourth response")) {
                sprintf(replymsg.buf, "Hello\r\n");
                printf("Server received data packet with value of '%s' from client (ID:%d), ", msg.data, msg.ClientID);
                fflush(stdout);
                delay(250); // Delay the reply by a second (just for demonstration purposes)
                printf("\nReplying with: '%s'\n\n",replymsg.buf);
                MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));
                                
            }  else if (!strcmp(msg.data,"Fifth response")) {
                sprintf(replymsg.buf, "Test Message");
                printf("Server received data packet with value of '%s' from client (ID:%d), ", msg.data, msg.ClientID);
                fflush(stdout);
                delay(250); // Delay the reply by a second (just for demonstration purposes)
                printf("\nReplying with: '%s'\n\n",replymsg.buf);
                MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));
                                
            } else if (!strcmp(msg.data,"Data?")) {
                // sprintf(replymsg.buf, "Message %d ... Oh no... Good bye", msgnum);
                strcpy(replymsg.buf, key_press_data);
                printf("'%s' from client (ID:%d) : Client is terminating communcations ", msg.data, msg.ClientID);
                fflush(stdout);
                delay(250); // Delay the reply by a second (just for demonstration purposes)
                printf("\nReplying with: '%s'\n\n",replymsg.buf);
                MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));
                stay_alive = 0;

            } else if (!strcmp(msg.data,"END")) {
                // sprintf(replymsg.buf, "Message %d ... Oh no... Good bye", msgnum);
                sprintf(replymsg.buf, "Request to Terminate has been received");
                printf("'%s' from client (ID:%d) : Client is terminating communcations ", msg.data, msg.ClientID);
                fflush(stdout);
                delay(250); // Delay the reply by a second (just for demonstration purposes)
                printf("\nReplying with: '%s'\n\n",replymsg.buf);
                MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));
                stay_alive = 0;

            } else {
                sprintf(replymsg.buf, "Message %d received", msgnum);
                printf("Server received data packet with value of '%s' from client (ID:%d), ", msg.data, msg.ClientID);
                fflush(stdout);
                delay(250); // Delay the reply by a second (just for demonstration purposes)
                printf("\nReplying with: '%s'\n\n",replymsg.buf);
                MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));
            }
            // sprintf(replymsg.buf, "Message %d received", msgnum);
            // printf("Server received data packet with value of '%s' from client (ID:%d), ", msg.data, msg.ClientID);
            // fflush(stdout);
            // sleep(1); // Delay the reply by a second (just for demonstration purposes)

            // printf("\nReplying with: '%s'\n",replymsg.buf);
            // MsgReply(rcvid, EOK, &replymsg, sizeof(replymsg));

        } else {
            printf("\n--->ERROR: Server received something, but could not handle it correctly\n");
        }
    }

    // Remove the attach point name from the file system (i.e. /dev/name/local/<myname>)
    name_detach(attach, 0);

    // return EXIT_SUCCESS;
}
