extern void keyboard_handler(void);
extern void load_idt(unsigned long *idt_ptr);
extern void timer_handler();
extern void asm_toMain();

unsigned char keyboard[128] ={
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8',	/* 9 */
  '9', '0', '-', '=', '\b',	/* Backspace */
  '\t',			/* Tab */
  'q', 'w', 'e', 'r',	/* 19 */
  't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',	/* Enter key */
    0,			/* 29   - Control */
  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',	/* 39 */
 '\'', '~',   0,		/* Left shift */
 '\\', 'z', 'x', 'c', 'v', 'b', 'n',			/* 49 */
  'm', ',', '.', '/',   0,				/* Right shift */
  '*',
    0,	/* Alt */
  ' ',	/* Space bar */
    0,	/* Caps lock */
    0,	/* 59 - F1 key ... > */
    0,   0,   0,   0,   0,   0,   0,   0,
    0,	/* < ... F10 */
    0,	/* 69 - Num lock*/
    0,	/* Scroll Lock */
    0,	/* Home key */
    0,	/* Up Arrow */
    0,	/* Page Up */
  '-',
    0,	/* Left Arrow */
    0,
    0,	/* Right Arrow */
  '+',
    0,	/* 79 - End key*/
    0,	/* Down Arrow */
    0,	/* Page Down */
    0,	/* Insert Key */
    0,	/* Delete Key */
    0,   0,   0,
    0,	/* F11 Key */
    0,	/* F12 Key */
    0,	/* All other keys are undefined */
};

struct IDT_entry{
	unsigned short int offset_lowerbits;
	unsigned short int selector;
	unsigned char zero;
	unsigned char type_attr;
	unsigned short int offset_higherbits;
};

struct marquee{
	char string[80];
	unsigned int row;
	unsigned int col;
};

struct message{
	char* key;
	char* text;
};

struct holder{
	char temp[10000];
	char store[10000];
};

struct process{
	int esp, ebp, eip;
	int stack[1024];
	char screenPtr[2000];
	int initialEip;
	char name[9];
	int cursorY, vidCtr;
};

char* header = "TheOS: ";

struct process processes[10];
struct IDT_entry IDT[256];

struct message messages[24];
struct holder stringHolder[24];
int messageCount = 0;
int holdCount = 0;

char *vidPtr = (char*)0xb8000;
char strSplit[80];
char command[80]; 
char strtemp[80];
char name[9];
unsigned int stringCounter = 0;
unsigned int vidCtr = 0;
unsigned int screensize =  80 * 25 * 2;
unsigned int newlineInt;
unsigned int cursorY = 0;
unsigned int mandyLine = 160*3;
unsigned long idt_ptr[2];
int atMain = 1;
int textAttrib = 0x07;
int currentfunction = 0;

struct marquee marquees[14];
unsigned int marqueeCount = 0;
int timerTicks = 0;

void appendChar(char *String, char charInput){
	int i = 0;
	for(i = 0; String[i] != '\0'; i++);
	String[i] = charInput;
	String[i+1] = '\0';
}

void outb(unsigned short port, unsigned char value){
	asm volatile("outb %1, %0": :"Nd"(port),"a"(value));
}

unsigned char inb(unsigned short _port){
	unsigned char rv;
	asm volatile("inb %1, %0":"=a"(rv):"dN"(_port));
	return rv;
}

void kb_init(void){
	outb(0x21 , 0xFD);
}

void timer_init(void){
	write_port(0x21, 0xFC);
}

