/*
 * @author - Arsh Deep Singh Padda
 * @version - 1.0, 5/23/2017
 */


#include <stdio.h>
#include <stdlib.h>
#include <wiringPiI2C.h>
#include <time.h>
#include <stdint.h>
#include <fcntl.h>
#include <pthread.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>


/*
 * The i2c address of the ph atlas sensor.
 */
#define Addr_ph_1 0x61
#define Addr_ph_2 0x62
#define Addr_ph_3 0x63


/*
 * The i2c address of the conductivity sensor.
 */
#define Addr_c_1 0x64
#define Addr_c_2 0x65
#define Addr_c_3 0x66


/*
 * Define the delay between each read of a sensor.
 * Time is in milli second.
 */
#define del 7600


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
 * counter is the atlas sensor from which the value is taken.
 */
struct read_write_arg{
	int channel;
	char* buffer;
	FILE* fp;
	int counter;
};


/*
 * Set the I2C Channel 0
 * Pin 27 - SDA (Data)
 * Pin 28 - SCL (Clock)
 * Sets up the ph sensor on Channel 0
 * Returns the file handler.
 */
static int set_I2C_channel_0(int Addr_ph){
	return wiringPiI2CSetupInterface("/dev/i2c-0", Addr_ph);
}


/*
 * Set the I2C Channel 1
 * Pin 3 - SDA (Data)
 * Pin 5 - SCL (Clock)
 * Sets up the conductivity sensor on Channel 1
 * Returns the file handler.
 */
