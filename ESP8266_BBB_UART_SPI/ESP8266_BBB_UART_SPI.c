#include <stdio.h>
#include <stdlib.h>

//New Headers
#include <devctl.h>
#include <sys/types.h>
#include <errno.h>
#include <unistd.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/neutrino.h>
#include <hw/spi-master.h>
#include <string.h>
#include <hw/inout.h>    //for in32() and out32()
#include <sys/mman.h>    //for mmap_device_io()
#include <stdint.h>      //for unit32 types

//Register use
#define AM335X_CONTROL_MODULE_BASE (uint64_t)   0x44E10000
#define AM335X_CONTROL_MODULE_SIZE (size_t)     0x00001448
#define AM335X_GPIO_SIZE                        0x00001000
#define AM335X_GPIO1_BASE                       0x4804C000

//GPIO register use
#define GPIO_OE                                 0x134
#define GPIO_DATAOUT                            0x13C

//GPIO Use
#define NHIB                                    (1<<6)    //Pin 3 - nHib
#define IRQ                                     (1<<7)    //Pin 4 - Interrupt


//GPIO/SPI/UART Pin pinmux mode overview
#define GPIO_6_pinConfig                        0x858   //conf_gpmc_a6 (TRM pp 1456) for GPIO1_6
#define GPIO_7_pinConfig                        0x85C   //conf_gpmc_a7 (TRM pp 1456) for GPIO1_7
#define uart1_ctsn_pinConfig                    0x978   //conf_uart1_ctsn (TRM pp 1458) for uart1_ctsn
#define uart1_rtsn_pinConfig                    0x97C   //conf_uart1_rtsn (TRM pp 1458) for uart1_rtsn
#define uart1_rxd_pinConfig                     0x980   //conf_uart1_rxd (TRM pp 1458) for uart1_rxd
#define uart1_txd_pinConfig                     0x984   //conf_uart1_txd (TRM pp 1458) for uart1_txd
#define spi_cs_1_pinConfig                      0x99C   //conf_conf_mcasp0_ahclkr (TRM pp 1458) for spi_1_cs
#define spi_d0_1_pinConfig                      0x994   //conf_conf_mcasp0_fsx (TRM pp 1458) for spi_1_d0
#define spi_d1_1_pinConfig                      0x998   //conf_conf_mcasp0_axr0 (TRM pp 1458) for spi_1_d1
#define spi_sclk_1_pinConfig                    0x990   //conf_conf_mcasp0_aclkx (TRM pp 1458) for spi_1_sclk

//UART - Path
#define UART_PATH       "/dev/ser2"

//Configuring the pinmux for UART1 - PIN MUX Configuration strut values (TRM pp 1446)
#define PU_ENABLE       0x00
#define PU_DISABLE      0x01
#define PU_PULL_UP      0x01
#define PU_PULL_DOWN    0x00
#define RECV_ENABLE     0x01
#define RECV_DISABLE    0x00
#define SLEW_FAST       0x00
#define SLEW_SLOW       0x01

//GPMC_Configurations
#define PIN_MODE_0      0x00
#define PIN_MODE_1      0x01
#define PIN_MODE_2      0x02
#define PIN_MODE_3      0x03
#define PIN_MODE_4      0x04
#define PIN_MODE_5      0x05
#define PIN_MODE_6      0x06
#define PIN_MODE_7      0x07

typedef union _CONF_MODULE_PIN_STRUCT { //See TRM Page 1446
    unsigned int d32;
    struct {                            //Name: field size unsigned
        int conf_mmode :                3;  //LSB
        unsigned int conf_puden :       1;
        unsigned int conf_putypesel :   1;
        unsigned int conf_rxactive :    1;
        unsigned int conf_slewctrl :    1;
        unsigned int conf_res_1 :       13; //Reserved
        unsigned int conf_res_2 :       12; //Reserved MSB
    } b;
} _CONF_MODULE_PIN;

int GPIO_val_high   =           0x01;
int GPIO_val_low    =           0x00;

//Interrupt
#define GPIO_IRQSTATUS_SET_1    0x38    //enable interrupt generation
#define GPIO_IRQWAKEN_1         0x48    //Wakeup Enable for Interrupt Line
#define GPIO_FALLINGDETECT      0x14C   //set falling edge trigger
#define GPIO_CLEARDATAOUT       0x190   //clear data out Register
#define GPIO_IRQSTATUS_1        0x30    //clear any prior IRQs

//SPI - Path & Message size
#define SPI_PATH "/dev/spi1"
#define TSPI_WRITE_SHORT        (8)

