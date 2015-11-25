Hackaday SuperCon Badge
=======================

"Most Over the Top" submission for the Hackaday Superconference Badge Hacking Contest

## Image Generation ##

Gimp to create 565 encoded bitmaps and then dd to strip the header:

```
dd if=input.bmp of=output.raw bs=1 skip=70
```