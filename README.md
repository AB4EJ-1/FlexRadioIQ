# FlexRadioIQ
Demonstrates how to read and analyze IQ data from FlexRadio 6000 series
using a Linux computer connected to the radio through a switch.

How to use it:
1. Install the fft fast fourier transform package on your system:
 ```
 sudo apt-get update
 sudo apt-get install fftw-dev
 ```
2. Clone this repository to a convenient directory on your system.
3. Navigate to the directory when you have the source and use:  make
This should compile the program. Note that the code looks for the
FlexRadio to output UDP IQ data on port 7791. If you are using this
port for something else, you can select whatever port you like
(but it generally should be a port# greater than 1000). If you do
that, change the defined value UDPPORT_IN in the code and recompile.
4. Ensure the FlexRadio is on the same network as the computer.
5. Run the FlexRadio SmartSDR application and select a Panadaper;
Using the DAX dropdown on the left, select an open DAXIQ Channel.
Let's say you select 1.
6. On your computer, start a nc terminal to talk to the FlexRadio.
FlexRadio uses TCP/IP port 4992 for commands. You can get the IP
address of your FlexRadio under Settings->Radio Setup->Network.
For this application, it works best to configure the FlexRadio to
use a static IP address. Example settings might be: IP Address = 
192.168.1.68  Mask = 255.255.255.0  Gateway 192.168.1.254 . Now
on the computer you would use the following to start controlling
the radio:
```
nc 192.168.1.66 4992
```
When you have connected, the FlexRadio will
send your terminal a response showing many of its current settings.

7. Now that you have connected to the FlexRadio, you can start to
command it. Bear in mind that the FlexRadio expects your commands to
begin with "C" and to each have an increasing serial number. Send the
command:     
```
c0|client udpport 7791        
c1|stream create daxiq=1 port=7791
```

These commands reserve the port and tell the radio to start output
of DAXIQ data to that UDP port.
Leave the nc session running in its own window; you will need it
later to shut down the DAXIQ output. After you did the create stream,
the FlexRadio should have displayed the stream number in an 8-digit
hexadecimal number such as 0x20000001.

8. This should start a stream of DAXIQ data coming to port 7791. These
buffers comply with the VITA-49 SDR data standard. You shoujld be
able to see the data streaming by using: sudo tcpdump -i eth0 udp -XX port 7791
(this assumes the interface on your computer use "eth0" - if yuo
don't know the interface name, you can find it using thre ifconfig command).
If you now see buffers coming out on this port, you are ready to
start processing them using the program. Run the compiled program;
it should display the FFT bin# with the strongest signal magnitude, and
this magnitude will ne shown also. Remember that DAXIQ data is relative
to the center frequency of the panadapter itself (not to the frequency
displayed on the subwindow used to set the radio's frequency).

9. You may shut down the DAXIQ output using your nc session:
```
c3|stream remove 0x20000001
```
This uses the stream number the FlexRadio gave you in Step 7, above. (Note, if you don't shut down
DAXIQ in the same session as you created it, the FlexRadio will not
let you stop it in another session).

For more information about the flexRadio command set, see:
http://wiki.flexradio.com/index.php?title=SmartSDR_TCP/IP_API



