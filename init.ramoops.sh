#!/system/bin/sh

# Decrypt the keys and write them to the kernel
ramoops -D

if [ $? -eq 0 ]; then
    # Pivot (and decrypt) and remount pstore
    echo 1 > /sys/devices/virtual/ramoops/pstore/use_alt
    setprop sys.ramoops.decrypted true
else
    setprop sys.ramoops.decrypted Error-$?
fi

# Generate keys (if none exist), and load the keys to carveout
if [[ $(getprop ro.hardware) == "walleye" ]]; then
    ramoops -g -l -c
else
    ramoops -g -l
fi

