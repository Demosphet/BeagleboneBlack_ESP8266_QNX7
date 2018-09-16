//--------------------------------Headers--------------------------------
#include <stdio.h>
#include <stdlib.h>

// New Headers
#include <devctl.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/neutrino.h>
#include <hw/spi-master.h>
#include <string.h>
#include <hw/inout.h>    // for in32() and out32()
#include <sys/mman.h>    // for mmap_device_io()
#include <stdint.h>      // for unit32 types


//--------------------------------Global Definitions--------------------------------
// Registers required to configure and use the pins
#define AM335X_CONTROL_MODULE_BASE (uint64_t)   0x44E10000
#define AM335X_CONTROL_MODULE_SIZE (size_t)     0x00001448
#define AM335X_GPIO_SIZE                        0x00001000
#define AM335X_GPIO1_BASE                       0x4804C000

// GPIO configuration registers
#define GPIO_OE                                 0x134   // Output Enable register
#define GPIO_DATAOUT                            0x13C   // Data Out register

// GPIO pins
#define GPIO06                                  (1<<6)  // Pin 3 - GPIO06
#define GPIO07                                  (1<<7)  // Pin 4 - GPIO07


// GPIO/SPI/UART Pin pinmux mode overview
#define GPIO_6_pinConfig                        0x858   // conf_gpmc_a6 (TRM pp 1456) for GPIO1_6
#define GPIO_7_pinConfig                        0x85C   // conf_gpmc_a7 (TRM pp 1456) for GPIO1_7
#define uart1_ctsn_pinConfig                    0x978   // conf_uart1_ctsn (TRM pp 1458) for uart1_ctsn
#define uart1_rtsn_pinConfig                    0x97C   // conf_uart1_rtsn (TRM pp 1458) for uart1_rtsn
#define uart1_rxd_pinConfig                     0x980   // conf_uart1_rxd (TRM pp 1458) for uart1_rxd
#define uart1_txd_pinConfig                     0x984   // conf_uart1_txd (TRM pp 1458) for uart1_txd
#define spi_cs_1_pinConfig                      0x99C   // conf_conf_mcasp0_ahclkr (TRM pp 1458) for spi_1_cs
#define spi_d0_1_pinConfig                      0x994   // conf_conf_mcasp0_fsx (TRM pp 1458) for spi_1_d0
#define spi_d1_1_pinConfig                      0x998   // conf_conf_mcasp0_axr0 (TRM pp 1458) for spi_1_d1
#define spi_sclk_1_pinConfig                    0x990   // conf_conf_mcasp0_aclkx (TRM pp 1458) for spi_1_sclk

// UART - Path & Message size
#define UART_PATH                               "/dev/ser2"

// (No longer used) Configuring the pinmux for UART1 - PIN MUX Configuration strut values (TRM pp 1446)
#define PU_ENABLE                               0x00
#define PU_DISABLE                              0x01
#define PU_PULL_UP                              0x01
#define PU_PULL_DOWN                            0x00
#define RECV_ENABLE                             0x01
#define RECV_DISABLE                            0x00
#define SLEW_FAST                               0x00
#define SLEW_SLOW                               0x01

// GPMC_Configurations for PIN MUX
#define PIN_MODE_0                              0x00
#define PIN_MODE_1                              0x01
#define PIN_MODE_2                              0x02
#define PIN_MODE_3                              0x03
#define PIN_MODE_4                              0x04
#define PIN_MODE_5                              0x05
#define PIN_MODE_6                              0x06
#define PIN_MODE_7                              0x07

// SPI - Path & Message size
#define SPI_PATH "/dev/spi1"
#define TSPI_WRITE_7                            (7)
#define TSPI_WRITE_SHORT                        (8)
#define TSPI_WRITE_12                           (12)
#define TSPI_WRITE_16                           (16)
#define TSPI_WRITE_32                           (32)

////--------------------------------Global Variables//--------------------------------
// UART - Message size
char char_read_buffer                           [32];
char char_write_buffer                          [32];

//Struct for configuring the Pin Multiplexer
typedef union _CONF_MODULE_PIN_STRUCT {             // See TRM Page 1446
    unsigned int d32;
    struct {                                        // Name: field size unsigned
        int conf_mmode :                        3;  // LSB
        unsigned int conf_puden :               1;
        unsigned int conf_putypesel :           1;
        unsigned int conf_rxactive :            1;
        unsigned int conf_slewctrl :            1;
        unsigned int conf_res_1 :               13; // Reserved
        unsigned int conf_res_2 :               12; // Reserved MSB
    } b;
}   _CONF_MODULE_PIN;

// File openers and return variable
int file;
int ret;