void idt_init(void){
	unsigned long function_address;
	unsigned long idt_address;
	unsigned long timer_address;

	function_address = (unsigned long)keyboard_handler;
	IDT[0x21].offset_lowerbits = function_address & 0xffff;
	IDT[0x21].selector = 0x08;
	IDT[0x21].zero = 0;
	IDT[0x21].type_attr = 0x8e;
	IDT[0x21].offset_higherbits = (function_address & 0xffff0000) >> 16;
	
	timer_address = (unsigned long)timer_handler;
	IDT[0x20].offset_lowerbits =  timer_address & 0xffff;
	IDT[0x20].selector = 0x08;
	IDT[0x20].zero = 0;
	IDT[0x20].type_attr = 0x8e;
	IDT[0x20].offset_higherbits = (timer_address & 0xffff0000) >> 16;

	outb(0x20 , 0x11);
	outb(0xA0 , 0x11);
	outb(0x21 , 0x20);
	outb(0xA1 , 0x28);
	outb(0x21 , 0x00);
	outb(0xA1 , 0x00);
	outb(0x21 , 0x01);
	outb(0xA1 , 0x01);
	outb(0x21 , 0xff);
	outb(0xA1 , 0xff);

	idt_address = (unsigned long)IDT ;
	idt_ptr[0] = (sizeof (struct IDT_entry) * 256) + ((idt_address & 0xffff) << 16);
	idt_ptr[1] = idt_address >> 16 ;

	load_idt(idt_ptr);
}

void clearStr(){	
	command[0] = '\0';
}

void moveCursor(){
	outb(0x3D4, 14);
	outb(0x3D5, vidCtr/2 >> 8);
	outb(0x3D4, 15);
	outb(0x3D5, vidCtr/2);
}

void scroll(){
	int i, j = 0;
	
	for(i = 160; i < screensize; i++){
		if(i%2 == 0){
			vidPtr[j] = vidPtr[i];
			vidPtr[j+1] = 0x07;
			j = j + 2;
		}
	}
	
	j = 160*24;
	
	while(j < screensize){
		vidPtr[j] = ' ';
		vidPtr[j+1] = 0x07; 	
		j = j + 2;		
	}

	for(i = 0; i < marqueeCount; i++)
		marquees[i].row = marquees[i].row - 1;
	

	for(i = 0; i < marqueeCount; i++)
    		marquees[i].row--;
	

	moveCursor();
}

int strCmp(char *str1, char *str2){
	int counter = 0;
	int check = 1;
	
	while(str1[counter] != '\0' && str2[counter] !='\0'){
		if(str1[counter] != str2[counter])
			check = 0;
		
		counter++;
	}
	
	if(str1[counter] != str2[counter])
		return 0;
	
	return check;
}

int strLen(char str[]){
	int count = 0;
	
	while(str[count] != '\0')
		count++;	
	
	return count;
}

void strCpy(char arr1[], char arr2[]){
	int i;
	
	for(i = 0; i < strLen(arr1); i++)
		arr2[i] = arr1[i];
}

void printStr(char *str){
	stringCounter = 0;
	int i;

	while (str[stringCounter] != '\0') {
	
		if(cursorY > 24) {
			for(i = 0; i < 99999; i++);
				scroll();
			
			vidCtr = 160*24;
			cursorY = 24;
		}
		
		if(str[stringCounter] == '\n') {
			vidCtr = 80*(cursorY+1)*2;
			cursorY++;	
			++stringCounter;
			
			if(cursorY > 24) {
				for(i = 0; i < 99999; i++);
				scroll();
				vidCtr = 160*24;
				cursorY = 24;
			}
		}
		
		else {
			vidPtr[vidCtr] = str[stringCounter];
			vidPtr[vidCtr+1] = 0x07;
			++stringCounter;
			vidCtr = vidCtr + 2;
			
			if(vidCtr >= 80*(cursorY+1)*2)
				cursorY++;
		}
	}
	moveCursor();
}

void printChar(char arg1){
	vidPtr[vidCtr] = arg1;
    vidPtr[vidCtr+1] = textAttrib;
    vidCtr = vidCtr + 2;
	moveCursor();
}

void printCharAtXY(char c, int x, int y){
	int loc;
	
	loc = (x * 2) + (y * 160);
	vidPtr[loc] = c;
	vidPtr[loc+1] = 0x07;
}

void clearLine(int y){
	int i, currPos;
	currPos = (160 * y);
	
	for(i = currPos; i < currPos + 160; i++){
		vidPtr[i] = ' ';
		vidPtr[i+1] = 0x07;
		i++;
	}
}

void printMarquee(struct marquee *marquee){
	int i, currX;
	clearLine(marquee->row);
	currX = marquee->col;
	i = 0;
	
	while(marquee->string[i] != '\0'){
		if(currX >= 80)
			currX -= 80;
		
		printCharAtXY(marquee->string[i], currX, marquee->row);
		currX++;
		i++;
	}
	
	if(marquee->col >= 80)
		marquee->col = 0;
	
	marquee->col += 1; 
}

