# FltCast

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