static int set_I2C_channel_1(int Addr_c){
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
static void write_data_to_file(FILE *fp, char* buffer, char* time, int counter){
	if(counter == 1){
		write_to_file(fp,time);
		write_to_file_comma(fp);
	}
	if(counter == 3){
		write_to_file(fp,buffer+1);
		write_to_file_eol(fp);
	}
	else{
		write_to_file(fp,buffer+1);
		write_to_file_comma(fp);
	}
}


/*
 * The function first displays the value in the buffer, then writes the value to the file pointed by fp.
 * The function will display "Still Processing" if the first char of the buffer is a 254.
 */
static void display_write_to_file(char* buffer, FILE* fp, int counter){
	if(buffer[0] == 254){
		printf("Still Processing \n\n");
	}
	else{
		current_time = getCurrentTime();
		int index = strlen(current_time);
		current_time[index-1] = '\0';
		printf("value @ %s : ",current_time);
		printf("%s", buffer);
		write_data_to_file(fp,buffer,current_time,counter);
		printf("\n");
	}
}


/*
 * The multithreading function which writes a command to request data from the atlas sensor and then read the data and
 * puts it in the buffer. It then display the value and writes it to a file. 
 */
static void *read_write(void *arguments){
	struct read_write_arg *data = arguments;
	write_data(data->channel);
	delay(800);
	read_data(data->channel, data->buffer);
	display_write_to_file(data->buffer, data->fp, data->counter);
}


/*
 * Used to get a keyboard interaction.
 * Returns a 1 if keyboard interaction is true i.e. 1 else returns a false i.e. 0.
 * Used to exit out the loop while calibration process.
 */
static int kbhit(void){
	struct termios oldt, newt;
  	int ch;
  	int oldf;

	tcgetattr(STDIN_FILENO, &oldt);
 	newt = oldt;
  	newt.c_lflag &= ~(ICANON | ECHO);
  	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
  	oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
  	fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

  	ch = getchar();

  	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
  	fcntl(STDIN_FILENO, F_SETFL, oldf);

  	if(ch != EOF){
    		ungetc(ch, stdin);
    		return 1;
  	}

  	return 0;
}


/*
 * Used to clear sensor of previous calibration.
 */
static void clear_sensor(int channel){
	char clear_data[] = "cal,clear";
	write(channel, clear_data, 10);
	delay(1000);
}


/*
 * Perform the mid calibration of the atlas ph sensor.
 */
static void mid_calibration_ph(int channel){
	printf(" Performing Midpoint Calibration for ph \n Please use 7.00 ph for calibration. \n Press enter when you are ready. \n Press enter again on seeing a stable reading to calibrate.\n");
	char dummy;
	dummy = getchar();
	while(!kbhit()){
		write_data(channel);
		delay(800);
		char *buffer = (char*)calloc(32, sizeof(char));
		read_data(channel,buffer);
		printf("%s \n", buffer);
	}
	dummy = getchar();
	char set_cal[]="cal,mid,7.00";
	write(channel, set_cal,12);
	delay(300);
}


/*
 * Perform the low calibration of the atlas ph sensor.
 */
static void low_calibration_ph(int channel){
	printf(" Performing Lowpoint Calibration for ph \n Please use 4.00 ph for calibration. \n Press enter when you are ready. \n Press enter again on seeing a stable reading to calibrate.\n");
	char dummy;
	dummy = getchar();
	while(!kbhit()){
		write_data(channel);
		delay(800);
		char *buffer = (char*)calloc(32, sizeof(char));
		read_data(channel,buffer);
		printf("%s \n", buffer);
	}
	dummy = getchar();
	char set_cal[]="cal,low,4.00";
	write(channel, set_cal,12);
	delay(300);
}


/*
 * Perform the high calibration of the atlas ph sensor
 */
static void high_calibration_ph(int channel){
	printf(" Performing Highpoint Calibration for ph \n Please use 10.00 ph for calibration. \n Press enter when you are ready. \n Press enter again on seeing a stable reading to calibrate.\n");
	char dummy;
	dummy = getchar();
	while(!kbhit()){
		write_data(channel);
		delay(800);
		char *buffer = (char*)calloc(32, sizeof(char));
		read_data(channel,buffer);
		printf("%s \n", buffer);
	}
	dummy = getchar();
	char set_cal[]="cal,high,10.00";
	write(channel, set_cal,13);
	delay(300);
}


/*
 * Perform the calibration of atlas ph sensor in the following order:
 * Mid Calibration
 * Low Calibration
 * High Calibration
 */
static void calibration_ph(int channel){
	clear_sensor(channel);
	mid_calibration_ph(channel);
	low_calibration_ph(channel);
	high_calibration_ph(channel);
}


/*
 * Perform the mid calibration of the altas conductivity sensor.
 */
static void mid_calibration_c(int channel){
	printf(" Performing Midpoint Calibration for conductivity \n Please use 1413 for calibration. \n Press enter when you are ready. \n Press enter again on seeing a stable reading to calibrate.\n");
	char dummy;
	dummy = getchar();
	while(!kbhit()){
		write_data(channel);
		delay(800);
		char *buffer = (char*)calloc(32, sizeof(char));
		read_data(channel,buffer);
		printf("%s \n", buffer);
	}
	dummy = getchar();
	char set_cal[]="cal,1413";
	write(channel, set_cal,8);
	delay(300);
}


/*
 * Perform the low calibration of the atlas conductivity sensor.
 */
static void low_calibration_c(int channel){
	printf(" Performing Lowpoint Calibration for conductivity \n Please use 1413 for calibration. \n Press enter when you are ready. \n Press enter again on seeing a stable reading to calibrate.\n");
	char dummy;
	dummy = getchar();
	while(!kbhit()){
		write_data(channel);
		delay(800);
		char *buffer = (char*)calloc(32, sizeof(char));
		read_data(channel,buffer);
		printf("%s \n", buffer);
	}
	dummy = getchar();
	char set_cal[]="cal,low,1413";
	write(channel, set_cal,12);
	delay(300);
}


/*
 * Perform the high calibration of the atlas conductivity sensor.
 */
static void high_calibration_c(int channel){
	printf(" Performing Highpoint Calibration for conductivity \n Please use 12900 for calibration. \n Press enter when you are ready. \n Press enter again on seeing a stable reading to calibrate.\n");
	char dummy;
	dummy = getchar();
	while(!kbhit()){
		write_data(channel);
		delay(800);
		char *buffer = (char*)calloc(32, sizeof(char));
		read_data(channel,buffer);
		printf("%s \n", buffer);
	}
	dummy = getchar();
	char set_cal[]="cal,high,12900";
	write(channel, set_cal,14);
	delay(300);
}


/*
 * Perform the calibration of the atlas conducitivity sensor in the following order:
 * Mid Calibration
 * Low Calibration
 * High Calibration
 */
static void calibration_c(int channel){
	clear_sensor(channel);
	//mid_calibration_c(channel);
	low_calibration_c(channel);
	high_calibration_c(channel);
}


int main(){
	//File handler that will be used to read data and write command to the atlas sensor.
	int channel0_ph1 = set_I2C_channel_0(Addr_ph_1);
	int channel0_ph2 = set_I2C_channel_0(Addr_ph_2);
	int channel0_ph3 = set_I2C_channel_0(Addr_ph_3);
	int channel1_c1 = set_I2C_channel_1(Addr_c_1);
	int channel1_c2 = set_I2C_channel_1(Addr_c_2);
	int channel1_c3 = set_I2C_channel_1(Addr_c_3);
	//If channel 0 or channel 1 is -ve then there is an issue with the connection or you are not running as root (Use sudo before execution
	//of the executable).
	if(channel0_ph1 < 0 || channel0_ph2 < 0 || channel0_ph3 < 0){
		printf("error : failed connection with the I2C channel 0. \n");
		return;
	}

	if(channel1_c1 < 0 || channel1_c2 < 0 || channel1_c3 < 0){
		printf("error : failed connection with the I2C channel 1. \n");
	}

	//Perform the check for calibration.
	char dummy;
	printf("Do you want to perform calibration of all sensors? \n Press y for Yes or n for No.\n Then press Enter\n");
	scanf(" %c",&dummy);
	getchar();
	printf("\n");
	if(dummy == 'y' || dummy == 'Y'){
		printf("****** Calibration for ph sensor 1****** \n");
		calibration_ph(channel0_ph1);
		printf("****** Calibration for ph sensor 2****** \n");
		calibration_ph(channel0_ph2);
		printf("****** Calibration for ph sensor 3****** \n");
		calibration_ph(channel0_ph3);
		printf("****** Calibration for conductivity sensor 1 ******\n");
		calibration_c(channel1_c1);
		printf("****** Calibration for conductivity sensor 2 ******\n");
		calibration_c(channel1_c2);
		printf("****** Calibration for conductivity sensor 3 ******\n");
		calibration_c(channel1_c3);
	}

	printf("\nPress Enter to start data collection. \n");
	while(!kbhit()){
	}
	getchar();

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
	char ph_str[] = "data_ph.csv";
	char c_str[] = "data_c.csv";
/*
	char *temp_time;

	//Add time stamp to file
	time(&t);
	temp_time = (char*)calloc(32, sizeof(char));
	temp_time = ctime(&t);
	int index = strlen(temp_time);
	temp_time[index - 1] = '\0';

	//Concatenate the time stamp to string and make a .csv format file.
	strcat(ph_str, temp_time);
	strcat(ph_str,".csv");
*/
	//Try to create a file and open it.
	fp_ph = fopen(ph_str,"a");

	if(fp_ph == NULL){
		printf("Couldn't open file ph\n");
		return;
	}
/*
	//Add time stamp to file
	time(&t);
	temp_time = (char*)calloc(32, sizeof(char));
	temp_time = ctime(&t);
	index = strlen(temp_time);
	temp_time[index - 1] = '\0';

	//Concatenate the time stamp to string and make a .csv format file.
	strcat(c_str, temp_time);
	strcat(c_str,".csv");
*/
	//Try to create a file and open it.
	fp_c = fopen(c_str,"a");

	if(fp_c == NULL){
		printf("Couldn't open file c\n");
		return;
	}

	//Create threads used to multithread.
	pthread_t thread_ph;
	pthread_t thread_c;

	//Create the structure to pass the data for ph in multithreading.
	struct read_write_arg ph_data;
	//ph_data.channel = channel0_ph1;
	ph_data.buffer = buffer_ph;
	ph_data.fp = fp_ph;

	//Create the structure to pass the data for conductivity in multithreading.
	struct read_write_arg c_data;
	//c_data.channel = channel1_c1;
	c_data.buffer = buffer_c;
	c_data.fp = fp_c;

	//Time to keep track of the time for which it records.
	time_t end_time;
	time_t start_time = time(NULL);
	time_t seconds= 100;
	end_time = start_time + seconds;
	printf("Data Collection starts at time %s",ctime(&start_time));

	while(start_time < end_time){
	//Reading Pair 1
	ph_data.channel = channel0_ph1;
	ph_data.counter = 1;
	c_data.channel = channel1_c1;
	c_data.counter = 1;

	//Create the thread and run them.
	pthread_create(&thread_ph, NULL, &read_write,(void *)&ph_data);
	pthread_create(&thread_c, NULL, &read_write,(void *)&c_data);

	//Wait for the thread to finish.
	pthread_join(thread_ph, NULL);
	pthread_join(thread_c, NULL);

	//Reading Pair 2
	ph_data.channel = channel0_ph2;
	ph_data.counter = 2;
	c_data.channel = channel1_c2;
	c_data.counter = 2;

	//Create the thread and run them.
	pthread_create(&thread_ph, NULL, &read_write, (void *)&ph_data);
	pthread_create(&thread_c, NULL, &read_write, (void *)&c_data);

	//Wait for the thread to finish.
	pthread_join(thread_ph, NULL);
	pthread_join(thread_c, NULL);

	//Reading the Pair 3
	ph_data.channel = channel0_ph3;
	ph_data.counter = 3;
	c_data.channel = channel1_c3;
	c_data.counter = 3;

	//Create the thread and run them.
	pthread_create(&thread_ph, NULL, &read_write, (void *)&ph_data);
	pthread_create(&thread_c, NULL, &read_write, (void *)&c_data);

	//Wait for the thread to finish.
	pthread_join(thread_ph, NULL);
	pthread_join(thread_c, NULL);

	printf("\n");

	//Update the time.
	start_time = time(NULL);

	//Delay between the readings.
	delay(del);

	}
	printf("Data collection ends at time %s",ctime(&start_time));
	//Data Backup
	printf("Performing Data Backup in DropBox. \n");
	system("sudo bash /home/pi/Dropbox-Uploader/dropbox_uploader.sh upload /home/pi/Desktop/New/ /");
return 0;
}
