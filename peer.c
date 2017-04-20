// -= Peer module =-
// ----------------
// (c) Assaf Haydu 
// ----------------

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>
#include <termio.h>	        // used for GETCH() simulation
#include <unistd.h>	        //  "
#include <math.h>
#include <time.h>

#define BUFLEN 512
#define NPACK 100
#define PORT 9930

#define SRV_IP "10.0.1.1"

#define SERVER_PORT 12345		/* arbitrary, but client and server must agree */
#define BUF_SIZE 8192			/* block transfer size */
#define QUEUE_SIZE 10
#define CHUNK_SIZE 256
#define MAX_FILES 500

struct fileStruct
{
	char fName[30];
	char uName[10];
	char ip[15];
	int port;
};

int loggedIn=0;			// 1 If logged in to server, 0 If not
int chatPort,filePort;
int credit[60];
char *userName,*fileName=0;
struct fileStruct filesFound[MAX_FILES];


/*
 * This func is a replacement to the 'getch()' function found on the windows-based "conio.h" library. This function is
 * essential for the "getString()" function.
 */
int getch()
{
  struct termios oldt,newt;
  int ch;

    tcgetattr( STDIN_FILENO, &oldt );		// Get old attributes of unix-based terminal
    newt = oldt;							// Save them
    newt.c_lflag &= ~( ICANON | ECHO );		// Remove character ECHO
    tcsetattr( STDIN_FILENO, TCSANOW, &newt );	// Set new attributes
    ch = getchar();								// Get a char from I/O
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt );  // Set old attributes back

  return ch;
}

/*
 * Gets a limited input from the user - characters that are pre-defined. 
 * Any other characters will be ignored. Returnes a dynamic array
 * maxChars - Number of maximum chars allowed
 * typeOfChars - If set to "1", will use regular Alphabet+Numbers. if "0", will use okChars array.
 * okChars[] Array of allowed characters (for example: "123abc")
 */
char *getString(int maxChars,int typeOfChars,char okChars[])
{
 int okCharsLen;	// Get size of okChars
 char *st;		// Output string
 char ch;		// Key pressed
 int i,stPos=0;		// Position integers
 int finish=0;		// Finish FLAG
 char abcNum[]="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

 if (typeOfChars==1)
	 okChars=abcNum;

 okCharsLen=strlen(okChars);
 st = (char *) malloc(maxChars+1);	// Allocate mem for output

 do
 {
	do
	{
	  i=0;
	  ch=getch();

	  if (ch==10) // User pressed ENTER
	  {
		  if (stPos>0)
		  {
			  putchar(13); // Return
			  putchar(10); // Go to beggining of line
			  finish=1;
			  break;
		  }
	    printf("\7");   // BEEP (nothing typed, and user pressed ENTER..)
	  }
	  else
	  if ((ch==127)&&(stPos>0)) // User pressed BACKSPACE
	  {
		 putchar(8);  // Go 1 char backwards
		 putchar(32); // Space (to erase current letter)
		 putchar(8);  // Go 1 char backwards again
		 stPos--;
	  	 st[stPos]=0;
	  }
	
	  // Any other key the user pressed -
	  while ((ch!=okChars[i])&&(i<=okCharsLen))  // checks weather the input is a member of okChars
		i++;

	} while (i>=okCharsLen); // DO

	if ((!finish)&&(stPos<maxChars))  // If all ok, adds the input to the "st" string
	{
		putchar(ch);		// Write char onScreen
		st[stPos]=+ch;		// Add char to ST
		stPos++;		// Increase position
	}

 } while (!finish); // DO

 for (i=stPos;i<maxChars+1;i++)    // Add NULL
   st[stPos]=0;

 return &st[0];  // Returns the string
}


/*
 * printMenu func. - Prints the Main menu
 */
void printMenu()
{
	printf("\n");
  	printf("Mini-Torrent CLIENT\n");
	printf("Choose one of the following:\n\n");
	printf("[0] Log in to SERVER\n");
	printf("[1] Split data to chunks (of 256 KB) \n");
	printf("[2] Send a list of my files \n");
	printf("[3] Check for shared directory for complete files and combine them to a single files \n");
	printf("[4] Submit a search query to the server for a file you wish to download \n");
	printf("[5] Submit a search query to the server for online users  \n");
	printf("[8] Log out.\n");
	printf("[9] EXIT\n");
	printf("[?] Print this menu again\n");
}

/*
 * sendMessage func. - Sends a TCP message to a specific socket.
 */
void sendMessage(int s, char *message)
{
	int ans;

	ans=write(s, message, strlen(message));
    if(ans==-1)
    	printf("SOCKET ERROR: Cannot send message.\n\n");

}/*sendMessage*/


/*
 * sendMessageTo func. - Sends a TCP message to a specific SOCKET - IP and PORT.
 */
