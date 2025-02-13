# WinTaskMan

A Work-In-Progress program to bring the old-fashioned task manager to linux using Qt6 framework

![image](https://github.com/user-attachments/assets/998115a1-42e6-40ae-bb10-48b6048ceb00)


## Building
To build the program you need qt6 base. This should be available on all rolling release distros as well as on the latest Ubuntu. To build the program, simply run the `build.sh` script to streamline the process and compiled binary will be found from `build/` directory

### What works
- Processes being listed
- User services being listed
- Per process CPU usage
- Total CPU usage
- Total process count

### What is missing
- Network tab contents as a whole
- Performance tab contents as a whole
- User tab contents as whole
- Control buttons from all tabs
- Menubar actions

### Known bugs
- Ram usage is all over the place
- Selections either do not persist or cannot be unselected
- Styling can be a bit wacky
