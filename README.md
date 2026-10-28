# FltCast

FltCast is a C++ ADS-B flight tracker built for Raspberry Pi and HUB75 RGB LED matrices.

It reads SBS aircraft data from port `30003`, enriches it with a local DB lookup, and renders it to a 128x64 display made of four 64x32 panels. A radar view maps the planes within range and lists the five nearest; for 10 seconds out of every 30, it switches to a detail screen for the nearest plane.

FltCast is designed to work with an RTL-SDR and any ADS-B decoder that provides an SBS/BaseStation-compatible stream, such as `dump1090-fa`.

<p align="center">
  <img src="assets/fltcast-v2-radar.webp" alt="FltCast radar view with the five nearest planes" width="45%" />
  &nbsp;&nbsp;&nbsp;&nbsp;
  <img src="assets/fltcast-v2-dossier.webp" alt="FltCast detail screen for the nearest plane" width="45%" />
</p>

## Parts List

*As an Amazon Associate, I earn from qualifying purchases.*

The Amazon links are there for convenience, but you may be able to find these parts cheaper elsewhere.

- [RTL-SDR](https://amzn.to/47ngHxq)
- [64x32 P2.5 LED Matrix](https://link.amazon/B09zRx6zl) (x4)
- [RGB Matrix Adapter Board](https://amzn.to/4hwtSAM)
- [5V 15A Power Supply](https://amzn.to/3TtoiXR)
- [USB-C Pigtail Cables](https://amzn.to/4iXFlMl)
- Raspberry Pi 4: Pi prices right now are outrageous, if you can get your hands on a Pi 4 with 1GB RAM that's plenty, but here's the link for a [reputable reseller that I used](https://link.amazon/B0gUnzBAM)
- [WAGO 221-413 Lever Connectors](https://amzn.to/4z2amDs)
- [5.5x2.1mm Panel-Mount DC Power Jack](https://link.amazon/B0cx1QTyA)
- [SMA Bulkhead Antenna Extension Cable](https://link.amazon/B0bvERtUQ)
- M3 and M2.5 heat-set threaded inserts and bolts
- [Case STLs](case)


## Setup

### Disable the Raspberry Pi Audio Kernel Module

The RGB matrix library may conflict with the `snd_bcm2835` audio kernel module.

```bash
sudo modprobe -r snd_bcm2835
```

### Install Dependencies

```bash
sudo apt install git cmake rtl-sdr
```

### Install dump1090-fa

```bash
wget https://www.flightaware.com/adsb/piaware/files/packages/pool/piaware/f/flightaware-apt-repository/flightaware-apt-repository_1.3_all.deb

sudo dpkg -i flightaware-apt-repository_1.3_all.deb
sudo apt update
sudo apt install dump1090-fa
```

Restart and verify the service:

```bash
sudo systemctl restart dump1090-fa
sudo systemctl status dump1090-fa
```

Reboot after installation:

```bash
sudo reboot
```
## Build and Run

```bash
cmake -S . -B build
cmake --build build
```

Run:
```bash
sudo ./build/flight_cast
```

## Start Service

Write the systemd service file. Update with the location of the repository.
```
[Unit]
Description=FltCast

[Service]
Type=simple
WorkingDirectory=/path/to/fltcast
ExecStart=/path/to/fltcast/build/flight_cast
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
```

Start the service
```
sudo systemctl daemon-reload
sudo systemctl enable --now fltcast
```

## Configuration

FltCast reads its runtime settings from an INI configuration file. In the repo root as "config.ini". The following are the current config options:

### Network

`host` and `port` define the SBS/BaseStation stream used by FltCast.

For a local `dump1090-fa` installation:

```ini
[network]
host = localhost
port = 30003
```

### Location

Set `lat` and `lon` to the location you're running from. This is used to calculate the planes distance from you.

```ini
[location]
lat = 40.7128
lon = -74.0060
```

### Fonts

(You shouldn't need to mess with these.)

```ini
[font]
sml = tom-thumb
med = 4x6
lrg = 5x8
```

### Matrix Layout

`cols` and `rows` are the size of one panel. `chain_length` is how many panels are chained, and `pixel_mapper` arranges them; `U-mapper` turns a chain of four into a 128x64 display. `limit_refresh` (Hz) and `gpio_slowdown` are passed to the RGB matrix library and may need tuning for your Pi model.

```ini
[matrix]
cols = 64
rows = 32
chain_length = 4
parallel = 1
pixel_mapper = U-mapper
limit_refresh = 300
gpio_slowdown = 5
padding = 2
row_gap = 0
img_span = 2
img_height = 14
```

### Radar

`range` is the radar radius in miles. Only planes within this distance are shown.

```ini
[radar]
range = 25
```

## AI Usage

V1 was written entirely by hand as a project to improve my system design and software architecture skills. V2 is mostly a UI update on the code side, so I used AI assistance to finish it without getting bogged down in tedious work.

So if the code is bad, that’s ~mostly~ all me :)