void sendMessageTo(int s,char *ip, int port, char *message)
{
	int ans;
	struct sockaddr_in channel;		// holds IP address
	struct hostent *h;

	h = gethostbyname(ip);		// look up host's IP address
	if (!h) {printf("FATAL ERROR: GetHostbyName function failed"); return;};

	memset(&channel, 0, sizeof(channel)); // Zeros
	channel.sin_family= AF_INET;
	memcpy(&channel.sin_addr.s_addr, h->h_addr, h->h_length);
	channel.sin_port= htons(port);  // Set port

	ans=sendto(s, message,strlen(message), 0, (struct sockaddr *)&channel,sizeof(channel));
	//write(s, message, strlen(message));
    if(ans==-1)
    	printf("SOCKET ERROR: Cannot send message.\n\n");

} /*sendMessageTo*/


/*
 * this function will send the correct message to the peer
 * with a int code name that is the type of message
 * 0-TAKE
 * 1-eror
 * 2-FBAD
 * 3-GOOD
 * 4-LGIN
 * 5-LRES
 * 6-FGET
 * 7-NOCR
 * the parameter: s=socket,message,num =the message type header,
 */
void sendMessageToPeer(int s, char *message,int num)
{
	int ans;
	char Pmessage[55]; // Message to send
	memset( Pmessage,'\0',sizeof(Pmessage)); // Zeros

	switch (num)
	{
			case 0://TAKE
				strcat(Pmessage,"TAKE"); break;
			case 1://EROR
				strcat(Pmessage,"EROR"); break;
			case 2://FBAD
				strcat(Pmessage,"FBAD"); break;
			case 3: //GOOD
				strcat(Pmessage,"GOOD"); break;
			case 4://LGIN
				strcat(Pmessage,"LGIN"); break;
			case 5: //LRES
				strcat(Pmessage,"LRES"); break;
			case 6://FGET
				strcat(Pmessage,"FGET"); break;
			case 7://NOCR
				strcat(Pmessage,"NOCR"); break;
		}//end of switch

	strcat(Pmessage,message);
	strcat(Pmessage,"\n");
	ans=write(s, Pmessage, strlen(message));
    if(ans==-1)
    	printf("SOCKET ERROR: Cannot send message.\n\n");

}/* sendMessageToPeer*/

/*
 * quitSequence func. - Preforms a sequence for quitting.
 * Sends a "quit" message to server and Closing the socket if needed.
 */
void quitSequence(int s)
{
	char message[50];

	if (s!=0)  // If there's a socket conected
	{
		printf("Closing SOCKET and Quitting.....\n\n");

		memset(message,'\0',sizeof(message));  // Zero
		strcat(message,"QUIT\n");
		sendMessage(s,message);		// Sends "QUIT" message to server

		close(s);	// Closing socket
	}

	exit(0);
}/*quitSequence*/

/*
 * fatalError func. - Displays error message (string) on screen, and performs quitSequence.
 */
void fatalError(int s,char *string)
{
  printf("\nEROR MSG: %s", string);
  quitSequence(s);
}/*fatalError*/

/*
 * F_RestoreChunks func. - Restors chunks of files in the ./shared dir to original files in ./completed dir.
 */
void F_RestoreChunks(int s)
{
	struct dirent **namelist;
    int n,counter=0,first=0,sameFiles=0,fileCnt=0,oldFileCnt=0,numChunks=0,finished=0;
    char fileName[30],lastFileName[30],tokenString[30];
    char *tok=0;
    char command[900]="";

	memset(fileName,'\0',sizeof(fileName));   // Zeros
	memset(lastFileName,'\0',sizeof(lastFileName));  // Zeros
	memset(tokenString,'\0',sizeof(tokenString));

    n = scandir("./shared", &namelist, 0, alphasort);
    if (n < 0) 	perror("scandir");
    else

    while (!finished)
     {
		 if (((first==0)||(sameFiles==0))&&((counter+2)<n))
		 {
			 strcpy(tokenString,namelist[counter+2]->d_name);
			 tok=strtok(tokenString,"_");

			 if (first==0)
				 { strcpy(fileName,tok);  strcpy(lastFileName,tok);  }
			 else strcpy(lastFileName,tok);

			 if (strcmp(lastFileName,fileName)==0)
			 {
				 tok=strtok(0,"_");
				 if (tok!=0)
				 {
					 fileCnt=atoi(tok);  // Chunk number
					 if ((fileCnt-1)!=oldFileCnt)
						 numChunks=999; // Bad file, missing chunk
					 oldFileCnt=fileCnt;
					 tok=strtok(0,"_"); // "of" is thrown out...
					 if (first==0)
					 { tok=strtok(0,"_");  numChunks=atoi(tok); }
					 counter++;
				 }
				 else
				 {
					 sameFiles=1;   // SingleFile detected.
					 numChunks=1;
					 fileCnt=1;
					 counter++;
				 }
			 }
			 else
     			sameFiles=1;

			 first=1;   // Not phase one anymore
		 }
		 else
		 {
		   first=0;
		   sameFiles=0;
		   if (fileCnt==numChunks)
		   {
			   printf("File complete: [%s] \n",fileName);
			   memset(command,'\0',sizeof(command));
			   strcpy(command,"cd shared;");
			   strcat(command,"cat ");
			   strcat(command,fileName);
			   strcat(command,"* > ../completed/");
			   strcat(command,fileName);
			   strcat(command,";");
			   system(command);
		   }
		   if (counter==(n-2)) finished=1;
		   fileCnt=0;
		   oldFileCnt=0;

		    memset(fileName,'\0',sizeof(fileName));   // Zeros
		   	memset(lastFileName,'\0',sizeof(lastFileName));  // Zeros
		   	memset(tokenString,'\0',sizeof(tokenString));

		 }
	  }
}/*F_RestoreChunks*/

