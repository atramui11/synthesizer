#include "tsc.h"
#include <lib/debug.h>
#include <lib/x86.h>

//these addresses refer to ports of sb16 sound card, need to double check
#define DSP_RESET 0x226

//PORT ADDRESSES
#define BASE_PORT 0x388 //sb16 and adlib
#define RESET_PORT (BASE_PORT + 0x06)

#define AD_BASE 0x388
#define AD_REG_TIME1 0x02
#define AD_REG_TIME2 0x03
#define AD_REG_TIME_FLAG 0x04
#define AD_PRIM_ADDR (AD_BASE)
#define AD_PRIM_DATA (AD_BASE+0x01)
#define AD_SCND_ADDR (AD_BASE+0x02)
#define AD_SCND_DATA (AD_BASE+0x03)

#define AD_FNUM_LOW8 (AD_BASE+0xA0)
#define AD_FNUM_HI2  (AD_BASE+0xB0)

void ad_init()
{
	//tests if adlib sound card is working
	//get access to timer enable register
	outb(AD_PRIM_ADDR, AD_REG_TIME_FLAG);

	//set timer 1 and timer 2
	outb(AD_PRIM_DATA, 0x60);

	//set IRQ
	outb(AD_PRIM_DATA,0x80);
	uint8_t status1 = inb(AD_BASE);
	KERN_DEBUG("Status register one val %#x\n", status1);

	//get access to timer 1 register
	outb(AD_PRIM_ADDR,AD_REG_TIME1);

	//set timer1 to 0xff
	outb(AD_PRIM_DATA, 0xff);

	outb(AD_PRIM_ADDR, AD_REG_TIME_FLAG);

	//unmask and start timer 1
	outb(AD_PRIM_DATA, 0x21);
	delay(1000);
	uint8_t status2 = inb(AD_BASE);
	KERN_DEBUG("Status register two value %#x\n",status2);

	//reset timers
	outb(AD_PRIM_DATA,0x60);
	outb(AD_PRIM_DATA,0x80);
	KERN_DEBUG("End of attempt init\n");

}



// uint8_t data inb(int port);
// void outb(int port, uint8_t data);

void write_reg(uint8_t reg, uint8_t data)
{
	outb(AD_PRIM_ADDR, reg);
	outb(AD_PRIM_DATA, data);
}

void play_note(char note, int sharp, char oct)
{
	//map char to hexcode for notes in octave 4

/*
	 16B          277.2       C#
	 181          293.7       D
	 198          311.1       D#
	 1B0          329.6       E
	 1CA          349.2       F
	 1E5          370.0       F#
	 202          392.0       G
	 220          415.3       G#
	 241          440.0       A
	 263          466.2       A#
	 287          493.9       B
	 2AE          523.3       C
*/

	uint8_t hex_code;
	uint8_t oct_freq_msb;
	//char oct=0x04; //default 4

	char MSB=0x01;

	if (note=='C')
	{
		if (sharp) {hex_code=0x6B;}
		else //C
		{MSB=0x02;hex_code=0xAE;}
	}

	if (note=='D')
	{
		if (sharp) {hex_code=0x98;}
		else {hex_code=0x81;}
	}

	if (note=='E') {hex_code=0xB0;}

	if (note=='F')
	{
		if (sharp) {hex_code=0xE5;}
		else {hex_code=0xCA;}
	}

	if (note=='G')
	{
		MSB=0x02;
		if (sharp) {hex_code=0x20;}
		else {hex_code=0x02;}
	}

	if (note=='A')
	{
		MSB=0x02;
		if (sharp) {hex_code=0x63;}
		else {hex_code=0x41;}
	}

	if (note=='B')
	{
		MSB=0x02;
		hex_code=0x87;
	}


	//dprintf("hex code before playing is %#x \n",hex_code);

	/*
	write_reg(0x20, 0x01);
	write_reg(0x40, 0x10);
	write_reg(0x60, 0xF0);
	write_reg(0x80, 0x77);
	*/

	write_reg(0xA0, hex_code); //set note here

	/*
	write_reg(0x23, 0x01);
	write_reg(0x43, 0x00);
	write_reg(0x63, 0xF0);
	write_reg(0x83, 0x77);
	//write_reg(0xB0, (0x30 | MSB)); //send 1/2,
	 */

	write_reg(0xB0, (0x20 | MSB | (oct<<2))); //speaker on

	//end current note
	delay(1000);
	write_reg(0xB0,0x0); //turns off reg

}

void sarias_song()
{
	//Ocarina of Time Saria's song
	//F4 A4 B4 F4 A4 B4
	//F4 A4 B4 E5 D5 B4 C4
	//B4 G4 E4 hold , D4
	//E4 G4 E4

	play_note('F',0,4); play_note('A',0,4); play_note('B',0,4);
	play_note('F',0,4); play_note('A',0,4); play_note('B',0,4);

	play_note('F',0,4); play_note('A',0,4); play_note('B',0,4); play_note('E',0,5);
	play_note('D',0,5); play_note('B',0,4); play_note('C',0,4);

	play_note('B',0,4); play_note('G',0,4); play_note('E',0,4); delay(1500); /*slur*/ play_note('D',0,4);

	play_note('E',0,4); play_note('G',0,4); play_note('E',0,4);
	delay(1000);
}

