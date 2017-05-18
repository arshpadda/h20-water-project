#include<stdio.h>
#include<stdlib.h>
#include<bcm2835.h>
#include<time.h>

/*
 * The i2c address of the atlas sensor.
 */
#define Addr_ph 0x63
#define Addr_c 0x64

/*
 * Define the delay between each read of a sensor.
 * Time is in milli second.
 */
#define del 100 

/*
 * Global time used to read system clock
 */
static time_t t;

/*
 * bcm2835 Library function to set up connection with i2c protocol.
 * Return 1 if connection is successful. 
 */ 
static int setUp_bcm2835(){
	bcm2835_init();
	printf("Init bcm2835 \n");
	return bcm2835_i2c_begin();
}

/*
 * bcm2835 Library function to set up connection with slave device using i2c protocol.
 * Sets up connection with slave device with address 0x63 Addr_ph)
 */ 
static void setUp_addr_ph(){
	//printf("Set I2C Address to %x \n",Addr_ph);
	bcm2835_i2c_setSlaveAddress(Addr_ph);
}

/*
 * bcm2835 Library function to set up connection with slave device using i2c protocol.
 * Sets up connection with slave device with address 0x63 Addr_ph) 
 */ 
static void setUp_addr_c(){
	//printf("Set I2C Address to %x \n",Addr_c);
	bcm2835_i2c_setSlaveAddress(Addr_c);
}

/*
 * Clears the sensor of previous calibration or setting.
 */
static void clearSensor(){
	char clear[] = "cal,clear ";
	bcm2835_i2c_write(clear, 32);
	delay(1000);
	printf("Data has been cleared. Start Reading\n");
	delay(1000);
}

/*
 * Writes a request to slave device for one sensor data reading
 */
static void write_to_I2C(){
	char current[] = "R ";
	bcm2835_i2c_write(current, 32);
}

/*
 * Reads the sensor data of the slave device.
 */ 
static char* read_from_I2C(){
	char* result = (char*) malloc(32 *sizeof(char));
	bcm2835_i2c_read(result, 32);
	return result;
}

/*
 * Delay between two readings
 */ 
static void time_between_reading(){
	delay(del);
}

/*
 * Returns the system clock time. Used for time stamp. 
 */ 
static char* getCurrentTime(time_t t){
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
 * Displays the data from atlas ph sensor and also writes it to a file.
 * If the first bit of the data is 1, then the value of data is a good data.
 * If the first bit of the data is 254, then the device is still processing the data.  
 */ 
static void get_ph(FILE *fp){
	setUp_addr_ph();
	//clearSensor();
	write_to_I2C();
	delay(800);
	char* result = read_from_I2C();
	int i;
	if(result[0] == 1){
		printf("ph Value : ");
		printf("%s ",getCurrentTime(t));
		//
		write_to_file(fp,result);
		write_to_file_comma(fp);
		//
		write_to_file(fp,getCurrentTime(t));
		for(i = 1 ; i < 32 ; i++){
			printf("%c",result[i]);
				if(result[i] == 0)
					break;
		}	
		printf("\n");
	}
	else if(result[0] == 254){
		printf("Still Processing \n");
	}
}

/*
 * Displays the data from atlas conductivity sensor and also writes it to a file.
 * If the first bit of the data is 1, then the value of data is a good data.
 * If the first bit of the data is 254, then the device is still processing the data.  
 */ 
static void get_c(FILE *fp){
	setUp_addr_c();
	//clearSensor();
	write_to_I2C();
	delay(800);
	char* result = read_from_I2C();
	int i;
	if(result[0] == 1){
		printf("Conductivity Value : ");
		printf("%s ",getCurrentTime(t));
		//
		write_to_file(fp,result);
		write_to_file_comma(fp);
		//
		write_to_file(fp,getCurrentTime(t));
		for(i = 1 ; i < 32 ; i++){
			printf("%c",result[i]);
			if(result[i] == 0)
				break;
		}	
		printf("\n");
	}
	else if(result[0] == 254){
		printf("Still Processing \n");
	}
}

int main(){
	int fd = setUp_bcm2835();

	if(fd != 1){
		printf("Error : %d \n",fd);
	}
	else{
		FILE* fp_ph;
		FILE* fp_c;
		fp_ph = fopen("data_ph.csv","w");
		if(fp_ph == NULL){
			printf("Couldn't open file\n");
			return;
		}
		
		fp_c = fopen("data_c.csv","w");
		if(fp_c == NULL){
			printf("Couldn't open file\n");
			return;
		}
		bcm2835_i2c_set_baudrate(100000);
		setUp_addr_ph();
		clearSensor();
		setUp_addr_c();
		clearSensor();
		int i;
		for(i = 0;i <= 5;i++){
			get_ph(fp_ph);
			get_c(fp_c);
			time_between_reading();
			printf("\n");
		}
		fclose(fp_ph);
		fclose(fp_c);	
	}
return 0;
}