// Data packets declarations for SPI communications
uint8_t write_buffer                            [256 * 1024];
uint8_t read_buffer                             [256 * 1024];
uint8_t reg1[8]     =                           {0xff, 0x3E, 0x11, 0x00, 0x44, 0x22, 0x66, 0x00};                       // Data to be sent for testing purposes
uint8_t reg2[7]     =                           {0xfb, 0x04, 0x04, 0x3b, 0x40, 0x40, 0x3f};                             // Data to be sent for testing purposes
uint8_t reg3[7]     =                           {0x0f, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02};                             // Data to be sent for testing purposes
uint8_t reg4[8]     =                           {0x48, 0x65, 0x72, 0x65, 0x21, 0x21, 0x21, 0x00};                       // Data to be sent for testing purposes
uint8_t reg5[12]    =                           {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x53, 0x6c, 0x61, 0x76, 0x65};
uint8_t reg6[16]    =                           {0x41, 0x72, 0x65, 0x20, 0x79, 0x6f, 0x75, 0x20, 0x61, 0x6c, 0x69, 0x76, 0x65, 0x3f, 0x0};

//--------------------------------Prototypes--------------------------------
void Pin_status();
void Pin_control(unsigned int pin, unsigned int value);
void Pin_config(int mode, unsigned int puden, unsigned int putypesel, unsigned int rxactive, unsigned int slewctrl, unsigned int pin);
int spiopen();
int spisetcfg();
int spigetdevinfo();
int spiwrite(int iterations);
int spiclose();
int UART_write(char *message, int iterations);
char *UART_read();

//-------------------------------------------------------------------------
int main(void){
    puts("Hello World!!!"); /* prints Hello World!!! */

    ThreadCtl( _NTO_TCTL_IO_PRIV , NULL); // Request I/O privileges

    // //--------GPIO Check & Configuration Code--------
    // // GPIO06 : Pin 3 - Connector 8
    // // GPIO07  : Pin 4 - Connector 8
    // Pin_status();
    // Pin_control(GPIO07,0x00);
    // Pin_control(GPIO07,0xFF);
    // Pin_control(GPIO06,0x00);
    // Pin_control(GPIO06,0xFF);
    // // sleep(1);
    // // Pin_config(PIN_MODE_0,PU_ENABLE,PU_PULL_DOWN,RECV_DISABLE,SLEW_FAST,uart1_ctsn_pinConfig);
    // // Pin_config(PIN_MODE_0,PU_ENABLE,PU_PULL_DOWN,RECV_DISABLE,SLEW_FAST,uart1_rtsn_pinConfig);
    // // Pin_status();

    //--------UART Code--------
    // Tx : Pin 13 - Connector 9
    // Rx : Pin 11 - Connector 9
    ret = 0;
    file = open(UART_PATH, O_RDWR);

    UART_write("Hello",1);
    UART_read();
    // printf("Value of UART_read: %s\n", UART_read());

    if (close(file) == -1) {
        printf("close failed: %s\n", strerror(errno));
    }

    // //--------SPI Code--------
    // // CS   : Pin 28 - Connector 9
    // // MOSI : Pin 29 - Connector 9 (MISO)
    // // MISO : Pin 30 - Connector 9 (MIS1)
    // // SCLK : Pin 31 - Connector 9
    // spiopen(SPI_PATH);
    // spisetcfg();

    // // printf("How many times would you like to send the data? ");
    // // int input;
    // // scanf("%d",&input);
    // // printf("Input: %d\n\n",input);

    // // spiwrite(input);
    // spiwrite(1000);
    // spiclose();

    printf("Main Terminated...!\n");
    return EXIT_SUCCESS;
}
//--------------------------------Function Definitions--------------------------------
// Checking the status of the pins
void Pin_status(){
    printf("0. val = %#8x\n\n",AM335X_GPIO1_BASE);

    //--------Setting GPIO_1 Pins for use with GPIO07 and GPIO06--------
    // GPIO global pointers
    uintptr_t gpio1_base = NULL;
    uintptr_t control_module = NULL;

    volatile uint32_t val = 0;

    control_module = mmap_device_io(AM335X_CONTROL_MODULE_SIZE, AM335X_CONTROL_MODULE_BASE);    //Reading the state of the registers

    // Displaying the current PIN MUX configurations
    if (control_module) {
        in32s(&val,1,control_module+GPIO_6_pinConfig);
        printf("Current pinmux configuration for      GPIO6  = %#010x\n", val);

        in32s(&val,1,control_module+GPIO_7_pinConfig);
        printf("Current pinmux configuration for      GPIO7  = %#010x\n", val);

        in32s(&val,1,control_module+spi_cs_1_pinConfig);
        printf("Current pinmux configuration for   SPI 1 CS  = %#010x\n", val);

        in32s(&val,1,control_module+spi_d0_1_pinConfig);
        printf("Current pinmux configuration for   SPI 1 D0  = %#010x\n", val);

        in32s(&val,1,control_module+spi_d1_1_pinConfig);
        printf("Current pinmux configuration for   SPI 1 D1  = %#010x\n", val);

        in32s(&val,1,control_module+spi_sclk_1_pinConfig);
        printf("Current pinmux configuration for SPI 1 SCLK  = %#010x\n", val);

        in32s(&val,1,control_module+uart1_ctsn_pinConfig);
        printf("Current pinmux configuration for UART 1 CTSN = %#010x\n", val);

        in32s(&val,1,control_module+uart1_rtsn_pinConfig);
        printf("Current pinmux configuration for UART 1 RTSN = %#010x\n", val);

        in32s(&val,1,control_module+uart1_rxd_pinConfig);
        printf("Current pinmux configuration for UART 1 RXD  = %#010x\n", val);

        in32s(&val,1,control_module+uart1_txd_pinConfig);
        printf("Current pinmux configuration for UART 1 TXD  = %#010x\n\n", val);

        munmap_device_io(control_module, AM335X_CONTROL_MODULE_SIZE);
    }
}

