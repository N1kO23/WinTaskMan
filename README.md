# WinTaskMan

A Work-In-Progress program to bring the old-fashioned task manager to linux using Qt6 framework

![image](https://github.com/user-attachments/assets/26f1f2d2-aaf1-4653-84d0-976bb0cb169b)

#### Please Note
If you want to have the Aero theme as seen in the screenshot, take a look into [wackyideas/aerothemeplasma](https://gitgud.io/wackyideas/aerothemeplasma)

## Building
To build the program you need cmake, qt6 base and qt6charts. These should be available on all rolling release distros as well as on the latest Ubuntu. To build the program, simply run the `build.sh` script to streamline the process and compiled binary will be found from `build/` directory

To install the dependencies on Arch:
```
sudo pacman -S cmake qt6-base qt6-charts
```

To install the dependencies on Ubuntu:
```
sudo apt install cmake qt6-base-dev libqt6charts6-dev
```

### What works
- Processes being listed
- User services being listed
- Per process CPU usage
- Total CPU usage
- Total process count

### What is missing
- Network tab contents as a whole
- Performance tab contents mostly missing, only 1 wip graph currently
- User tab contents as whole
- Control buttons from all tabs
- Menubar actions

### Known bugs
- Ram usage is all over the place
- Styling can be a bit wacky