void timer(){
	int i;
	outb(0x20, 0x20);
    timerTicks++;
	
    if((timerTicks % 2) == 0){
    	if(marqueeCount > 0){
    		for(i = 0; i < marqueeCount; i++)
       			printMarquee(&marquees[i]);
       	}
       	else printMarquee(&marquees[0]);
    }
}

void addMarquee(char str[]){	
	int i;
	
	timer_init();
	marquees[marqueeCount].row = cursorY + 1;
	marquees[marqueeCount].col = 0;
	
	while(str[i] != '\0'){
		marquees[marqueeCount].string[i] = str[i];
		i++;
	}
	
	marqueeCount++;
}

void printInt(int num){
	int digit, i = 0;
	
	if(cursorY > 24){
		for(i = 0; i < 99999; i++);
			scroll();
		
		vidCtr = 160*24;
		cursorY = 24;
	}
	
	digit = num;
	i = 1;
	
	while(digit > 9){
		digit = digit / 10;
		i = i * 10;
	}
	
	while(i > 9){
		digit = num / i;
		num = num % i;
		i /= 10;
		
		vidPtr[vidCtr] = digit + 48;
		vidPtr[vidCtr+1] = 0x07;
		vidCtr = vidCtr + 2;
		moveCursor();
		
		if(vidCtr >= 80*(cursorY+1)*2)
			cursorY++;
	}
	
	vidPtr[vidCtr] = num + 48;
	vidPtr[vidCtr+1] = 0x07;
	vidCtr = vidCtr + 2;
	moveCursor();
	
	if(vidCtr >= 80*(cursorY+1)*2)
		cursorY++;
}

void processList(){
	int i;
	for(i = 0; i <= 8; i++){
		if(processes[i].eip != 0){
			printStr("[");
			printInt(i);
			printStr("] ");
			printStr(processes[i].name);
			printStr("\n");
		}
	}
	
	printStr("\n");
	printStr("Input: ");
}

void clearMarquee(){
	int i, j;
	
	for(i = 0; i < marqueeCount; i++){
		for(j = 0; j < strLen(marquees[i].string); j++)
			marquees[i].string[j] = ' ';
		
		marquees[i].col = 0;
		marquees[i].row = 0;
	}
	
	marqueeCount = 0;
}

void clearScreen(){
	int a, b, c=0;
	vidCtr = 0;
	
	while (c < screensize) {
		vidPtr[c] = ' ';
		vidPtr[c+1] = 0x07; 
		c = c + 2;
	}

	clearMarquee();
	marqueeCount = 0;
	vidCtr = 0;
	cursorY = 0;

	printStr(header);
}

void say(){
	int i=4;
	
	printStr(header);	
	while(command[i] != '\0'){
		printChar(command[i]);
		i++;
	}
}

void checkCommand(){
	int counter = 0;
	
	while(command[counter] != ' ' && command[counter] != '\0'){
		strSplit[counter] = command[counter];
		counter++;
	}
	
	strSplit[counter] = '\0';
}

void func1(){
	int i=0, x;
	
	printStr("Starting function 1...\n");
	while(1){
		i++;
		printStr(header);
		printInt(i);
		printStr("\n");
		for(x = 0; x < 100000000; x++);
	}
}

void func1ToMain(){
	func1();
	asm_toMain();
}

void func2(){
	int x = 0, i;
	
	printStr("Starting function 2...\n");
	while(x != 30){
		printStr(header);
		printInt(x);
		printStr("\n");
		x += 2;
		for(i = 0; i < 100000000; i++);
	}
}

void func2ToMain(){
	func2();
	asm_toMain();
}

void func3(){
	int x=0,i;
	
	printStr("Starting function 3\n");
	for(i=0; i<100; i++){
		printStr(header);
		printStr("Sup Sir!\n");
		for(x = 0; x < 100000000; x++);
	}
}

void func3ToMain(){
	func3();
	asm_toMain();
}

void programList(){	
	while(1);
}

void listToMain(){
	programList();
	asm_toMain();
}