// GPIO Pin control
void Pin_control(unsigned int pin, unsigned int value) {
    unsigned int PIN        = pin;
    unsigned int VALUE      = value;
    unsigned int new_PIN    = (PIN & VALUE);

    printf("PIN: %d\n",PIN);
    printf("VALUE: %d\n",VALUE);
    printf("New PIN VALUE: %d\n\n",new_PIN);

    uintptr_t gpio1_base    = NULL;
    volatile uint32_t val   = 0;

    gpio1_base = mmap_device_io(AM335X_GPIO_SIZE , AM335X_GPIO1_BASE);  //Reading the state of the registers

    // Write value to output enable
    if (gpio1_base) {
        val &= ~(PIN);
        out32(gpio1_base + GPIO_OE, val);

        val = in32(gpio1_base + GPIO_DATAOUT);      // Read in current value
        printf("1. val = %#8x\n", val);             // Debug
        
        if (new_PIN) {                              // Determining whether the PIN is required to set LOW or HIGH
            val |= (PIN);                           // Set the pattern to display (set next value, i++)
            printf("3. val = %#8x\n\n", val);       // Debug
        } if (!new_PIN) {
            val &= ~(PIN);                          // Clear the bits that we might change
            printf("2. val = %#8x\n", val);         // Debug            
        }
        out32(gpio1_base + GPIO_DATAOUT, val);      // Write new value
        delay(250);

        munmap_device_io(gpio1_base, AM335X_GPIO_SIZE);
    }
}

// Pin configuration through configuring the PIN MUX settings
void Pin_config(int mode, unsigned int puden, unsigned int putypesel, unsigned int rxactive, unsigned int slewctrl, unsigned int pin) {
    int MODE                = mode;
    unsigned int PUDEN      = puden;
    unsigned int PUTYPESEL  = putypesel;
    unsigned int RXACTIVE   = rxactive;
    unsigned int SLEWCTRL   = slewctrl;
    unsigned int PIN        = pin;

    // Test code to configure the PinMux for use with UART1
    uintptr_t control_module = mmap_device_io(AM335X_CONTROL_MODULE_SIZE, AM335X_CONTROL_MODULE_BASE);
    uintptr_t gpio1_base = mmap_device_io(AM335X_GPIO_SIZE , AM335X_GPIO1_BASE);

    // Set up pin mux for the pins we are going to use (see page 1354 of TRM)
    volatile _CONF_MODULE_PIN pinConfigGPMC;

    // Pin configuration strut
    pinConfigGPMC.d32 = 0;

    // Pin MUX register default setup for input (GPIO input, disable pull up/down - Mode 7)
    pinConfigGPMC.b.conf_slewctrl   = SLEWCTRL;     // Select between faster or slower slew rate
    pinConfigGPMC.b.conf_rxactive   = RXACTIVE;     // Input enable value for the PAD
    pinConfigGPMC.b.conf_putypesel  = PUTYPESEL;    // Pad pullup/pulldown type selection
    pinConfigGPMC.b.conf_puden      = PUDEN;        // Pad pullup/pulldown enable
    pinConfigGPMC.b.conf_mmode      = MODE;         // Pad functional signal mux select 0 - 7

    // Write to the PinMux registers for UART1
    out32(control_module + PIN, pinConfigGPMC.d32);
}

// Opening the communication link to SPI1
int spiopen(){
    //  Open SPI1
    if((file = spi_open(SPI_PATH) ) < 0) {  // Open SPI1
        printf("Error while opening Device File!!\n\n");
    } else {
        printf("SPI1 Opened Successfully\n\n");
    }
    return ret;

}