void recieveFileFromPeer(int s, int firstBytes, char *filename, char *buf)
{
  char preamble[7]="";
  int bytes,endPos=0;
  char *tok,*buffer;
  char *eofData,fullFileName[50];
  FILE *file;

  printf("Recieving file: [%s] from peer...\n",filename);

  strcat(fullFileName,"./shared/");
  strcat(fullFileName,filename);
  file=fopen(fullFileName,"w+");
  if (file==0)
  {
     printf("Cannot open output file.\n");
     return;
  }

  buffer=(char *)malloc(BUF_SIZE);
  eofData=(char *)malloc(BUF_SIZE);
  memset(buffer,'\0',sizeof(buffer));   // Zeros
  memset(eofData,'\0',sizeof(eofData));   // Zeros
  strcpy(buffer,buf);

  buffer=buffer+4;  // Bypass "FILE"
  tok=strtok(buffer,"#");
  strcpy(preamble,tok);
  buffer=buffer+8;  // Bypass "preamble#"

  bytes=firstBytes-12;  // Remove "FILEpreamble#" from count of bytes
  tok = 0;
  tok = strstr (buffer,preamble);  // Finds the 2nd preamble pos

  while (tok==0)		// If there's no preamble at the end, the file is sent in parts
  {
	printf("Part sized [%d] recieved\n",bytes);
	fwrite(buffer,bytes, 1, file);    // WRITE PART TO FILE
	memset(buffer,'\0',sizeof(buffer)); // Zeros
	bytes=read(s, buffer, BUF_SIZE);	// Read TCP Input
	if (bytes <= 0) exit(0);

	tok = strstr (buffer,preamble);
  }

  if (tok!=0) bytes=bytes-9;   // Remove "#preamble/n" from count of bytes

  printf("FINALPart sized [%d] recieved\n",bytes);
  endPos=strcspn(buffer,preamble);  // Find pos of preamble
  endPos--;   // Remove "#"
  strncpy(eofData,buffer,endPos);

  fwrite(eofData,bytes, 1, file); // WRITE END PART TO FILE

  printf("Download finished.\n");

  fclose(file);

}/*recieveFileFromPeer*/


void sendFileToPeer(int s,char *fName)
{
  char preamble[7]="";
  int i;
  FILE *file;
  char *buffer,fullFileName[50];
  unsigned long fileLen;
  char *message;

  strcat(fullFileName,"./shared/");
  strcat(fullFileName,fName);
  for (i=0;i<7;i++)
	  preamble[i]=(rand()%1)+48;

  printf("Sending file: [%s] from peer...\n",fName);

  //Open file
  file = fopen(fullFileName, "rb");
  if (!file)
  {
  	fprintf(stderr, "Unable to open file %s", fullFileName);
  	return;
  }

  //Get file length
  fseek(file, 0, SEEK_END);
  fileLen=ftell(file);
  fseek(file, 0, SEEK_SET);

  //Allocate memory
  message=(char *)malloc(fileLen+1+25);
  buffer=(char *)malloc(fileLen+1);
  if ((!buffer)||(!message))
  {
  	fprintf(stderr, "Memory error!");
    fclose(file);
 	return;
  }

  //Read file contents into buffer
  fread(buffer, fileLen, 1, file);
  fclose(file);

  memset(message,'\0',sizeof(message));   // Zeros
  strcat(message,"FILE");
  strcat(message,preamble);
  strcat(message,"#");
  strcat(message,buffer);
  strcat(message,"#");
  strcat(message,preamble);
  strcat(message,"\n");

  sendMessage(s,message);
  printf("File sent successfully.\n");

  //Do what ever with buffer

  free(buffer);
  free(message);

}/*sendFileToPeer*/


/*
 * this function will check if we got the chunk name in our list. we will return int value:
 * -1= we don't want this chunk we have it,eror the chunk name isn't valid
 * 0=we want this chunk
 */
int checkFileName(char *fileName)
{
	FILE *stream;
	int ans=-1;
	char fullFileName[50];
	memset(fullFileName,'\0',sizeof(fullFileName)); // Zeros

	strcat(fullFileName,"./shared/");
	strcat(fullFileName,fileName);
	stream=fopen(fullFileName,"r"); //if zero it means it couldn't open-> we need this file because we don't have it
	if (stream==0)
		ans=0;
	else
	fclose(stream);

	return ans;

}//checkFileName


/*
 * this function will wait to read the peer response
 */
