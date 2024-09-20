# Dect nr testing
Repo consisting of different tests for nrf9161 DK. To run them on a new device you first need to flash modem firmware onto nrf9161 by using nrfConnect programmer. Read this on how to setup everything: https://devzone.nordicsemi.com/nordic/nordic-blog/b/blog/posts/getting-started-with-nr-phy 

## Tests:
- simple connectivity checker (need 2+ nrf91x1 dk)
    - one device is in transmitting mode, at least one in recieving
        - on boot device is in tx mode for 10 seconds (in tx led0 is off)
        - if the transimittion is started, the device will always stay in tx mode
        - after 10 seconds, the device will go to rx mode (led0 is on)
    - when a button is pressed on tx device, 2000 messages will be sent to recieving devices (led0 is blinking)
    - when tx is ended, rx devices will report back number of succesful ping (and log it to their own console)