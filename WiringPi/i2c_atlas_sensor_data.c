/*
 * This i2c_atlas_sensor_data program implements an application that
 * collects data from sensors and displays the data on standard output.
 * Also, it sends the data on the socket for a python application to psuh
 * data on the Google cloud pub/sub.
 * @author - Arsh Deep Singh Padda.
 * @version - 1.0, 7/12/2017.
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
#include <zmq.h>
#include <assert.h>


/*
 * The i2c address of the ph atlas sensor.
 */
#define ADDR_PH_1 0x61
#define ADDR_PH_2 0x62
#define ADDR_PH_3 0x63


/*
 * The i2c address of the conductivity sensor.
 */
#define ADDR_C_1 0x64
#define ADDR_C_2 0x65
#define ADDR_C_3 0x66


/*
 * Define the delay between each read of a sensor.
 * Time is in milli second.
 */
#define DEL 57600


/*
 * Define global string pointer to contain the string to be send from the server.
 */
static char* kCloudDataPh;
static char* kCloudDataCond;


/*
 * Struct type to pass the arguments in the multithreading.
 * channel is the file handler for the device which is used to access the atlas sensor.
 * buffer is the pointer to the string which will hold the data from the atlas senor.
 * type is to identify whether the arguments belong to ph or conductivity.
 * counter is the atlas sensor from which the value is taken.
 */
struct ReadWriteArg {
	int channel;
	char* buffer;
	char type;
	int counter;
};


/*
 * Set the I2C Channel 0.
 * Pin 27 - SDA (Data).
 * Pin 28 - SCL (Clock).
 * Sets up the ph sensor on Channel 0.
 * Returns the file handler.
 */
static int SetI2cChannel0(int Addr_ph) {
	return wiringPiI2CSetupInterface("/dev/i2c-0", Addr_ph);
}


/*
 * Set the I2C Channel 1.
 * Pin 3 - SDA (Data).
 * Pin 5 - SCL (Clock).
 * Sets up the conductivity sensor on Channel 1.
 * Returns the file handler.
 */
static int SetI2cChannel1(int Addr_c) {
	return wiringPiI2CSetupInterface("/dev/i2c-1", Addr_c);
}


/*
 * Write a command to get data from the channel specified.
 * Writes a "R" (0x72) to the atlas sensor.
 */
static void WriteData(int channel) {
	wiringPiI2CWrite(channel, 0x72);
}


/*
 * Read the data provided by the atlas sensor.
 */
static void ReadData(int channel, char *buffer) {
	read(channel, buffer, 32);
}


/*
 * Send Data via the socket from the Server.
 */
static int SendData(void *socket, char *string) {
	int size = zmq_send(socket, string, strlen(string), 0);
	return size;
}


/*
 * Write the data, then the comma, then time stamp to the file nad finally the end of line.
 */
static void WriteDataToFile(char* buffer, char* time, int counter, char type) {
	//If the counter value is equal to one then we need to write the time as it is the first value in the new row.
	if (counter == 1) {
		if (type == 'c') {
			strcpy(kCloudDataCond, time);
		}
		else {
			strcpy(kCloudDataPh, time);
		}
	}
	//If the counter value is equal to three then we need to write the end of file as it is the last value in the current row.
	if (counter == 3) {
		if (type == 'c') {
			strcat(kCloudDataCond, ",");
			strcat(kCloudDataCond, buffer + 1);
		}
		else {
			strcat(kCloudDataPh, ",");
			strcat(kCloudDataPh, buffer + 1);
		}
	}
	else {
		if (type == 'c') {
			strcat(kCloudDataCond, ",");
			strcat(kCloudDataCond, buffer + 1);
		}
		else {
			strcat(kCloudDataPh, ",");
			strcat(kCloudDataPh, buffer + 1);
		}
	}
}


/*
 * The function first displays the value in the buffer, then writes the value to the file pointed by fp.
 * The function will display "Still Processing" if the first char of the buffer is a 254.
 */