int analyzePeerResponse(int s)
{
		char buf[BUF_SIZE];
		int bytes;
		int ans =0;
		char msgType[5];   // Will hold message type recieved
		char *tmp=0;	   // For tokenizer usage
		char message[50]; // Message to send

		memset( message,'\0',sizeof(message)); // Zeros
		memset(buf,'\0',BUF_SIZE); 		// Zero

		bytes=read(s, buf, BUF_SIZE);	// Read TCP Input
		if (bytes <= 0) exit(0);

		strncpy(msgType,buf,4);  // Get message type
		msgType[4]='\0';

		 	// ---= LRES Message =--------------------------------------------------------------
		 	if (strcmp(msgType,"LRES")==0)
		 	 	{
		 	 	   loggedIn=1;
		 	 	   tmp=buf;
		 	 	   tmp=tmp+4;  // Remove first 4 bytes of message
		 	 	   printf("\n\r%s MSG: %s",msgType,tmp);
		 	 	   //sendFileRequestToPeer(s);//this function will send FGET MESSAGE to the peer
		 	 	   return 1;
		 	 	}
		 	 else
			// ---= EROR Message =--------------------------------------------------------------
		 	 if (strcmp(msgType,"EROR")==0)//this function will print the eror
		 	 	{
		 	 	   tmp=buf;
		 	 	   tmp=tmp+4; // Remove first 4 bytes of message
		 	 	 //  fatalError(s,tmp);
		 	 	  printf("\n\r%s MSG: %s",msgType,tmp);
		 	 	  //printMenu();//back to main menu
		 	 	   return -1;
		 	 	}
		 	 else
		 		 // ---= FGET Message =--------------------------------------------------------------
		 	 if (strcmp(msgType,"FGET")==0)
		 		{
					tmp=strtok(buf,"\n");
					tmp=tmp+4; // Remove first 4 bytes of message
					printf("\n\rFGET MSG: %s",tmp);//will print the chunkname
					sendFileToPeer(s,tmp);
		 			return 2;
		 		}
		 	 else
		 		//----NOCR Message=------------------------------------------------------
		 		 if (strcmp(msgType,"NOCR")==0)
		 		 {
		 			tmp=buf;
		 			tmp=tmp+4; // Remove first 4 bytes of message
		 			printf("\n\r%s MSG: %s",msgType,tmp);//will print the chunkname
		 			printf("you got an NOCR message this means you have no credit in that peer\n\r");
		 			printf("please  uplode some chunks to this peer  in order to increase your credit\n\r");
		 			return 3;
		 		 }
		 		 else
		 		//----TAKE  Message=------------------------------------------------------
		 		 if (strcmp(msgType,"TAKE"))
		 		 {
		 			tmp=buf;
		 			tmp=tmp+4; // Remove first 4 bytes of message
		 			printf("\n\r%s MSG: %s",msgType,tmp);//will print  the chunk name
		 			ans=checkFileName(msgType);   //need to do

		 			if(ans ==-1)
		 			{
		 				printf("i Don't want this chunk!\n\r");
		 				strcat(message,"i Don't want this chunk!");
		 				sendMessageToPeer(s,message,2);//send FBAD
		 			}//i have this chunk
		 			if (ans ==0)
		 			{
		 				printf("i want this chunk\n\r");
		 				strcat(message,"i want this chunk!");
		 				sendMessageToPeer(s,message,3);//send good
		 			}//i want this chunk
		 			return 4;
		 		 }
		 		 else
		 			//----NOCR Message=------------------------------------------------------
		 			if (strcmp(msgType,"FILE")==0)
		 		    {
		 				if (fileName!=0)
							recieveFileFromPeer(s,bytes,fileName,buf);
		 				else {printf("ERROR in filename");};
		 				memset(fileName,'\0',sizeof(fileName));
		 			    return 5;
		 			 }
		 			 else
		 			//----MESG message for udp part----------------------------
		 			if (strcmp(msgType,"MESG")==0)
		 		    {
		 				tmp=buf;
		 				tmp=tmp+4; // Remove first 4 bytes of message
		 				printf("\n\r%s MSG: %s",msgType,tmp);//we will print"username says:"
		 				return 6;
		 			 }

   return -2;

}/*analyzePeerResponse*/


/*
 * analyzeServerResponse func. - Main func. for analyzes response after sending messages to SERVER.
 * Deals with: "LRES","EROR","SHOK","FRES","WHOR" commands.
 */