// Configuring the SPI1
int spisetcfg(){
    spi_cfg_t spicfg;

    // Setting the correct SPI operation mode
    // CLK POL -> 0 || CLK PHA -> 1 || For later use maybe?
    spicfg.mode         = (8 & SPI_MODE_CHAR_LEN_MASK)|SPI_MODE_CKPHASE_HALF|SPI_MODE_CKPOL_HIGH;   //SPI Mode
    spicfg.clock_rate   = 100000;                                                                   //Clock rate
     // spicfg.clock_rate  = 1000000;

    // Configuring the SPI bus
    ret = spi_setcfg(file,0,&spicfg);
    if (ret != EOK){
        fprintf(stdout,"spi_setcfg failed: %d\n\n", ret);
    } else {
        fprintf(stdout,"spi_setcfg successful: %d\n\n", ret);
    }
}

// Checking device info
int spigetdevinfo(){
    spi_devinfo_t devinfo;
    spi_cfg_t spicfg;

    // Retreiving the information on the SPI bus
    ret = spi_getdevinfo(file,SPI_DEV_ID_NONE,&devinfo);
    if (ret != EOK){
        fprintf(stdout,"spi_getdevinfo failed: %d\n", ret);
    } else {
        fprintf(stdout,"spi_getdevinfo successful: %d\n", ret);
        fprintf(stdout,"Device ID: %d\n",devinfo.device);
        fprintf(stdout,"Device Name: %s\n",devinfo.name);
        fprintf(stdout,"Device Mode: %d\n",spicfg.mode);
        fprintf(stdout,"Device Speed: %d\n\n",spicfg.clock_rate);
    }
}

// SPI write function
int spiwrite(int iterations){
    int loop_state      = 1;
    int counter         = 0;
    char output[128]    = "";
    char values[50]     = "";

    while(loop_state) {
        printf("Counter = %d\n",counter);

        // --------Write to SPI1--------
        // Writing data to the packet
        for (int i = 0; i < sizeof(reg4); i++) {
            write_buffer[i] = reg4[i];
        }
        ret = spi_write(file,SPI_DEV_LOCK,write_buffer,TSPI_WRITE_SHORT);   // Writing to the buffer
        if (ret == -1){
            printf("spi_write failed: %s\n", strerror(errno));
        } else {
            fprintf(stdout,"spi_write successful! \n");
            fprintf(stdout,"Number of bytes: %d\n",ret);

            // Checking what was sent to the buffer
            for (int i = 0; i < ret; i++) {
                //  printf("Loop %d\n",i);
                sprintf(values, "%#0x ", write_buffer[i]);
                strcat(output, values);
                memset(values, 0, sizeof(values));
            }
            fprintf(stdout, "Sent - Data: %s\n\n", output);
            memset(output, 0, sizeof(output));
        }

        // --------Read from SPI1--------(Placed here for testing purposes)
        // Reading from the buffer
        ret = spi_read(file,SPI_DEV_LOCK,read_buffer,TSPI_WRITE_32);
        if(ret == -1){
            printf("spi_read failed: %s\n", strerror(errno));
        } else {
            fprintf(stdout,"spi_read successful! \n");
            fprintf(stdout,"Number of bytes: %d\n",ret);

            // Displaying what was read from the buffer
            for(int i = 0; i < ret; i++) {
                //  printf("Loop %d\n",i);
                sprintf(values, "%#0x ", read_buffer[i]);
                strcat(output, values);
                memset(values, 0, sizeof(values));
            }
            fprintf(stdout, "Read - Data: %s\n\n", output);
            memset(output, 0, sizeof(output));
        }

        sleep(1);
        counter++;
        printf("Count = %d\n\n",counter);

        if(iterations == counter){
            loop_state = 0;
        }
    }
}

// Closing the communication link to SPI1
int spiclose(){
    ret = spi_close(file);
    fprintf(stdout,"Value returned from spi_close: %d\n\n",ret);
}

// UART write function
int UART_write(char *message, int iterations){
    int counter = 0;
    int living  = 1;
    ret         = 0;
    strcpy(char_write_buffer,message);

    while(living) {
        printf("Test 2\n");
        printf("\n%d Tx: %s", counter, char_write_buffer);
        ret = write(file, &char_write_buffer, strlen(char_write_buffer));
        printf("\n%d Tx: Number of Bytes: %d\n", counter, ret);
        // delay(1000);
        counter++;

        if(iterations == counter){
            living = 0;
        }
    }
    return ret;
}

// UART read function
char *UART_read(){
    int counter = 0;
    int living  = 1;
    ret         = 0;

    while(living) {
        printf("Test 3\n");
        ret = read(file, &char_read_buffer, sizeof(char_read_buffer));
        printf("Test 4\n");
        char_read_buffer[ret] = '\0';
        printf("\n%d Rx: Number of Bytes: %d", counter, ret);
        printf("\n%d Rx: %s\n", counter, char_read_buffer);
        counter++;
        if(counter == 2){
            living = 0;
        }
    }
    return char_read_buffer;
}