void runProgram(){
	int i = 0;
	
	while(i < 9){
		if(strCmp(name, "prg1") && processes[i].eip == 0){
			processes[i].eip = (int)&func1ToMain;
			processes[i].initialEip = (int)&func1ToMain;
			strCpy(name, processes[i].name);
			break;
		}
		
		else if(strCmp(name, "prg2") && processes[i].eip == 0){
			processes[i].eip = (int)&func2ToMain;
			processes[i].initialEip = (int)&func2ToMain;
			strCpy(name, processes[i].name);
			break;
		}

		else if(strCmp(name, "prg3") && processes[i].eip == 0){
			processes[i].eip = (int)&func3ToMain;
			processes[i].initialEip = (int)&func3ToMain;
			strCpy(name, processes[i].name);
			break;	
		}
		
		i++;
	}
}

void putString(char* tempKey, char* tempText){
	int i = 0;
	
	messages[messageCount].key = tempKey;
	messages[messageCount].text = tempText;
	
    messageCount++;
}

char* getString(char* tempKey){
	int i = 0;
	
	for(i = 0; i < messageCount; i++){
		 if(strCmp(messages[i].key, tempKey))
		 	return messages[i].text;
	}
	
    return "Key not found";
}

void popString(char* tempKey){
	int i = 0;
	int j = 0;
	
	for(i = 0; i < messageCount; i++){
		if(strCmp(messages[i].key, tempKey)){
			printStr(messages[i].text);
			printStr(" has been popped!");
			messageCount--;
			
			for(j = i; j < messageCount; j++)
				messages[j] = messages[j+1];
		}
		else
			printStr("Key not found!");
	}
}

int runCommand(){
	int switchTo = -1;
	char getStringCommand[80];
	char getS[80];
	char store[2];
	int i;
	
	checkCommand();
	
	for(i= 0; i<9; i++)
		getStringCommand[i] = command[i+10];
	getStringCommand[i] = '\0';

	for(i= 0; i<9; i++)
		getS[i] = command[i+10];
	getS[i] = '\0';
	
	if(strCmp(command, "cls")==1)
		clearScreen();
	
	else if (strCmp(strSplit, "say") && command[3] == ' '  && atMain){
		printStr("\n");
		say();
		printStr("\n");
		printStr(header);	
	}
	
	else if (strCmp(strSplit, "putstring") && command[9] == ' ' && atMain){
		stringHolder[holdCount].store[0] = command[10];

		for(i = 12; i<strLen(command);i++)
			stringHolder[holdCount].temp[i-12] = command[i];
	
		stringHolder[holdCount].temp[i-12] = '\0';

		putString(stringHolder[holdCount].store,stringHolder[holdCount].temp);
		holdCount++;	
	}
 
	else if(strCmp(strSplit, "getstring") && command[9] == ' ' && atMain){
	 	store[0] = command[10];
		printStr("\n");
		printStr(header);
		printStr(getString(store));
		printStr("\n");
		printStr(header);
	}
	
	else if(strCmp(strSplit, "popstring") && command[9] == ' ' && atMain){
	 	store[0] = command[10];
		printStr("\n");
		printStr(header);
		popString(store);
		printStr("\n");
		printStr(header);
	}
	
	else if (strCmp(strSplit, "marquee") && command[7] == ' '  && atMain){
		addMarquee(command + 8);
		printStr("\n");
		printStr("\n");
		printStr(header);
	}
	
	else if( strCmp(strSplit, "run") && command[3] == ' ' && atMain){
		int i = 4, j = 0;
		
		while(command[i] != '\0'){
			name[j] = command[i];
			i++;
			j++;
		}
		
		name[j] = '\0';
		runProgram(name);
		printStr("\n");
		printStr(header);	
	}
	
	else if(currentfunction == 9){
		if(strCmp(command, "1"))
			switchTo = 1;
		else if(strCmp(command, "2"))
			switchTo = 2;
		else if(strCmp(command, "3"))
			switchTo = 3;
		else if(strCmp(command, "4"))
			switchTo = 4;
		else if(strCmp(command, "5"))
			switchTo = 5;
		else if(strCmp(command, "6"))
			switchTo = 6;
		else if(strCmp(command, "7"))
			switchTo = 7;
		else if(strCmp(command, "8"))
			switchTo = 8;
		else if(strCmp(command, "0"))
			switchTo = 0;
	}
		
	else {
		if(currentfunction == 0 || currentfunction == 9){
			printStr("\n");	
			printStr(header);
		}
	}
			
	clearStr();
	moveCursor();	
	return switchTo;
}

