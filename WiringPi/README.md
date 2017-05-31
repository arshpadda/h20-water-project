# h20-water-project

## Instruction to install WiringPi.

Install the WiringPi Library
First check that wiringPi is not already installed.
In a terminal, run:
```
gpio -v
```
If you get something, then you have it already installed. 
The next step is to work out if it’s installed via a standard package or from source. 
If you installed it from source, then you know what you’re doing – carry on – but if it’s installed as a package, you will need to remove the package first.
To do this:
```
sudo apt-get purge wiringpi
hash -r
```
Then carry on.
If you do not have GIT installed, then under any of the Debian releases (e.g. Raspbian), you can install it with:
```
sudo apt-get install git-core
```
If you get any errors here, make sure your Pi is up to date with the latest versions of Raspbian: (this is a good idea to do regularly, anyway)
```
sudo apt-get update
sudo apt-get upgrade
```
To obtain WiringPi using GIT:
```
cd
git clone git://git.drogon.net/wiringPi
```
If you have already used the clone operation for the first time, then
```
cd ~/wiringPi
git pull origin
```
Will fetch an updated version then you can re-run the build script below.
To build/install there is a new simplified script:
```
cd ~/wiringPi
./build
```
The new build script will compile and install it all for you – it does use the sudo command at one point, so you may wish to inspect the script before running it.

## Instruction to install DropBox Uploader.

First, clone the repository using git (recommended):
```bash
git clone https://github.com/andreafabrizi/Dropbox-Uploader.git
```
or download the script manually using this command:
```bash
curl "https://raw.githubusercontent.com/andreafabrizi/Dropbox-Uploader/master/dropbox_uploader.sh" -o dropbox_uploader.sh
```
Then give the execution permission to the script and run it:
```bash
 $chmod +x dropbox_uploader.sh
 $./dropbox_uploader.sh
```
The first time you run `dropbox_uploader`, you'll be guided through a wizard in order to configure access to your Dropbox. This configuration will be stored in `~/.dropbox_uploader`.

## Usage
The syntax is quite simple:
```
./dropbox_uploader.sh [PARAMETERS] COMMAND...
[%%]: Optional param
<%%>: Required param
```
**Available commands:**
* **upload** &lt;LOCAL_FILE/DIR ...&gt; &lt;REMOTE_FILE/DIR&gt;  
Upload a local file or directory to a remote Dropbox folder.  
If the file is bigger than 150Mb the file is uploaded using small chunks (default 50Mb); 
in this case a . (dot) is printed for every chunk successfully uploaded and a * (star) if an error 
occurs (the upload is retried for a maximum of three times).
Only if the file is smaller than 150Mb, the standard upload API is used, and if the -p option is specified
the default curl progress bar is displayed during the upload process.  
The local file/dir parameter supports wildcards expansion.

* **download** &lt;REMOTE_FILE/DIR&gt; [LOCAL_FILE/DIR]  
Download file or directory from Dropbox to a local folder

* **delete** &lt;REMOTE_FILE/DIR&gt;  
Remove a remote file or directory from Dropbox

* **move** &lt;REMOTE_FILE/DIR&gt; &lt;REMOTE_FILE/DIR&gt;  
Move or rename a remote file or directory

* **copy** &lt;REMOTE_FILE/DIR&gt; &lt;REMOTE_FILE/DIR&gt;  
Copy a remote file or directory

* **mkdir** &lt;REMOTE_DIR&gt;  
Create a remote directory on Dropbox

* **list** [REMOTE_DIR]  
List the contents of the remote Dropbox folder

* **monitor** [REMOTE_DIR] [TIMEOUT]  
Monitor the remote Dropbox folder for changes. If timeout is specified, at the first change event the function will return.

* **share** &lt;REMOTE_FILE&gt;  
Get a public share link for the specified file or directory

* **saveurl** &lt;URL&gt; &lt;REMOTE_DIR&gt;  
Download a file from an URL to a Dropbox folder directly (the file is NOT downloaded locally)

* **search** &lt;QUERY&gt;
Search for a specific pattern on Dropbox and returns the list of matching files or directories

* **info**  
Print some info about your Dropbox account

* **space**
Print some info about the space usage on your Dropbox account

* **unlink**  
Unlink the script from your Dropbox account

**Optional parameters:**  
* **-f &lt;FILENAME&gt;**  
Load the configuration file from a specific file

* **-s**  
Skip already existing files when download/upload. Default: Overwrite

* **-d**  
Enable DEBUG mode

* **-q**  
Quiet mode. Don't show progress meter or messages

* **-h**  
Show file sizes in human readable format

* **-p**  
Show cURL progress meter

* **-k**  
Doesn't check for SSL certificates (insecure)

**Examples:**
```bash
    ./dropbox_uploader.sh upload /etc/passwd /myfiles/passwd.old
    ./dropbox_uploader.sh upload *.zip /
    ./dropbox_uploader.sh download /backup.zip
    ./dropbox_uploader.sh delete /backup.zip
    ./dropbox_uploader.sh mkdir /myDir/
    ./dropbox_uploader.sh upload "My File.txt" "My File 2.txt"
    ./dropbox_uploader.sh share "My File.txt"
    ./dropbox_uploader.sh list
```
## Complie the code using the command 
```
gcc i2c_atlas_sensor_data.c -o i2c_atlas_sensor_data -lwiringPi -lpthread
```
## Execute command
```
sudo ./i2c_atlas_sensor_data 
```
