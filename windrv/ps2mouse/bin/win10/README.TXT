PS/2 mouse pointer driver for Windows 1.0.

==== Windows 1.0 install process ====

Note that Windows 1.0 does not provide any selection
screen on setup to install 3rd party drivers. The following
instructions are a hack that requires modification of the
install disks.

To install:

1. Locate the Windows 1.0 install disk images for your
   emulator. Keep a backup of the original images,
   because these steps require modification of the
   disks.

2. Locate disk 1. Overwrite MOUSE.DRV on the install
   disk with P2MOUSE.DRV.

3. Run the setup program. Answer all questions. When
   the program gets to the mouse selection screen,
   select "Microsoft bus or serial mouse". This
   tells setup to use MOUSE.DRV, which we just overwrote
   with the PS/2 driver.

4. Run through the rest of the setup process.

5. When you run Windows 1.x, the mouse cursor should
   interact with the PS/2 mouse.