static void DisplayWriteToFile(char* buffer, int counter, char type) {
	if (buffer[0] == 254) {
		printf("Still Processing \n\n");
	}
	else {
		time_t timer;
		char* time_buffer = (char*)calloc(32, sizeof(char));
		struct tm* tm_info;

		timer = time(NULL);
		tm_info = localtime(&timer);

		//Time Format : YYYY-MM-DD, HH:MM.
		strftime(time_buffer, 26, "%Y-%m-%d,%H:%M", tm_info);
		printf("%s", time_buffer);

		//We require only the first value before comma in conductivity.
		if (type == 'c') {
			int loop;
			for (loop = 0; loop < strlen(buffer); loop++) {
				if (buffer[loop] == ',') {
					buffer[loop] = '\0';
					break;
				}
			}
		}

		printf(" : %s", buffer + 1);
		WriteDataToFile(buffer, time_buffer, counter, type);
		printf("\n");
	}
}


/*
 * The multithreading function which writes a command to request data from the atlas sensor and then read the data and
 * puts it in the buffer. It then display the value and writes it to a file.
 */
static void *ReadWrite(void *arguments) {
	//Put the arguments in the new struct.
	struct ReadWriteArg *data = arguments;
	WriteData(data->channel);
	delay(800);
	ReadData(data->channel, data->buffer);
	DisplayWriteToFile(data->buffer, data->counter, data->type);
}


/*
 * Used to get a keyboard interaction.
 * Returns a 1 if keyboard interaction is true i.e. 1 else returns a false i.e. 0.
 * Used to exit out the loop while calibration process.
 * Link : https://cboard.cprogramming.com/c-programming/63166-KeyBoardHit-linux.html
 */
static int KeyBoardHit(void) {
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

	if (ch != EOF) {
		ungetc(ch, stdin);
		return 1;
	}

	return 0;
}


/*
 * Used to clear sensor of previous calibration.
 * Important to clear the sensor of the previous calibration in order to ensure the new calibration is effective.
 */
static void ClearSensor(int channel) {
	char clear_data[] = "cal,clear";
	write(channel, clear_data, 9);
	delay(1000);
}


/*
 * Perform the mid calibration of the atlas ph sensor.
 */
static void MidCalibrationPh(int channel) {
	printf(" Performing Midpoint Calibration for ph \n Please use 7.00 ph for calibration. \n Press enter when you are ready. \n Press enter again on seeing a stable reading to calibrate.\n");
	getchar();
	while (!KeyBoardHit()) {
		WriteData(channel);
		delay(800);
		char *buffer = (char*)calloc(32, sizeof(char));
		ReadData(channel, buffer);
		printf("%s \n", buffer);
	}
	getchar();
	char set_cal[] = "cal,mid,7.00";
	write(channel, set_cal, 12);
	delay(300);
}


/*
 * Perform the low calibration of the atlas ph sensor.
 */
static void LowCalibrationPh(int channel) {
	printf(" Performing Lowpoint Calibration for ph \n Please use 4.00 ph for calibration. \n Press enter when you are ready. \n Press enter again on seeing a stable reading to calibrate.\n");
	getchar();
	while (!KeyBoardHit()) {
		WriteData(channel);
		delay(800);
		char *buffer = (char*)calloc(32, sizeof(char));
		ReadData(channel, buffer);
		printf("%s \n", buffer);
	}
	getchar();
	char set_cal[] = "cal,low,4.00";
	write(channel, set_cal, 12);
	delay(300);
}


/*
 * Perform the high calibration of the atlas ph sensor.
 */
static void HighCalibrationPh(int channel) {
	printf(" Performing Highpoint Calibration for ph \n Please use 10.00 ph for calibration. \n Press enter when you are ready. \n Press enter again on seeing a stable reading to calibrate.\n");
	getchar();
	while (!KeyBoardHit()) {
		WriteData(channel);
		delay(800);
		char *buffer = (char*)calloc(32, sizeof(char));
		ReadData(channel, buffer);
		printf("%s \n", buffer);
	}
	getchar();
	char set_cal[] = "cal,high,10.00";
	write(channel, set_cal, 13);
	delay(300);
}


/*
 * Perform the calibration of atlas ph sensor in the following order:
 * Clear Sensor.
 * Mid Calibration Ph Sensor.
 * Low Calibration Ph Sensor.
 * High Calibration Ph Sensor.
 */
static void CalibrationPh(int channel) {
	ClearSensor(channel);
	MidCalibrationPh(channel);
	LowCalibrationPh(channel);
	HighCalibrationPh(channel);
}


/*
 * Perform the dry calibration of the atlas conductivity sensor.
 */
