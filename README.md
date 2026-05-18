# os-usb-viewer

An SDL GUI for viewing connected USB devices.

## Compile

1. Install dependencies: `sudo apt-get install libusb-1.0-0-dev libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev`
2. Open this directory in a terminal.
3. Run `make`.

## Run

After compiling, open this directory in a terminal, then run `sudo ./bin/usbviewer`. (If you'd rather not give the application full privileges, set udev rules for the devices you want available to access, then run without sudo.)

## Dependencies

- [libusb](https://github.com/libusb/libusb) (USB library)
- [SDL](https://github.com/libsdl-org/SDL) (GUI)
- [Inter](https://github.com/rsms/inter) (font)
- Based on [@tmarrinan's](https://github.com/tmarrinan) file explorer GUI