int file;
int ret;
uint8_t buffer                  [256 * 1024];
uint8_t j           =           0x02;
uint8_t reg1[7]     =           {0xff, 0x3E, 0x11, 0x00, 0x44, 0x22, 0x66};    //Data to be sent for testing purposes
uint8_t reg2[7]     =           {0xfb, 0x04, 0x04, 0x3b, 0x40, 0x40, 0x3f};    //Data to be sent for testing purposes
uint8_t reg3[7]     =           {0x0f, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02};    //Data to be sent for testing purposes

void Pin_status(){
    printf("0. val = %#8x\n\n",AM335X_GPIO1_BASE);

    //Setting GPIO_1 Pins for use with Interrupt and nHib
    //GPIO global pointers
    uintptr_t gpio1_base = NULL;
    uintptr_t control_module = NULL;

    volatile uint32_t val = 0;

    gpio1_base = mmap_device_io(AM335X_GPIO_SIZE , AM335X_GPIO1_BASE);
    control_module = mmap_device_io(AM335X_CONTROL_MODULE_SIZE, AM335X_CONTROL_MODULE_BASE);

    if (control_module) {
        in32s(&val,1,control_module+GPIO_6_pinConfig);
        printf("Current pinmux configuration for      GPIO6 = %#010x\n", val);

        in32s(&val,1,control_module+GPIO_7_pinConfig);
        printf("Current pinmux configuration for      GPIO7 = %#010x\n", val);

        in32s(&val,1,control_module+spi_cs_1_pinConfig);
        printf("Current pinmux configuration for   SPI 1 CS = %#010x\n", val);

        in32s(&val,1,control_module+spi_d0_1_pinConfig);
        printf("Current pinmux configuration for   SPI 1 D0 = %#010x\n", val);

        in32s(&val,1,control_module+spi_d1_1_pinConfig);
        printf("Current pinmux configuration for   SPI 1 D1 = %#010x\n", val);

        in32s(&val,1,control_module+spi_sclk_1_pinConfig);
        printf("Current pinmux configuration for SPI 1 SCLK = %#010x\n", val);

        in32s(&val,1,control_module+uart1_ctsn_pinConfig);
        printf("Current pinmux configuration for UART 1 CTSN = %#010x\n", val);

        in32s(&val,1,control_module+uart1_rtsn_pinConfig);
        printf("Current pinmux configuration for UART 1 RTSN = %#010x\n", val);

        in32s(&val,1,control_module+uart1_rxd_pinConfig);
        printf("Current pinmux configuration for UART 1 RXD = %#010x\n", val);

        in32s(&val,1,control_module+uart1_txd_pinConfig);
        printf("Current pinmux configuration for UART 1 TXD = %#010x\n\n", val);



        munmap_device_io(control_module, AM335X_CONTROL_MODULE_SIZE);
    }

    if( gpio1_base ) {
        //Write value to output enable
        val &= ~(NHIB|IRQ);
        out32(gpio1_base + GPIO_OE, val);

        val = in32(gpio1_base + GPIO_DATAOUT);  //Read in current value
        printf("1. val = %#8x\n",val);          //Debug
        val &= ~(NHIB|IRQ);                     //Clear the bits that we might change
        printf("2. val = %#8x\n",val);          //Debug
        val |= (NHIB|IRQ);                      //Set the pattern to display (set next value, i++)
        printf("3. val = %#8x\n\n",val);        //Debug
        out32(gpio1_base + GPIO_DATAOUT, val);  //Write new value
        sleep(1);

        printf("Press a key to continue... ");
        char string;
        scanf("%c",&string);
        printf("\n\n");

        munmap_device_io(gpio1_base, AM335X_GPIO_SIZE);
    }
}

void Pin_control (int mode, unsigned int puden, unsigned int putypesel, unsigned int rxactive, unsigned int slewctrl, unsigned int pin) {
    int MODE = mode;
    unsigned int PUDEN = puden;
    unsigned int PUTYPESEL = putypesel;
    unsigned int RXACTIVE = rxactive;
    unsigned int SLEWCTRL = slewctrl;
    unsigned int PIN = pin;

    //Test code to configure the PinMux for use with UART1
    uintptr_t control_module = mmap_device_io(AM335X_CONTROL_MODULE_SIZE, AM335X_CONTROL_MODULE_BASE);
    uintptr_t gpio1_base = mmap_device_io(AM335X_GPIO_SIZE , AM335X_GPIO1_BASE);

    //Set up pin mux for the pins we are going to use (see page 1354 of TRM)
    volatile _CONF_MODULE_PIN pinConfigGPMC;

    //Pin configuration strut
    pinConfigGPMC.d32 = 0;

    //Pin MUX register default setup for input (GPIO input, disable pull up/down - Mode 7)
    pinConfigGPMC.b.conf_slewctrl   = SLEWCTRL;     //Select between faster or slower slew rate
    pinConfigGPMC.b.conf_rxactive   = RXACTIVE;     //Input enable value for the PAD
    pinConfigGPMC.b.conf_putypesel  = PUTYPESEL;    //Pad pullup/pulldown type selection
    pinConfigGPMC.b.conf_puden      = PUDEN;        //Pad pullup/pulldown enable
    pinConfigGPMC.b.conf_mmode      = MODE;         //Pad functional signal mux select 0 - 7

    //Write to the PinMux registers for UART1
    out32(control_module + PIN, pinConfigGPMC.d32);
}