void ddd()
{
	//OoS DDD
	play_note('D', 0,4); play_note('D', 0,4); play_note('F', 0,4);
	play_note('D', 0,4); play_note('A', 0,4); play_note('C', 0,4); play_note('B', 0,4);
	delay(500);

	play_note('B', 0,4); play_note('B', 0,4); play_note('C', 0,4); play_note('B', 0,4);
	play_note('A', 0,4); play_note('G', 0,4); play_note('A', 0,4);
	delay(500);
}

void print_controls()
{
	dprintf("\n\t\t\t\tControls:\n\t\t\t\tw -> C#, s -> D, e -> D#\n\t\t\t\td -> E, f -> F, y -> F#\n\t\t\t\th -> G, u -> G# j -> A\n\t\t\t\ti -> A# k -> B, l -> c\n\n");
	dprintf("\t\t\t\tTry C# Scale key sequence: w e f y u i l p\n\n");
}

//play_simple_sound is called in the system call
void play_simple_sound()
{
	//do i need lock?? maybe more efficient


	write_reg(0x20, 0x01); //set the modulator's mutiple to 1
	write_reg(0x40, 0x10); //set modulators level to about 40dB




	//next to do : see what THESE do
	write_reg(0x60, 0xF0); //modulator attack
	write_reg(0x80, 0x77); //modulator sustain



	//middle cmd for note in other fx
	write_reg(0x23, 0x01); //Set carrier's multiple to 1
	write_reg(0x43, 0x00); //set carrier to max vol (47dB)





	//next to do : see what THESE do
	write_reg(0x63, 0xF0); //carrier attack
	write_reg(0x83, 0x77); //carrier sustain





	dprintf("\t\t\t\t----------------------------------------------\n");
	dprintf("\t\t\t\t\tSPACE to play sample song!\n\n");
	dprintf("\t\t\t\t\t+/- to change octave\n\n");
	dprintf("\t\t\t\t\tc to print controls, ESC to quit\n");
	dprintf("\t\t\t\t----------------------------------------------\n\n");

	dprintf("\t\t\t\t\t\tPIANO KEY LAYOUT\n");
	dprintf("\t\t\t\t______________________________________________\n");
	dprintf("\t\t\t\t|  |  |  |    |    |  |  |  |  |  |   |   |  |\n");
	dprintf("\t\t\t\t|C#|  |D#|    |    |F#|  |G#|  |A#|   |   |C#|\n");
	dprintf("\t\t\t\t|_w|  |_e|    |    |_y|  |_u|  |_i|   |   |_p|\n");
	dprintf("\t\t\t\t| |  s  |  d  |  f  |  h  |  j  |  k  |  l   |\n");
	dprintf("\t\t\t\t| |  D  |  E  |  F  |  G  |  A  |  B  |  C   |\n");
	dprintf("\t\t\t\t|_|_____|_____|_____|_____|_____|_____|______|\n\n\n\n\n\n\n\n\n\n\n\n");


//input loop
	uint8_t oct=4;
	uint8_t ctrlsFlag=1;

	while (1)
	{
		char user_input = cons_getc();

		if (user_input)
		{
			if (user_input==27) break;

			//user pressed something
			//dprintf("user pressed %c\n", user_input);
			if (user_input=='l'){play_note('C',0,oct);}
			else if (user_input=='w'){play_note('C',1,oct);}
			else if (user_input=='s')	 {play_note('D',0,oct);}
			else if (user_input=='e')	 {play_note('D',1,oct);}
			else if (user_input=='d'){play_note('E',1,oct);}
			else if (user_input=='f'){play_note('F',0,oct);}
			else if (user_input=='y'){play_note('F',1,oct);}
			else if (user_input=='h'){play_note('G',0,oct);}
			else if (user_input=='u'){play_note('G',1,oct);}
			else if (user_input=='j'){play_note('A',0,oct);}
			else if (user_input=='i'){play_note('A',1,oct);}
			else if (user_input=='k'){play_note('B',0,oct);}
			else if (user_input=='p'){play_note('C',1,oct+1);} //high C#
			else if (user_input=='c'){
				if (ctrlsFlag) {print_controls(); ctrlsFlag=0;}
			}

			//octave changing code
			else if (user_input=='-'){if (oct>2) oct--;}
			else if (user_input=='+'){if (oct<6) oct++;}

			else if (user_input==' '){
				dprintf("\t\t\t\tPlaying Zelda OoT Saria's song...");
				delay(1000);
				sarias_song();
				dprintf("END SAMPLE SONG!\n");
			}


			else {continue;}

		}

		delay(100);//too much looping

    }

	//break puts here
	dprintf("\tQuitting...\n\n");
	//ddd();

	//unlock??
	return;
}