int analyzeServerResponse(int s)
{
	char buf[BUF_SIZE];
	int bytes,i;
	char msgType[5];   // Will hold message type recieved
	char *tmp=0;	   // For tokenizer usage
	int counter=0;	   // A Counter for FRES,WHOR

	memset(buf,'\0',BUF_SIZE); 		// Zero
	bytes=read(s, buf, BUF_SIZE);	// Read TCP Input
 	if (bytes <= 0) exit(0);

 	strncpy(msgType,buf,4);  // Get message type
 	msgType[4]='\0';

 	// ---= LRES Message =--------------------------------------------------------------
 	if (strcmp(msgType,"LRES")==0)
 	{
 	   loggedIn=1;
 	   tmp=buf;
 	   tmp=tmp+4;  // Remove first 4 bytes of message
 	   printf("\n\r%s MSG: %s",msgType,tmp);
 	   return 1;
 	}
 	else
 	// ---= EROR Message =--------------------------------------------------------------
 	if (strcmp(msgType,"EROR")==0)
 	{
 	   tmp=buf;
 	   tmp=tmp+4; // Remove first 4 bytes of message
 	   fatalError(s,tmp);
 	   return 0;
 	}
 	else
 	// ---= SHOK Message =--------------------------------------------------------------
 	if (strcmp(msgType,"SHOK")==0)
 	{
 	   printf("\nSHOK MSG: Share OK\n");
 	   return 2;
 	}
 	else
 	// ---= FRES Message =--------------------------------------------------------------
 	if (strcmp(msgType,"FRES")==0)
 	{
 		printf("\nFSER MSG: Waiting for list..\n");
 		tmp=0;

 		for (i=0;i<MAX_FILES;i++)
 		{
 			memset(filesFound[i].fName,'\0',sizeof(filesFound[i].fName));
 			memset(filesFound[i].uName,'\0',sizeof(filesFound[i].uName));
 			memset(filesFound[i].ip,'\0',sizeof(filesFound[i].ip));
 			filesFound[i].port=0;
 		}

 		while ((buf[4]!='\n')&&(counter!=-1))
 		{
 		  counter=0;
 		  while (1)  //NULL
 		  {
 			  if (counter==0) tmp=strtok(buf,"#");   // If first token
 			  else tmp=strtok(0,"#");

 			  if (tmp==0) break;		// If end of tokens, break
              else if (*(tmp+4)=='\n')  // If we recieved end_of_list in the same buffer
              { counter=-1;  break;  }

 			  tmp=tmp+4;  		  printf("File: [%s]",tmp);		// Output
 			  strcpy(filesFound[counter].fName,tmp);
 			  tmp=strtok(0,"#");  printf(" User: [%s]",tmp);
 		      strcpy(filesFound[counter].uName,tmp);
 			  tmp=strtok(0,"#");  printf(" IP: [%s]",tmp);
 			  strcpy(filesFound[counter].ip,tmp);
 			  tmp=strtok(0,"#\n");printf(" ServicePort: [%s]\n",tmp);
 			  filesFound[counter].port=atoi(tmp);
 			  counter++;
		  }
 		  memset(buf,'\0',BUF_SIZE);			// Zeros
 		  if ((buf[4]!='\n')&&(counter!=-1))	// If not end
 			  bytes=read(s, buf, BUF_SIZE);
 		  if (bytes <= 0) exit(0);
 		}

 	   printf("FSER MSG: End of list.\n");
  	   return 3;
	}
 	else
 	// ---= WHOR Message =--------------------------------------------------------------
 	if (strcmp(msgType,"WHOR")==0)
 	 	{
 	 		printf("\nWHOR MSG: Waiting for online-users list..\n");
 	 		tmp=0;

 	 		while ((buf[4]!='\n')&&(counter!=-1))
 	 		{
 	 		  counter=0;
 	 		  while (1)  //NULL
 	 		  {
 	 			  if (counter==0) tmp=strtok(buf,"#");  // If first token
 	 			  else tmp=strtok(0,"#");

 	 			  if (tmp==0) break;
 	              else if (*(tmp+4)=='\n')  // If we recieved end_of_list in the same buffer
 	              {	counter=-1; break;  }

 	 			  tmp=tmp+4;   			printf("User: [%s]",tmp);// Output
 	 			  tmp=strtok(0,"#");    printf(" IP: [%s]",tmp);
 	 			  tmp=strtok(0,"#\n");  printf(" ChatPort: [%s]",tmp);
 	 			  counter++;
 			  }
 	 		  memset(buf,'\0',BUF_SIZE);		   // Zero
 	 		  if ((buf[4]!='\n')&&(counter!=-1))  // If not end
 	 			  bytes=read(s, buf, BUF_SIZE);
 	 		  if (bytes <= 0) exit(0);
 	 		}
 	 	   printf("\nWHOR MSG: End of list.\n");
 	  	   return 3;
 	 	}

  return -1;

}

/*
void sendFileRequestToPeer(int s)
{
	char message[50]; // Message to send
	memset( message,'\0',sizeof(message)); // Zeros
	char *fileName;		    // Filenames to be downloaded from peers

	sendMessageToPeer(s,fileName,6);  //we will send FGET message

	analyzePeerResponse(s);   //we will wait for the next message for the peer
}// getChunkFromPeer*/



/*
 * searchQuery func. - Sends a search query to server, and displays file-list returned
 */
void searchQuery(int s)
{
	char message[4096];// Message to send
	char *token;   	   // For tokenizer

	if (loggedIn==0)
	{
		printf("ERROR: You're not logged in.\n");
		return;
	}

	memset( message,'\0',sizeof(message));  // Zero

	printf("Enter search token: ");
	token=getString(20,1,"");

	strcat(message,"FSER");
	strcat(message,token);
	strcat(message,"\n");

	sendMessage(s,message);  // Send message
	analyzeServerResponse(s);
}