void saveScreen(int index){
	int i=0;
	
	while (i < screensize) {
		processes[index].screenPtr[i/2] = vidPtr[i];
		i = i + 2;
	}
	
	processes[index].cursorY = cursorY;
	processes[index].vidCtr = vidCtr;
}

void switchScreen(int index){
	int i=0;
	clearScreen();
	
	while(i < screensize){
		vidPtr[i*2] = processes[index].screenPtr[i];		
		vidPtr[i*2+1] = 0x07;
		i++;
	}

	vidCtr = processes[index].vidCtr;
	cursorY = processes[index].cursorY;
	moveCursor();
}

void c_toMain(int *ptr){
	int j=0;

	ptr[0] = processes[0].ebp;
	ptr[-1] = processes[0].esp;
	
	processes[currentfunction].eip = 0;
	processes[currentfunction].initialEip = 0;
	processes[currentfunction].ebp = (int)&processes[currentfunction].stack[1024];
	processes[currentfunction].esp = processes[currentfunction].ebp;
	processes[currentfunction].cursorY = 0;
	processes[currentfunction].vidCtr = 0;
	
	while(j < 2000){
		processes[currentfunction].screenPtr[j] = ' ';
		j = j+1;
	}

	atMain = 1;
	currentfunction = 0;
	switchScreen(0);
}

