# FltCast

This a C++ flight tracker that reads SBS data from from a sockey, slects the nearest plane, and writes it to led matrix. This data is read directly from socket 30003. Paired with the rtl-sdr. And a ADSB Decoder. 

A ADS-B flight tracker and display build in C++ to be ran on Raspberry Pi and HUB75 matrices.

FltCast recieves aircraft data from SBS streams from any adsb decoder, and renders nearby aircraft to a LED matrix

Architecture


Remove pi's audio kernel mod - can't run matrix library without it 
```
lsmod | grep snd_bcm2835
sudo modprobe -r snd_bcm2835
```

build commands
```
cmake -S . -B build

cmake --build build
```

install dependencies
```
sudo apt install git cmake rtl-sdr
```

dump1090 install
```
wget https://www.flightaware.com/adsb/piaware/files/packages/pool/piaware/f/flightaware-apt-repository/flightaware-apt-repository_1.3_all.deb
sudo dpkg -i flightaware-apt-repository_1.3_all.deb
sudo apt update
sudo apt install dump1090-fa

# Check status
sudo systemctl restart dump1090-fa
sudo systemctl status dump1090-fa
```