/*
 * whoQuery func. - Sends a query for who is online to server, and displays a list of users online returned.
 */
void whoQuery(int s)
{
	char message[4096]; // Message to send
	char chatPortStr[6];

	if (loggedIn==0)
	{
		printf("ERROR: You're not logged in.\n");
		return;
	}

	memset(message,'\0',sizeof(message));
	sprintf(chatPortStr,"%d",chatPort);		// Convert chatPort(int) to chatPortStr(str)

	strcat(message,"WHO?");
	strcat(message,chatPortStr);
	strcat(message,"\n");

	sendMessage(s,message);  // Sends message
	analyzeServerResponse(s);
}

/*
 * sendList func. - Sends a list of files (chunks) from the ./shared dir to be shared on server.
 */
void sendList(int s)
{
	struct dirent **eps;  // To use with scandir
	int n; 					 // Number of files on curr. dir
	char message[BUF_SIZE];  // Message to send
	char filePortStr[6];

	if (loggedIn==0)
	{
		printf("ERROR: You're not logged in.\n");
		return;
	}

	memset( message,'\0',sizeof(message)); // Zero

	sprintf(filePortStr,"%d",filePort);  // Convert int to str

	strcat(message,"FSHE");
	strcat(message,filePortStr);

	n = scandir ("./shared/", &eps, 0, alphasort);  // Scans the directory for files
	if (n >= 0)
	{
		int cnt;
		for (cnt = 2; cnt < n; ++cnt)   // CNT starts from 2, to remove ".",".." which are unusable
		{
			strcat(message,"#");
			strcat(message,eps[cnt]->d_name);  // Add curent file to message
		}
	}
	else
	fatalError(s,"Couldn't open the directory, SHARED dir couldn't be found!\n");

	strcat(message,"\n");
	printf("\nSharing %d chunks...",n-2);

	sendMessage(s,message);  // Sends message
	analyzeServerResponse(s);
}

/*
 * login func. - Performs a login to the server using an encrypted password.
 */
void lginServer(int s)
{
	char *pass;
	char message[50]; // Message to send
	int i;			  // For usage
	int keyInt=0,passLen;  // keyInt - key for encryption
	char keyChr[2];

	if (loggedIn==1)
	{
		printf("You're already logged in.\n\n");
		return;
	}

	memset( message,'\0',sizeof(message)); // Zeros

    printf("Enter USERNAME: ");
    userName=getString(8,1,"");			// Gets string
	printf("Enter PASSWORD: ");
	pass=getString(8,1,"");			// Gets string

	keyInt=rand()%9;				// Choose random encryption integer

	passLen = strlen(pass);
	for(i = 0; i <= passLen-1; i++)	// Do encryption
			pass[i] -= keyInt;

	keyChr[0]=keyInt+48;			// Convert to string
	keyChr[1]=0;

	strcat(message,"LGIN");
	strcat(message,userName);
	strcat(message,"#");
	strcat(message,keyChr);
	strcat(message,pass);
	strcat(message,"\n");

	sendMessage(s,message);		// Send login message
	analyzeServerResponse(s);

}


/*
 * logout func. - Performs logout scheme
 */
void logout(int s)
{
	char message[50];

	if (loggedIn==1)
	{
		printf("Logging out.\n");

		memset(message,'\0',sizeof(message));
		strcat(message,"LOUT\n");

		sendMessage(s,message); // Send logout message
		loggedIn=0;
	}
	else
		printf("ERROR: You're not logged in.\n");

}

/*
 * F_PrepareChunks func. - Creates chunks of files in ./Completed dir with the size of CHUNK_SIZE
 */
void F_PrepareChunks()
{
	char command[900]="";
	char temp[200]="";
	int i;
	strcpy(command,"cd completed;");
	strcat(command,"for complete_file in *; do ");
	strcat(command,"cd ../shared;");
	strcat(command,"rm -f $complete_file\\_*;");
	sprintf(temp, "split -b %dk -d -a 3 ../completed/$complete_file temp_$complete_file\\_;", CHUNK_SIZE);  // Split to chunks
	strcat(command,temp);
	strcat(command,"i=1;for file in temp_$complete_file\\_*; do mv $file $(printf $complete_file\\_%03i $i);i=$((i+1)); done;");
	strcat(command,"rm -f temp_$complete_file\\_*;");
	strcat(command,"res=$(ls -l $complete_file\\_* | wc -l);");
	strcat(command,"subst=s/$/_of_$res/;");
	strcat(command,"rename $subst $complete_file\\_*;");
	strcat(command,"done;");

	i=system(command);
	 if (i==-1) puts ("Error executing PrepareChunks");
	  else puts ("SPLIT: Command successfully executed.");

}/* F_PrepareChunks */

int lginToPeer(int s)
{
	//	char *name;
		char message[50]; // Message to send
	//	memset( message,'\0',sizeof(message)); // Zeros
	//	printf("please enter the remote peer user name\n\r");
	//	name=getString(8,1,"");			// Gets string

		strcat(message,"LGIN");
		strcat(message,userName);
		strcat(message,"\n");

		sendMessage(s,message);		// Send login to remote peer message
		return analyzePeerResponse(s);

}/*lginToPeer */