void keyCall(int* ptr){
	unsigned int characterASCII;	
	unsigned int pressed;
	char inputChar;
	int counter = 0;
	int *pointer;	
	int i, isprocessed=0;
	outb(0x20, 0x20);
	pressed = inb(0x64);
	
	if(pressed & 0x01) {
		characterASCII = inb(0x60);
		
		if(characterASCII >= 0 && characterASCII < 128) {
			inputChar = keyboard[characterASCII];
			
			if(inputChar == '~' && processes[9].eip != 0 && (atMain || currentfunction != 9)){
				atMain = 0;
				isprocessed = 1;
				saveScreen(currentfunction);
				
				if(currentfunction != 9) {
					processes[currentfunction].eip = ptr[1];
					processes[currentfunction].esp = (int)&ptr[1];
					processes[currentfunction].ebp = ptr[0];
					
					if(processes[9].eip == processes[9].initialEip){
						processes[9].stack[1021] = processes[9].eip;		
						processes[9].esp -= 4;
						ptr[0] = processes[9].ebp;
						processes[9].stack[1022] = ptr[2];
						processes[9].esp -= 4;
						processes[9].stack[1023] = ptr[3];
						processes[9].esp -= 4;
						ptr[-1] = processes[9].esp;
					}
					
					else {
						ptr[0] = processes[9].ebp;
						ptr[-1] = processes[9].esp;
					}
				}
				
				else
					ptr[-1] = (int)&ptr[1];	
				
				currentfunction = 9;
				switchScreen(currentfunction);	
				clearScreen();
				printStr("Initializing Program Menu:");
				printStr("\n\n");
				processList();	
			}
			
			else if(inputChar == '0' && processes[0].eip != 0 && !atMain && currentfunction < 9){
				isprocessed = 1;
				saveScreen(currentfunction);
				atMain = 1;
				
				if(currentfunction != 0){
					processes[currentfunction].eip = ptr[1];
					processes[currentfunction].esp = (int)&ptr[1];
					processes[currentfunction].ebp = ptr[0];
				
				
					if(processes[0].eip == processes[0].initialEip){
							processes[0].stack[1021] = processes[0].eip;		
							processes[0].esp -= 4;
							ptr[0] = processes[0].ebp;
							processes[0].stack[1022] = ptr[2];
							processes[0].esp -= 4;
							processes[0].stack[1023] = ptr[3];
							processes[0].esp -= 4;
							ptr[-1] = processes[0].esp;				
					}
					
					else {
						ptr[0] = processes[0].ebp;
						ptr[-1] = processes[0].esp;				
					}
				}
				
				else
					ptr[-1] = (int)&ptr[1];
			
				currentfunction = 0;
				switchScreen(currentfunction);			
			}
			
			else if(inputChar == '1' && processes[1].eip != 0 && !atMain && currentfunction < 9) {
				isprocessed = 1;
				saveScreen(currentfunction);
				atMain = 0;
				
				if(currentfunction != 1){
					processes[currentfunction].eip = ptr[1];
					processes[currentfunction].esp = (int)&ptr[1];
					processes[currentfunction].ebp = ptr[0];
						
					if(processes[1].eip == processes[1].initialEip){
							processes[1].stack[1021] = processes[1].eip;		
							processes[1].esp -= 4;
							ptr[0] = processes[1].ebp;
							processes[1].stack[1022] = ptr[2];
							processes[1].esp -= 4;
							processes[1].stack[1023] = ptr[3];
							processes[1].esp -= 4;
							ptr[-1] = processes[1].esp;							
					}
					
					else {
						ptr[0] = processes[1].ebp;
						ptr[-1] = processes[1].esp;		
					}
				}
				
				else
					ptr[-1] = (int)&ptr[1];
					
					currentfunction = 1;
					switchScreen(currentfunction);			
			}
			
			else if(inputChar == '2' && processes[2].eip != 0 && !atMain && currentfunction  < 9){
				isprocessed = 1;
				saveScreen(currentfunction);
				atMain = 0;
				
				if(currentfunction != 2){
					processes[currentfunction].eip = ptr[1];
					processes[currentfunction].esp = (int)&ptr[1];
					processes[currentfunction].ebp = ptr[0];
				
					if(processes[2].eip == processes[2].initialEip)	{
						processes[2].stack[1021] = processes[2].eip;	
						processes[2].esp -= 4;
						ptr[0] = processes[2].ebp;
						processes[2].stack[1022] = ptr[2];
						processes[2].esp -= 4;
						processes[2].stack[1023] = ptr[3];
						processes[2].esp -= 4;						
						ptr[-1] = processes[2].esp;
					}
					
					else{
						ptr[0] = processes[2].ebp;
						ptr[-1] = processes[2].esp;
					}
				}
				
				else
					ptr[-1] = (int)&ptr[1];
			
				currentfunction = 2;
				switchScreen(currentfunction);			
			}
			
			else if(inputChar == '3' && processes[3].eip != 0 && !atMain && currentfunction < 9){
				isprocessed = 1;
				saveScreen(currentfunction);
				atMain = 0;
				
				if(currentfunction != 3){
					processes[currentfunction].eip = ptr[1];
					processes[currentfunction].esp = (int)&ptr[1];
					processes[currentfunction].ebp = ptr[0];
				
					if(processes[3].eip == processes[3].initialEip)	{
						processes[3].stack[1021] = processes[3].eip;
						processes[3].esp -= 4;
						ptr[0] = processes[3].ebp;
						processes[3].stack[1022] = ptr[2];
						processes[3].esp -= 4;
						processes[3].stack[1023] = ptr[3];
						processes[3].esp -= 4;						
						ptr[-1] = processes[3].esp;					
					}
					
					else {
						ptr[0] = processes[3].ebp;
						ptr[-1] = processes[3].esp;
					}
				}
				
				else
					ptr[-1] = (int)&ptr[1];
				
				currentfunction = 3;
				switchScreen(currentfunction);				
			}
			
			else if(inputChar == '4' && processes[4].eip != 0 && !atMain && currentfunction < 9){
				isprocessed = 1;
				saveScreen(currentfunction);
				atMain = 0;
				
				if(currentfunction != 4){
					processes[currentfunction].eip = ptr[1];
					processes[currentfunction].esp = (int)&ptr[1];
					processes[currentfunction].ebp = ptr[0];
				
					if(processes[4].eip == processes[4].initialEip){
						processes[4].stack[1021] = processes[4].eip;
						processes[4].esp -= 4;
						ptr[0] = processes[4].ebp;
						processes[4].stack[1022] = ptr[2];
						processes[4].esp -= 4;
						processes[4].stack[1023] = ptr[3];
						processes[4].esp -= 4;						
						ptr[-1] = processes[4].esp;			
					}
					
					else {
						ptr[0] = processes[4].ebp;
						ptr[-1] = processes[4].esp;
					}
				}
				
				else
					ptr[-1] = (int)&ptr[1];
				
				currentfunction = 4;
				switchScreen(currentfunction);			
			}
			
			else if(inputChar == '5' && processes[5].eip != 0 && !atMain && currentfunction < 9){
				isprocessed = 1;
				saveScreen(currentfunction);
				atMain = 0;
				
				if(currentfunction != 5) {
					processes[currentfunction].eip = ptr[1];
					processes[currentfunction].esp = (int)&ptr[1];
					processes[currentfunction].ebp = ptr[0];
				
					if(processes[5].eip == processes[5].initialEip) {
						processes[5].stack[1021] = processes[5].eip;
						processes[5].esp -= 4;
						ptr[0] = processes[5].ebp;
						processes[5].stack[1022] = ptr[2];
						processes[5].esp -= 4;
						processes[5].stack[1023] = ptr[3];
						processes[5].esp -= 4;						
						ptr[-1] = processes[5].esp;
					}
					
					else {
						ptr[0] = processes[5].ebp;
						ptr[-1] = processes[5].esp;
					}
				}
				
				else
					ptr[-1] = (int)&ptr[1];
				
				currentfunction = 5;
				switchScreen(currentfunction);			
			}
			
			else if(inputChar == '6' && processes[6].eip != 0 && !atMain && currentfunction < 9) {
				isprocessed = 1;
				saveScreen(currentfunction);
				atMain = 0;
				
				if(currentfunction != 6) {
					processes[currentfunction].eip = ptr[1];
					processes[currentfunction].esp = (int)&ptr[1];
					processes[currentfunction].ebp = ptr[0];
				
					if(processes[6].eip == processes[6].initialEip)	 {
						processes[6].stack[1021] = processes[6].eip;	
						processes[6].esp -= 4;
						ptr[0] = processes[6].ebp;
						processes[6].stack[1022] = ptr[2];
						processes[6].esp -= 4;
						processes[6].stack[1023] = ptr[3];
						processes[6].esp -= 4;						
						ptr[-1] = processes[6].esp;				
					}
					
					else {
						ptr[0] = processes[6].ebp;
						ptr[-1] = processes[6].esp;
					}				
				}
				
				else
					ptr[-1] = (int)&ptr[1];
				
				currentfunction = 6;
				switchScreen(currentfunction);	
			}
			
			else if(inputChar == '7' && processes[7].eip != 0 && !atMain && currentfunction < 9) {
				isprocessed = 1;
				saveScreen(currentfunction);
				atMain = 0;
				
				if(currentfunction != 7){
					processes[currentfunction].eip = ptr[1];
					processes[currentfunction].esp = (int)&ptr[1];
					processes[currentfunction].ebp = ptr[0];
				
					if(processes[7].eip == processes[7].initialEip)	{
						processes[7].stack[1021] = processes[7].eip;
						processes[7].esp -= 4;
						ptr[0] = processes[7].ebp;
						processes[7].stack[1022] = ptr[2];
						processes[7].esp -= 4;
						processes[7].stack[1023] = ptr[3];
						processes[7].esp -= 4;						
						ptr[-1] = processes[7].esp;				
					}
					
					else {
						ptr[0] = processes[7].ebp;
						ptr[-1] = processes[7].esp;
					}
				}
				
				else
					ptr[-1] = (int)&ptr[1];
				
				currentfunction = 7;
				switchScreen(currentfunction);		
			}
			
			else if(inputChar == '8' && processes[8].eip != 0 && !atMain && currentfunction < 9) {
				isprocessed = 1;
				saveScreen(currentfunction);
				atMain = 0;
				
				if(currentfunction != 8){
					processes[currentfunction].eip = ptr[1];
					processes[currentfunction].esp = (int)&ptr[1];
					processes[currentfunction].ebp = ptr[0];
				
					if(processes[8].eip == processes[8].initialEip){
						processes[8].stack[1021] = processes[8].eip;	
						processes[8].esp -= 4;
						ptr[0] = processes[8].ebp;
						processes[8].stack[1022] = ptr[2];
						processes[8].esp -= 4;
						processes[8].stack[1023] = ptr[3];
						processes[8].esp -= 4;						
						ptr[-1] = processes[8].esp;	
					}
					
					else {
						ptr[0] = processes[8].ebp;
						ptr[-1] = processes[8].esp;
					}		
				}
				
				else
					ptr[-1] = (int)&ptr[1];
				
				currentfunction = 8;
				switchScreen(currentfunction);	
			}
			
			if(!isprocessed && characterASCII != 28 && characterASCII != 14) {
				if(currentfunction == 0 || currentfunction == 9)
					printChar(inputChar);
	
				appendChar(command, inputChar);
				ptr[-1] = (int)&ptr[1];
			}
			
			else if(characterASCII == 28) {
				int switchTo = runCommand();
				
				if(switchTo > 0)
					atMain = 0;			
				else if(switchTo == 0)		
					atMain = 1;
								
				if(switchTo == -1)			
					ptr[-1] = (int)&ptr[1];		
				else if(processes[switchTo].eip != 0){			
					saveScreen(currentfunction);
					
					if(currentfunction != switchTo){
						processes[currentfunction].eip = ptr[1];
						processes[currentfunction].esp = (int)&ptr[1];
						processes[currentfunction].ebp = ptr[0];
					
						if(processes[switchTo].eip == processes[switchTo].initialEip){
							processes[switchTo].stack[1021] = processes[switchTo].eip;
							processes[switchTo].esp -= 4;
							ptr[0] = processes[switchTo].ebp;
							processes[switchTo].stack[1022] = ptr[2];
							processes[switchTo].esp -= 4;
							processes[switchTo].stack[1023] = ptr[3];
							processes[switchTo].esp -= 4;						
							ptr[-1] = processes[switchTo].esp;						
						}
						
						else {
							ptr[0] = processes[switchTo].ebp;
							ptr[-1] = processes[switchTo].esp;
						}					
					}
					
					else
						ptr[-1] = (int)&ptr[1];
						
					currentfunction = switchTo;
					switchScreen(currentfunction);					
				}
				
				else {
					ptr[-1] = (int)&ptr[1];
					printStr("\n");
					printStr(header);
				}
			}
			
			else if(characterASCII == 14){
				if(vidCtr%160 > 26){
					vidPtr[vidCtr - 2] = ' ';			
					vidCtr = vidCtr - 2;
					moveCursor();		
				
					while(command[counter] != '\0')
						counter++;
					
					command[counter-1] = '\0';
					counter = 0;
				}
				
				ptr[-1] = (int)&ptr[1];
			}
			
			else
				ptr[-1] = (int)&ptr[1];
		}
		
		else	
			ptr[-1] = (int)&ptr[1];	
	}
}