static void DryCalibrationCond(int channel) {
	printf(" Performing Dry Calibration for conductivity. \n Press enter when you are ready. \n Press enter again on seeing a stable reading to calibrate.\n");
	getchar();
	while (!KeyBoardHit()) {
		WriteData(channel);
		delay(800);
		char *buffer = (char*)calloc(32, sizeof(char));
		ReadData(channel, buffer);
		printf("%s \n", buffer);
	}
	getchar();
	char set_cal[] = "cal,dry";
	write(channel, set_cal, 7);
	delay(800);
}


/*
 * Perform the low calibration of the atlas conductivity sensor.
 */
static void LowCalibrationCond(int channel) {
	printf(" Performing Lowpoint Calibration for conductivity \n Please use 12900 for calibration. \n Press enter when you are ready. \n Press enter again on seeing a stable reading to calibrate.\n");
	getchar();
	while (!KeyBoardHit()) {
		WriteData(channel);
		delay(800);
		char *buffer = (char*)calloc(32, sizeof(char));
		ReadData(channel, buffer);
		printf("%s \n", buffer);
	}
	getchar();
	char set_cal[] = "cal,low,12900";
	write(channel, set_cal, 13);
	delay(800);
}


/*
 * Perform the high calibration of the atlas conductivity sensor.
 */
static void HighCalibrationCond(int channel) {
	printf(" Performing Highpoint Calibration for conductivity \n Please use 50000 for calibration. \n Press enter when you are ready. \n Press enter again on seeing a stable reading to calibrate.\n");
	getchar();
	while (!KeyBoardHit()) {
		WriteData(channel);
		delay(800);
		char *buffer = (char*)calloc(32, sizeof(char));
		ReadData(channel, buffer);
		printf("%s \n", buffer);
	}
	getchar();
	char set_cal[] = "cal,high,50000";
	write(channel, set_cal, 14);
	delay(800);
}


/*
 * Perform the calibration of the atlas conducitivity sensor in the following order:
 * Clear Sensor.
 * Dry Calibration Cond Sensor.
 * Low Calibration Cond Sensor.
 * High Calibration Cond Sensor.
 */
static void CalibrationCond(int channel) {
	ClearSensor(channel);
	DryCalibrationCond(channel);
	LowCalibrationCond(channel);
	HighCalibrationCond(channel);
}


/*
 * Used to check the value read by the sensors.
 * Does not store the data.
 */
void DryRun(int channel) {
	char *buffer = (char*)calloc(32, sizeof(char));
	while (!KeyBoardHit()) {
		WriteData(channel);
		delay(1000);
		read(channel, buffer, 32);
		printf("%s\n", buffer);
	}
	getchar();
}


void DisplayMainMenu() {
	printf("Select one of the the following option : \n");
	printf("0. Exit. \n");
	printf("1. Calibration of all sensors. \n");
	printf("2. Calibration of one particular sensor. \n");
	printf("3. Dry run to see values on each sensors. No Data Recording. \n");
	printf("4. Start Data Collection. \n");
}


void DisplayAskCalibrationAllSensor() {
	printf("Do you want to perform calibration of all sensors? \n Press y for Yes or n for No.\n Then press Enter\n");
}


void DisplayCalibrationPhSensor(int number) {
	printf("****** Calibration for ph sensor %d ****** \n", number);
}

void DisplayCalibrationCondSensor(int number) {
	printf("****** Calibration for conductivity sensor %d ******\n", number);
}


void DisplayCalibrationOneSensor() {
	printf("Select one of the the following sensor to calibrate : \n");
	printf("1. Calibrate ph sensor 1. \n");
	printf("2. Calibrate ph sensor 2. \n");
	printf("3. Calibrate ph sensor 3. \n");
	printf("4. Calibrate conductivity sensor 1. \n");
	printf("5. Calibrate conductivity sensor 2. \n");
	printf("6. Calibrate conductivity sesnor 3. \n");
}


void DisplayDryRunOption() {
	printf("Select one of the the following sensor to dry run : \n");
	printf("1. ph sensor 1. \n");
	printf("2. ph sensor 2. \n");
	printf("3. ph sensor 3. \n");
	printf("4. conductivity sensor 1. \n");
	printf("5. conductivity sensor 2. \n");
	printf("6. conductivity sesnor 3. \n");
}

void DisplayDryRunPh(int number) {
	printf("****** Dry run for ph sensor %d ****** \n", number);
}

