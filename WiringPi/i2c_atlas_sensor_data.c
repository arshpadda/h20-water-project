#include<stdio.h>
#include<stdlib.h>
#include<wiringPiI2C.h>
#include<time.h>
#include<stdint.h>
#include<fcntl.h>
#include<pthread.h>
#include<string.h>


/*
 * The i2c address of the atlas sensor.
 */
#define Addr_ph 0x63
#define Addr_c 0x64


/*
 * Define the delay between each read of a sensor.
 * Time is in milli second.
 */
#define del 9200


/*
 * Global time used to read system clock
 */
static time_t t;


/*
 * Global string used to read sustem time in String format
 */
static char* current_time;


/*
 * Struct type to pass the arguments in the multithreading.
 * channel is the file handler for the device which is used to access the atlas sensor.
 * buffer is the pointer to the string which will hold the data from the atlas senor.
 * fp is the file handler to the file in which data will be written.
 */
struct read_write_arg{
	int channel;
	char* buffer;
	FILE* fp;
};


/*
 * Set the I2C Channel 0
 * Pin 27 - SDA (Data)
 * Pin 28 - SCL (Clock)
 * Sets up the ph sensor on Channel 0
 * Returns the file handler.
 */
static int set_I2C_channel_0(){
	return wiringPiI2CSetupInterface("/dev/i2c-0", Addr_ph);
}


/*
 * Set the I2C Channel 1
 * Pin 3 - SDA (Data)
 * Pin 5 - SCL (Clock)
 * Sets up the conductivity sensor on Channel 1
 * Returns the file handler.
 */
static int set_I2C_channel_1(){
	return wiringPiI2CSetupInterface("/dev/i2c-1", Addr_c);
}


/*
 * Write a command to get data from the channel specified
 * Writes a "R" (0x72) to the atlas sensor.
 */
static void write_data(int channel){
	wiringPiI2CWrite(channel, 0x72);
}


/*
 * Read the data provided by the atlas sensor.
 */
static void read_data(int channel,char *buffer){
	read(channel, buffer, 32);
}


/*
 * Returns the system clock time. Used for time stamp.
 */
static char* getCurrentTime(){
	time(&t);
	return ctime(&t);
}


/*
 * Writes the given string to the file pointed by the file pointer.
 */
static void write_to_file(FILE *fp, char* str){
	fprintf(fp,"%s",str);
}


/*
 * Writes a comma(,) to the file pointed by the file pointer. Used to switch to next row in .csv file
 */
static void write_to_file_comma(FILE *fp){
	fprintf(fp,",");
}


/*
 * Writes a end of line(\n) to the file pointed by the file pointer. Used to switch to next column in .csv file
 */
static void write_to_file_eol(FILE *fp){
	fprintf(fp,"\n");
}


/*
 * Write the data, then the comma, then time stamp to the file nad finally the end of line.
 */
static void write_data_to_file(FILE *fp, char* buffer, char* time){
	write_to_file(fp,time);
	write_to_file_comma(fp);
	write_to_file(fp,buffer);
	write_to_file_eol(fp);
}

/*
 * The function first displays the value in the buffer, then writes the value to the file pointed by fp.
 * The function will display "Still Processing" if the first char of the buffer is a 254.
 */
void display_write_to_file(char* buffer, FILE* fp){
	if(buffer[0] == 254){
		printf("Still Processing \n\n");
	}
	else{
		current_time = getCurrentTime();
		int index = strlen(current_time);
		current_time[index-1] = '\0';
		printf("value @ %s : ",current_time);
		printf("%s", buffer);
		write_data_to_file(fp,buffer,current_time);
		printf("\n");
	}
}


/*
 * The multithreading function which writes a command to request data from the atlas sensor and then read the data and
 * puts it in the buffer. It then display the value and writes it to a file. 
 */
void *read_write(void *arguments){
	struct read_write_arg *data = arguments;
	write_data(data->channel);
	delay(800);
	read_data(data->channel, data->buffer);
	display_write_to_file(data->buffer, data->fp);
}


int main(){
	//File handler that will be used to read data and write command to the atlas sensor.
	int channel0 = set_I2C_channel_0();
	int channel1 = set_I2C_channel_1();

	//If channel 0 or channel 1 is -ve then there is an issue with the connection or you are not running as root (Use sudo before execution
	//of the executable). 
	if(channel0 < 0 || channel1 < 0){
		printf("error : failed connection with the I2C channel \n");
		return;
	}

	printf("Creating buffer for data input\n");
	//Create pointer to the buffer that will hold the data returned from the atlas sensor. 
	char *buffer_ph;
	char *buffer_c;

	buffer_ph = (char*)calloc(32, sizeof(char));
	buffer_c = (char*)calloc(32, sizeof(char));

	//dummy variable
	unsigned char read1 = 0x90;

	//Variable for loop traversal/
	int i,j;

	//Create pointer for the file that will be used to write the data.
	FILE* fp_ph;
	FILE* fp_c;

	//Try to create a file with write permission to store ph values.
	fp_ph = fopen("data_ph.csv","w");

	if(fp_ph == NULL){
		printf("Couldn't open file ph\n");
		return;
	}

	//Try to create a file with write permission to store conductivity values
	fp_c = fopen("data_c.csv","w");

	if(fp_c == NULL){
		printf("Couldn't open file c\n");
		return;
	}

	//Create threads used to multithread.
	pthread_t thread_ph;
	pthread_t thread_c;

	//Create the structure to pass the data for ph in multithreading.
	struct read_write_arg ph_data;
	ph_data.channel = channel0;
	ph_data.buffer = buffer_ph;
	ph_data.fp = fp_ph;

	//Create the structure to pass the data for conductivity in multithreading.
	struct read_write_arg c_data;
	c_data.channel = channel1;
	c_data.buffer = buffer_c;
	c_data.fp = fp_c;

	for(i=0;i<5;i++){
	//Create the thread and run them.
	pthread_create(&thread_ph, NULL, &read_write,(void *)&ph_data);
	pthread_create(&thread_c, NULL, &read_write,(void *)&c_data);

	//Wait for the thread to finish.
	pthread_join(thread_ph, NULL);
	pthread_join(thread_c, NULL);

	printf("\n");

	delay(del);
	}

return 0;
}
