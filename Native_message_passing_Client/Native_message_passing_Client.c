#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/dispatch.h>
#include <fcntl.h>
#include <share.h>

// Define where the channel is located
// Only need to use one of these (depending if you want to use QNET networking or running it locally):
#define LOCAL_ATTACH_POINT "test_native_message_passing"                                            // Change myname to the same name used for the server code.
#define QNET_ATTACH_POINT  "net/RMIT_BBB_v5_Sachith/dev/name/local/test_native_message_passing"     // Hostname using full path, change myname to the name used for server

#define BUF_SIZE 100


typedef struct {
    struct _pulse hdr;  // Our real data comes after this header
    int ClientID;       // Our data (unique id from client)
    // int data;        // Our data
    char data[BUF_SIZE];
}   my_data;

typedef struct {
    struct _pulse hdr;  // Our real data comes after this header
    char buf[BUF_SIZE]; // Message we send back to clients to tell them the messages was processed correctly.
}   my_reply;


// prototypes
int client(char *sname);

int main(int argc, char *argv[]) {
    printf("This is A Client running\n");

    int ret = 0;
    ret     = client(LOCAL_ATTACH_POINT);

    printf("Main (client) Terminated....\n");
    return ret;
}

/*** Client code ***/
int client(char *sname) {
    my_data msg;
    my_reply reply;

    int server_coid;
    int compare;
    int index    = 0;
    int living   = 1;
    msg.ClientID = 600; // Unique number for this client (optional)
    char message[32];

    printf("Trying to connect to server named: %s\n", sname);
    if ((server_coid = name_open(sname, 0)) == -1) {
        printf("--->ERROR, could not connect to server!\n\n");
        return EXIT_FAILURE;
    }

    printf("Connection established to: %s\n\n", sname);

    // We would have pre-defined data to stuff here
    msg.hdr.type    = 0x00;
    msg.hdr.subtype = 0x00;

    // Do whatever work you wanted with server connection
    while(living) { // send data packets
        printf("Enter the message you wish to send: ");
        if(fgets(message, sizeof message, stdin) != NULL) {
            message[strcspn(message, "\r\n")] = 0;
        } else {
            printf("Error: No characters have been read at end-of-file!\n");
        }
        // scanf("%s", &message);
        // printf("You entered: %s\n", message);

        compare = strcmp(message, "END");
        if (!compare) {
            printf("Equal to intended\n");
            living = 0;
        }
        strcpy(msg.data, message);

        // the data we are sending is in msg.data
        printf("char message: '%s'\n", message);
        printf("Client (ID:%d), sending data packet with the integer value: %s \n", msg.ClientID, msg.data);
        fflush(stdout);

        if (MsgSend(server_coid, &msg, sizeof(msg), &reply, sizeof(reply)) == -1) {
            printf("--->Error data '%s' NOT sent to server\n", msg.data);
            // maybe we did not get a reply from the server
            // break;

            printf("--->Trying to connect to server named: %s\n", sname);
            if ((server_coid = name_open(sname, 0)) == -1) {
                printf("--->ERROR, could not connect to server!\n\n");
                // return EXIT_FAILURE;
            } else {
                printf("Connection established to: %s\n", sname);
            }
        } else { // now process the reply
            printf("Reply is: '%s'\n\n", reply.buf);
        }
        sleep(1);    // wait a few seconds before sending the next data packet
    }

    // Close the connection
    printf("\nSending message to server to tell it to close the connection\n");
    name_close(server_coid);

    return EXIT_SUCCESS;
}
