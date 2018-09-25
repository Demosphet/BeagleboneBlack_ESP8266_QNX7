/*
 *  mqsend.c
 *  Simple example of using mqueue messaging between processes
 *
 *  This code requires that the mqueue service is started on the target.
 *  The mqueue manager implements POSIX 1003.1b message queues.
 *  By default it is not started on QNX 6.6 VM target - just type
 *  'mqueue' at the terminal prior to starting this processes.
 *
 *  For more info, read: Why use POSIX message queues?
 *  http://www.qnx.com/developers/docs/660/index.jsp
 *
 *
 *
 *  Run mqsend in one terminal window, then after a few messages
 *  are queued (less than 5) run mqreceive in another terminal window
 *
*/
#include <stdlib.h>
#include <stdio.h>
#include <mqueue.h>
#include <sys/stat.h>

#define  MESSAGESIZE 1000

#define Q_FLAGS O_RDWR | O_CREAT
#define Q_Mode S_IRUSR | S_IWUSR
/*
 * For oflag overview see: http://www.qnx.com/developers/docs/660/index.jsp
 * O_RDWR	- send-receive   (others: O_RDONLY (receive-only), O_WRONLY (send-only))
 * O_CREAT	- instruct the server to create a new message queue
 *
 * Mode flags overview see: http://www.qnx.com/developers/docs/660/index.jsp
 * S_IRUSR	- Read permission
 * S_IWUSR	- Write permission
 */

int main(int argc, char *argv[])
{
	printf("Welcome to the QNX Momentics mqueue send process\n");

    mqd_t	qd;
    int		i;
    char	buf[MESSAGESIZE];

    struct  mq_attr  attr;
    // attr.mq_maxmsg = 100;
    attr.mq_msgsize = MESSAGESIZE;
    attr.mq_flags = 0;

	// example using the default path notation.
	const char * MqueueLocation = "/test_queue";	/* will be located /dev/mqueue/test_queue  */
    //const char * MqueueLocation = "/net/VM-Target01/dev/mqueue/test_queue"; // (when Bridged use: VM-Target01.sece-lab.rmit.edu.au)
    /* Use the above line for networked (qnet) MqueueLocation
	 * the command 'hostname <name>' to set hostname. here it is 'M1'
	 * You mast also have qnet running. to do this execute the following
	 * command: mount -T io-net /lib/dll/lsm-qnet.so
	 */

    qd = mq_open(MqueueLocation, Q_FLAGS, Q_Mode, &attr);			 // full path will be: <host_name>/dev/mqueue/test_queue
    if (qd != -1)
    {
		for (i=1; i <= 10; ++i)
        {
			sprintf(buf, "message %d", i);			//put the message in a char[] so it can be sent
			printf("queue: '%s'\n", buf); 			//print the message to this processes terminal
			mq_send(qd, buf, MESSAGESIZE, 0);		//send the mqueue
			sleep(2);
		}
		mq_send(qd, "done", 5, 0);					// send last message so the receive process knows
													// not to expect any more messages. 5 char long because
													// of '/0' char at end of the "done" string
		printf("\nAll Messages sent to the queue\n");

		// as soon as this code executes the mqueue data will be deleted
		// from the /dev/mqueue/test_queue  file structure
		mq_close(qd);
		mq_unlink(MqueueLocation);
    }
    else
    {
    	printf("\nThere was an ERROR opening the message queue!");
    	printf("\nHave you started the 'mqueue' process on the VM target?\n");
    }

	printf("\nmqueue send process Exited\n");
	return EXIT_SUCCESS;
}
