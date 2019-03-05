wget -O /dev/null http://10.42.0.1:8000/1GB.file
sudo ifconfig wlo1 mtu 2304
python -m SimpleHTTPServer
dd if=/dev/zero of=1GB.file bs=MB count=1000