int spiopen() {
    // Open SPI1
    if((file = spi_open(SPI_PATH) ) < 0) {  //Open SPI1
        printf("Error while opening Device File!!\n\n");
    } else {
        printf("SPI1 Opened Successfully\n\n");
    }
    return ret;

}

int spisetcfg() {
    //Configuring the SPI1
    spi_cfg_t spicfg;

    //CLK POL -> 0 || CLK PHA -> 1 || For later use maybe?
    spicfg.mode = (8 & SPI_MODE_CHAR_LEN_MASK)|SPI_MODE_CKPHASE_HALF;
    spicfg.clock_rate = 100000;

    ret = spi_setcfg(file,0,&spicfg);
    if (ret != EOK){
        fprintf(stdout,"spi_setcfg failed: %d\n\n", ret);
    } else {
        fprintf(stdout,"spi_setcfg successful: %d\n\n", ret);
    }
}

int spigetdevinfo() {
    //Ensuring the correct setting have been implemented
    spi_devinfo_t devinfo;
    spi_cfg_t spicfg;

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

int spiwrite(int iter) {
    int p = 1;
    int iterations = iter;
    int counter = 0;
    //Write to SPI1
    while(p) {
        // printf("Counter = %d\n",counter);
        for(int i = 0; i < 7; i++){
            buffer[0] = reg1[0];
            buffer[1] = reg1[1];
            buffer[2] = reg1[2];
            buffer[3] = reg1[3];
            buffer[4] = reg1[4];
            buffer[5] = reg1[5];
            buffer[6] = reg1[6];
            buffer[7] = reg1[7];

            ret = spi_write(file,SPI_DEV_LOCK,buffer,TSPI_WRITE_SHORT);

            j = j << 1;

            if (ret == -1){
                printf("spi_write failed: %s\n", strerror(errno));
            } else {
                fprintf(stdout,"spi_write successful! \n");
                fprintf(stdout,"Number of bytes: %d\n\n",ret);
            }

            //Read from SPI1
            ret = spi_read(file,SPI_DEV_LOCK,buffer,TSPI_WRITE_SHORT);
            if (ret == -1){
                printf("spi_read failed: %s\n", strerror(errno));
            } else {
                fprintf(stdout,"spi_read successful! \n");
                fprintf(stdout,"Number of bytes: %d\n\n",ret);
            }
        }
        sleep(0.5);
        counter++;
        printf("Count = %d\n\n",counter);

        if ( iterations == counter){
            p = 0;
        }
    }
}

int spiclose() {
    ret = spi_close(file);
    fprintf(stdout,"Value returned from spi_close: %d\n\n",ret);
}

int main(void) {
    puts("Hello World!!!"); /* prints Hello World!!! */

    ThreadCtl( _NTO_TCTL_IO_PRIV , NULL); // Request I/O privileges

    //GPIO Check and Configuration Code
    Pin_status();
    Pin_control(PIN_MODE_0,PU_ENABLE,PU_PULL_DOWN,RECV_DISABLE,SLEW_FAST,uart1_ctsn_pinConfig);
    Pin_control(PIN_MODE_0,PU_ENABLE,PU_PULL_DOWN,RECV_DISABLE,SLEW_FAST,uart1_rtsn_pinConfig);
    Pin_status();

    //UART Code
    int fd;
    int ret = 0;
    int p = 1;
    int int_counter = 0;
    char char_counter[5];
    char read_buffer[10000];

    fd = open(UART_PATH,O_RDWR);
    char write_buffer[50] = "AT";
    write(fd,&write_buffer,sizeof(write_buffer));

    ret = read(fd,&read_buffer,sizeof(read_buffer));
    read_buffer[ret] = '\0';
    printf("1. Number of Bytes: %d\n",ret);
    printf("1. Read value: %s\n\n",read_buffer);
    ret = read(fd,&read_buffer,sizeof(read_buffer));
    read_buffer[ret] = '\0';

    //SPI Code
    spiopen(SPI_PATH);
    spisetcfg();

    printf("How many times would you like to send the data? ");
    int input;
    scanf("%d",&input);
    printf("Input: %d\n\n",input);

    spiwrite(input);
    spiclose();

    printf("Main Terminated...!\n");
    return EXIT_SUCCESS;
}
