# h20-water-project


# Instruction to run the code.

Install the WiringPi Library
First check that wiringPi is not already installed.
In a terminal, run:
```c
gpio -v
```
If you get something, then you have it already installed. 
The next step is to work out if it’s installed via a standard package or from source. 
If you installed it from source, then you know what you’re doing – carry on – but if it’s installed as a package, you will need to remove the package first.
To do this:
```c
sudo apt-get purge wiringpi
hash -r
```
Then carry on.
If you do not have GIT installed, then under any of the Debian releases (e.g. Raspbian), you can install it with:
```c
sudo apt-get install git-core
```
If you get any errors here, make sure your Pi is up to date with the latest versions of Raspbian: (this is a good idea to do regularly, anyway)
```c
sudo apt-get update
sudo apt-get upgrade
```

To obtain WiringPi using GIT:
```c
cd
git clone git://git.drogon.net/wiringPi
```

If you have already used the clone operation for the first time, then
```c
cd ~/wiringPi
git pull origin
```

Will fetch an updated version then you can re-run the build script below.
To build/install there is a new simplified script:
```c
cd ~/wiringPi
./build
```
The new build script will compile and install it all for you – it does use the sudo command at one point, so you may wish to inspect the script before running it.

# Complie the code using the command 
```c
gcc i2c_atlas_sensor_data.c -o i2c_atlas_sensor_data -lwiringPi -lpthread
```
# Execute command
```c
sudo ./i2c_atlas_sensor_data 
