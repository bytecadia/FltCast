# FltCast

Remove pi's audio kernel mod - can't run matrix library without it 
```
lsmod | grep snd_bcm2835
sudo modprobe -r snd_bcm2835
```