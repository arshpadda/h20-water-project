# h20-water-project


Instruction to run the code.
Install the bcm2835 Library

# download the latest version of the library, bcm2835-1.52.tar.gz, then:
tar zxvf bcm2835-1.52.tar.gz <br>
cd bcm2835-1.52 <br>
./configure <br>
make <br>
sudo make check <br>
sudo make install <br>

# Complie the code using the command 
gcc i2c_atlas_sensor_data.c -o i2c_atlas_sensor_data -lbcm2835<br>

# Execute command
sudo ./i2c_atlas_sensor_data<br> 