void startPeerCommunication(int s)
{
	printf("Connected to PEER!\n");

	while(1)
	{
		analyzePeerResponse(s);
	}
}/*startPeerCommunication*/


void listenToPeers()
{
	int s1,b,l,sa,child;
	struct sockaddr_in channel;		/* holds IP address */

	/* Build address structure to bind to socket. */
	memset(&channel, 0, sizeof(channel));	/* zero channel */
	channel.sin_family = AF_INET;
	channel.sin_addr.s_addr = htonl(INADDR_ANY);
	channel.sin_port = htons(filePort);

	/* Passive open. Wait for connection. */
	s1 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); /* create socket */
    if (s1 < 0) {printf("socket failed"); return;};

	b = bind(s1, (struct sockaddr *) &channel, sizeof(channel));
	if (b < 0) {printf("bind failed"); return;};

	l = listen(s1, QUEUE_SIZE);		/* specify queue size */
    if (l < 0) {printf("listen failed"); return;};

    while (1)
     {
           sa = accept(s1, 0, 0);		/* block for connection request */
           if (sa < 0) { printf("accept failed"); return;};
           child=fork();

           if (child==0)
              startPeerCommunication(sa);

     }
}/*listenToPeers/

int connectToPeer(char *ip, int port)
{
	int s,c;
	struct hostent *h;
	struct sockaddr_in channel;		/* holds IP address */

	h = gethostbyname(ip);		/* look up host's IP address */

	s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);  // Socket connect
	if (s < 0)
		{printf("FATAL ERROR: Socket initializing failed"); return -1;};

	memset(credit,'\0',sizeof(credit));//initialize the credit array.
	memset(&channel, 0, sizeof(channel)); // Zeros
	channel.sin_family= AF_INET;
	memcpy(&channel.sin_addr.s_addr, h->h_addr, h->h_length);
	channel.sin_port= htons(port);

    c = connect(s, (struct sockaddr *) &channel, sizeof(channel)); // Server connect
	if (c < 0)
		{printf("FATAL ERROR: Server connection failed"); return -1;};

	printf("Connected to PEER at ip [%s] and port [%d].\n",ip,port);
	return s;
} /*connectToPeer*/


void OLDconnectToPeer()
{
	int s2,c;
	struct hostent *h;
	struct sockaddr_in channel;		/* holds IP address */
	char *portQ;

	h = gethostbyname("127.0.0.1");		/* look up host's IP address */

	s2 = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);  // Socket connect
	if (s2 < 0)
		{printf("FATAL ERROR: Socket initializing failed"); return;};

	printf("Enter port: ");
	portQ=getString(5,1,"");

	memset(credit,'\0',sizeof(credit));//initialize the credit array.
	memset(&channel, 0, sizeof(channel)); // Zeros
	channel.sin_family= AF_INET;
	memcpy(&channel.sin_addr.s_addr, h->h_addr, h->h_length);
	channel.sin_port= htons(atoi(portQ));

    c = connect(s2, (struct sockaddr *) &channel, sizeof(channel)); // Server connect
	if (c < 0)
		{printf("FATAL ERROR: Server connection failed"); return;};

	printf("Connected TO PEER!.\n");
}/*OLDconnectToPeer*/


void startMultiDownload(int s)
{
	int i;
	char *tok;
	int s1;

	printf("Enter name of file to download: \n");
	fileName=getString(30,1,"");			// FILENAME IS A GLOBAL VARIABLE

	while (filesFound[i].port!=0)
	{
		tok=strtok(filesFound[i].fName,"_");

		// If this is one of the chunks we want, and it doesn't exist on HD
		if ((strcmp(tok,fileName)==0)&&(checkFileName(filesFound[i].fName)==0))
		{
			s1=connectToPeer(filesFound[i].ip,filesFound[i].port);
			if (lginToPeer(s)>0)  //Login success
			{
				sendMessageToPeer(s,fileName,6);  // Send FGET
				analyzePeerResponse(s);		  	  // Get response, start file transfer
			}
		}

		i++;
	}
}/*startMultiDownload*/

/*
 * this function will return the number of the udp socket
 */
int connectToPeerUdp(char *ip, int port)
{

	struct sockaddr_in si_other;
  	int s, i, slen=sizeof(si_other);
 	 char buf[BUFLEN];

  	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
    diep("socket");

  	memset((char *) &si_other, 0, sizeof(si_other));
  	si_other.sin_family = AF_INET;
  	si_other.sin_port = htons(PORT);

 	 if (inet_aton(SRV_IP, &si_other.sin_addr)==0)
 	 {
  	  fprintf(stderr, "inet_aton() failed\n");
  	  exit(1);
  	}
  	return s;

}/*connectToPeerUdp*/

/*
 * this function will open udp port for udp server and will listen to information
 */