void DisplayDryRunCond(int number) {
	printf("****** Dry run for conductivity sensor %d ******\n", number);
}


int main() {

	//File handler that will be used to read data and write command to the atlas sensor.
	int channel0_ph1 = SetI2cChannel0(ADDR_PH_1);
	int channel0_ph2 = SetI2cChannel0(ADDR_PH_2);
	int channel0_ph3 = SetI2cChannel0(ADDR_PH_3);
	int channel1_c1 = SetI2cChannel1(ADDR_C_1);
	int channel1_c2 = SetI2cChannel1(ADDR_C_2);
	int channel1_c3 = SetI2cChannel1(ADDR_C_3);

	//If channel 0 or channel 1 is -ve then there is an issue with the connection or you are not running as root (Use sudo before execution
	//of the executable).
	if (channel0_ph1 < 0 || channel0_ph2 < 0 || channel0_ph3 < 0) {
		printf("error : failed connection with the I2C channel 0. \n");
		return -1;
	}

	if (channel1_c1 < 0 || channel1_c2 < 0 || channel1_c3 < 0) {
		printf("error : failed connection with the I2C channel 1. \n");
		return -1;
	}

	int option;

	//Print the selection menu until the user presses 0.
	do {

		//Display the options.
		DisplayMainMenu();

		//Get the user input.
		scanf("%d", &option);
		getchar();

		//Select the right option to user's input.
		switch (option) {
		case 0:

			//Exit case. Break and exit the program.
			break;

		case 1:

			//Calibrate all sensors case. Perform calibration on all sensors one by one.
			DisplayAskCalibrationAllSensor();
			char dummy;

			//Get the user input.
			scanf(" %c", &dummy);
			getchar();
			printf("\n");
			if (dummy == 'y' || dummy == 'Y') {
				DisplayCalibrationPhSensor(1);
				CalibrationPh(channel0_ph1);
				DisplayCalibrationPhSensor(2);
				CalibrationPh(channel0_ph2);
				DisplayCalibrationPhSensor(3);
				CalibrationPh(channel0_ph3);
				DisplayCalibrationCondSensor(1);
				CalibrationCond(channel1_c1);
				DisplayCalibrationCondSensor(2);
				CalibrationCond(channel1_c2);
				DisplayCalibrationCondSensor(3);
				CalibrationCond(channel1_c3);
			}
			break;

		case 2:

			//Calibrate one sensor case. Perform calibration on only one sensor.
			printf("\n");

			//Display the options.
			DisplayCalibrationOneSensor();
			int cal_option;

			//Get the user input.
			scanf("%d", &cal_option);
			getchar();
			switch (cal_option) {
			case 1:
				//Calibrate ph sensor 1.
				DisplayCalibrationPhSensor(1);
				CalibrationPh(channel0_ph1);
				break;
			case 2:
				//Calibrate ph sensor 2.
				DisplayCalibrationPhSensor(2);
				CalibrationPh(channel0_ph2);
				break;
			case 3:
				//Calibrate ph sensor 3.
				DisplayCalibrationPhSensor(3);
				CalibrationPh(channel0_ph3);
				break;
			case 4:
				//Calibrate conductivity sensor 1.
				DisplayCalibrationCondSensor(1);
				CalibrationCond(channel1_c1);
				break;
			case 5:
				//Calibrate conductivity sensor 2.
				DisplayCalibrationCondSensor(2);
				CalibrationCond(channel1_c2);
				break;
			case 6:
				//Calibrate conductivity sensor 3.
				DisplayCalibrationPhSensor(3);
				CalibrationCond(channel1_c3);
				break;
			default:
				//Invalid Option Case.
				printf("Invalid option \n");
			}
			break;

		case 3:
			//Dry Run Case. Display the value read by the sesnor. Don't record the data.
			//Display the sensors option.
			DisplayDryRunOption();
			int dry_option;

			//Get the user input.
			scanf("%d", &dry_option);
			getchar();
			switch (dry_option) {
			case 1:
				//Dry run ph sensor 1.
				DisplayDryRunPh(1);
				DryRun(channel0_ph1);
				break;
			case 2:
				//Calibrate ph sensor 2.
				DisplayDryRunPh(2);
				DryRun(channel0_ph2);
				break;
			case 3:
				//Calibrate ph sensor 3.
				DisplayDryRunPh(3);
				DryRun(channel0_ph3);
				break;
			case 4:
				//Calibrate conductivity sensor 1.
				DisplayDryRunCond(1);
				DryRun(channel1_c1);
				break;
			case 5:
				//Calibrate conductivity sensor 2.
				DisplayDryRunCond(2);
				DryRun(channel1_c2);
				break;
			case 6:
				//Calibrate conductivity sensor 1.
				DisplayDryRunCond(3);
				DryRun(channel1_c3);
				break;
			default:
				printf("Invalid option \n");
			}
			break;

		case 4:
			//Data Collection Case. Start data collection.
			printf("\nPress Enter to start data collection. \n");
			while (!KeyBoardHit()) {
			}

			getchar();
			printf("Creating buffer for data input\n");

			//Create pointer to the buffer that will hold the data returned from the atlas sensor.
			char *buffer_ph;
			char *buffer_c;

			buffer_ph = (char*)calloc(32, sizeof(char));
			buffer_c = (char*)calloc(32, sizeof(char));

			//Set up server to socket localhost:5556.
			void *context = zmq_ctx_new();
			void *publisher = zmq_socket(context, ZMQ_PUB);
			int rc = zmq_bind(publisher, "tcp://*:5556");
			assert(rc == 0);

			//Create threads used to multithread.
			pthread_t thread_ph;
			pthread_t thread_c;

			//Create the structure to pass the data for ph in multithreading.
			struct ReadWriteArg ph_data;
			ph_data.buffer = buffer_ph;
			ph_data.type = 'p';

			//Create the structure to pass the data for conductivity in multithreading.
			struct ReadWriteArg c_data;
			c_data.buffer = buffer_c;
			c_data.type = 'c';

			//Time to keep track of the time for which it records.
			time_t end_time;
			time_t start_time = time(NULL);

			//4 hours = 4 * 60 min = 4 * 60 * 60 seconds = 14400 seconds.
			//4.5 hours = 16200 seconds.
			//30 min = 30 * 60 = 1800 seconds.
			//1 hour = 60 * 60 = 3600 seconds.
			time_t seconds = 14400;

			end_time = start_time + seconds;
			printf("Data Collection starts at time %s", ctime(&start_time));
			//

			while (start_time < end_time) {

				kCloudDataPh = (char*)calloc(254, sizeof(char));
				kCloudDataCond = (char*)calloc(254, sizeof(char));

				//Reading Pair 1.
				ph_data.channel = channel0_ph1;
				ph_data.counter = 1;
				c_data.channel = channel1_c1;
				c_data.counter = 1;

				//Create the thread and run them.
				pthread_create(&thread_ph, NULL, &ReadWrite, (void *)&ph_data);
				pthread_create(&thread_c, NULL, &ReadWrite, (void *)&c_data);

				//Wait for the thread to finish.
				pthread_join(thread_ph, NULL);
				pthread_join(thread_c, NULL);

				//Reading Pair 2.
				ph_data.channel = channel0_ph2;
				ph_data.counter = 2;
				c_data.channel = channel1_c2;
				c_data.counter = 2;

				//Create the thread and run them.
				pthread_create(&thread_ph, NULL, &ReadWrite, (void *)&ph_data);
				pthread_create(&thread_c, NULL, &ReadWrite, (void *)&c_data);

				//Wait for the thread to finish.
				pthread_join(thread_ph, NULL);
				pthread_join(thread_c, NULL);

				//Reading the Pair 3.
				ph_data.channel = channel0_ph3;
				ph_data.counter = 3;
				c_data.channel = channel1_c3;
				c_data.counter = 3;

				//Create the thread and run them.
				pthread_create(&thread_ph, NULL, &ReadWrite, (void *)&ph_data);
				pthread_create(&thread_c, NULL, &ReadWrite, (void *)&c_data);

				//Wait for the thread to finish.
				pthread_join(thread_ph, NULL);
				pthread_join(thread_c, NULL);

				SendData(publisher, kCloudDataPh);
				SendData(publisher, kCloudDataCond);
				printf("\n");

				//Delay between the readings.
				delay(DEL);

				//Update the time.
				start_time = time(NULL);

			}

			printf("Data collection ends at time %s", ctime(&start_time));

			zmq_close(publisher);
			zmq_ctx_destroy(context);

			break;

		default:
			printf("Invalid option.\n");
		}
	} while (option != 0);

	return 0;
}
