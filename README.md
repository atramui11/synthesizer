"# synthesizer"

Compile: make / make all
Run tests: make clean && make TEST=1
Run in qemu: make qemu / make qemu-nox



This is a synthesizer application controlled by user input to the computer keyboard. 

Details:

USER LEVEL:
	For a more interactive/effective UI, fingers are positioned as similar as possible to a real keyboard in my controls layout.
	Plays like a piano, but you can position your hands like you're typing something. ('sdf hjkl', thumbs on SPACE)
	The keyboard is musically responsive and plays scales/songs. Try the C# scale (w e f y u i l p)! 
	
	The piano is capable of playing five octaves of frequencies.
	Octave can be changed with +/- key (+ to increase octave, - to decrease)
	As it is a synthesizer-based soundcard, it has a wide range of frequencies (low to high).
	
	SPACE plays a sample song. 
	The user only needs to input characters. The ASCII art in the application illustrates the keyboard layout with corresponding controls.

KERNEL LEVEL:
	Each sound played requires I/O communication to the soundcard/soundcard registers using 8 bit hexcodes. 
	This communication is accomplished with the kernel function outb(port, data). 
	
	In order to build audio support for QEMU, it was first necessary to enable the adlib soundcard in the QEMU main Makefile.
	Then, it was necessary to learn the hexcode communication specifications of the adlib soundcard.
	Once I/O hexcode communication details were resolved, it was possible to write code that plays specific notes by triggering different hexcode sends depending on the note.
	The hexcode sends allow for change of pitch and octave depending on the key the user input. 

In summary, I implemented audio support for the OS, and used this implementation to build a piano application with UI. 

4. There may sometimes be static-y sound issues with the soundcard. 
	This was probably just an problem with my own machine, which has speaker issues.  
	If this happens, you can briefly stop user input, wait, try again. If there is still static, restart QEMU or make clean+make again.

	CONTROLS:
	Press +/- to change octave:
		+ to increase octave
		- to decrease octave 
	Press SPACE for a sample song! 
	Press c to print piano controls. (w -> C#, s -> D, e -> D#, d -> E, f -> F, y -> F#, h -> G, u -> G# j -> A, i -> A# k -> B, l -> c)
		These controls will only print once in the duration of the program. 		
	Press ESC to quit.
		
		