void listenToPeersUdp()
{

	struct sockaddr_in si_me, si_other;
	int ud, i, slen=sizeof(si_other);
	char buf[BUF_SIZE];

	char msgType[5];   // Will hold message type recieved
	char *tmp=0;	   // For tokenizer usage
	char message[50]; // Message to send


	if ((ud=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
		diep("socket");

  memset((char *) &si_me, 0, sizeof(si_me));
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(PORT);
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(ud, &si_me, sizeof(si_me))==-1)
      diep("bind");


	printf("Connected to PEER!\n");
	while(1)
	{
			for (i=0; i<NPACK; i++)
 			 {
  				  if (recvfrom(ud, buf, BUFLEN, 0, &si_other, &slen)==-1)
   				   diep("recvfrom()");
     			//  sprintf(buf, "This is packet %d\n", s);
   				 printf("Received packet from %s:%d\nData: %s\n\n",
         		  inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port), buf);
 			 }//end of for

		memset( message,'\0',sizeof(message)); // Zeros
		memset(buf,'\0',BUF_SIZE); 		// Zero


		strncpy(msgType,buf,4);  // Get message type
		msgType[4]='\0';

		//----MESG message for udp part----------------------------
		 if (strcmp(msgType,"MESG")==0)
		  	{
		 	tmp=buf;
		 	tmp=tmp+4; // Remove first 4 bytes of message
		 	printf("\n\r%s MSG: %s",msgType,tmp);//we will print"username says:"

		 	}
	}

} /*listenToPeersUdp*/

void diep(char *s)
{
  perror(s);
  exit(1);
} /*diep*/


void sendMessageToPeerUdp(int ud)
{
	struct sockaddr_in si_me, si_other;
	 int  slen=sizeof(si_other);
	int i;
	char buf[BUFLEN];
	char *req;
	memset(req,'\0',sizeof(req));//initialize req
	printf("please enter the name pf the user that you want to chat\n");
	req=getString(4,1,"");
	strcat(buf,"MSEG");
	strcat(buf,req);
	strcat(buf,"says:");
	printf("please enter your message for the udp peer");
	memset(req,'\0',sizeof(req));//init req
	req=getString(50,1,"");
	strcat(buf,req);
	strcat(buf,"\n");//buf is our message.
	 for (i=0; i<NPACK; i++) //sending bit by bit
  		{
   			 printf("Sending packet %d\n", i);
   			 sprintf(buf, "This is packet %d\n", i); /* sent 10 text messages (packets) */

    		if (sendto(ud, buf, BUFLEN, 0, &si_other, slen)==-1)
     		 diep("sendto()");
  		}//end of for


} /*sendMessageToPeerUdp*/


/*
 * MAIN Func. - Main program function
 */
int main(int argc, char *argv[])
{
	char *choice ;			// User's choice
	int c, s=0;				// c for Connect command, s for Socket
	char test[10];
	struct hostent *h;				/* info about server */
	struct sockaddr_in channel;		/* holds IP address */
	int child;
	strcpy(test,"aaa.txt");
	h = gethostbyname(argv[1]);		/* look up host's IP address */
	if (!h) fatalError(s,"FATAL ERROR: GetHostbyName function failed");

	s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);  // Socket connect
	if (s < 0) fatalError(s,"FATAL ERROR: Socket initializing failed");
	memset(credit,'\0',sizeof(credit));//initialize the credit array.
	memset(&channel, 0, sizeof(channel)); // Zeros
	channel.sin_family= AF_INET;
	memcpy(&channel.sin_addr.s_addr, h->h_addr, h->h_length);
	channel.sin_port= htons(SERVER_PORT);

	c = connect(s, (struct sockaddr *) &channel, sizeof(channel)); // Server connect
	if (c < 0) fatalError(s,"FATAL ERROR: Server connection failed");
	printf("Connected.\n");


	srand(time(0));
	chatPort=(rand()%5000)+10001;	// Set random chatport
	filePort=(rand()%5000)+10001;	// Set random fileport

	child=fork();
	if (child==0)
		listenToPeers(); //LISTEN TO PEERS!


	printf("MYPORT IS: [%d]\n",filePort);
	printMenu();	// Prints menu

	while (choice[0]!='9') // Main loop
	{
		printf("\nCommand(?=MENU): ");
		choice=getString(1,0,"01234589?c");	// Get the user Command

		switch (choice[0])
		{
			case '0':lginServer(s);			   // Login
					 break;

			case '1':F_PrepareChunks();    // Split files
					 break;

			case '2':sendList(s);		   // Send list of files
					 break;

			case '3':F_RestoreChunks(s);   // Restore chunked files
					 break;

			case '4':searchQuery(s);	   // Do search query
					 break;

			case '5':whoQuery(s);		   // Do who query
					 break;

			case '6':lginToPeer(s);		   // Do login to remote peer.
					 break;

			case '8':logout(s);			   // Logout
					 break;

			case '9':quitSequence(s);	   // Quit
					 break;

			case 'c': checkFileName(test);
			         break;

			case '?':printMenu();		   // Print menu
					 break;
			}


	}


  return EXIT_SUCCESS;
}/*main*/