void nextPtr(int *ptr){
	outb(0x20, 0x20);
}

void kmain(){
	while(1);
}

void stack_init(){
	int i,j;
	
	for(i=0; i<=9; i++) {
		processes[i].eip = 0;
		processes[i].initialEip = 0;
		processes[i].ebp = (int)&processes[i].stack[1024];
		processes[i].esp = processes[i].ebp;
		
		while(j < 2000) {
			processes[i].screenPtr[j] = ' ';
			j = j+1;
		}
	}
	
	for(i=0; i<=9;i++){
		processes[i].cursorY=0;
		processes[i].vidCtr=0;
	}
	
	processes[9].eip = (int)&listToMain;
	processes[9].initialEip = (int)&listToMain;
	strCpy("Program", processes[9].name);		
}

void main(int *ptr){
	stack_init();
	currentfunction = 0;
		
	idt_init();
	kb_init();
	
	vidCtr = 0;
	cursorY = 0;
	
	clearScreen();
	
	processes[0].initialEip = (int)&kmain;
	processes[0].eip = processes[0].initialEip;
	ptr[0] = processes[0].ebp;
	processes[0].stack[1023] = processes[0].initialEip;
	processes[0].esp -= 4;
	ptr[-1] = processes[0].esp;

	strCpy("Console", processes[0].name);
}

