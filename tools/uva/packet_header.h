/***
 * packet_header.h : Packet header
 *
 * Manifestation of socket packet header.
 * written by: gwangmu
 *
 * **/

#ifndef LLVM_CORELAB_PACKET_HEADER_H
#define LLVM_CORELAB_PACKET_HEADER_H

/* Server Packet type */
#define SERVER_RETURN				0
#define SERVER_REQUEST 			1
#define SERVER_OUTPUT 			2
#define SERVER_INPUT 				3
#define SERVER_LIBC 				4
#define SERVER_YIELD 				5

/* - Output operation type */
#define OUTPUT_STREAM 		 	1
#define OUTPUT_FDESC 	 			2
#define OUTPUT_FFLUSH 	 		3
#define OUTPUT_DEBUGSIG	 		4

#define OUTSRC_STDOUT 			1
#define OUTSRC_STDERR 			2
#define OUTSRC_FILEOUT 			3

/* - Input operation type */
#define INPUT_READLINE			1
#define INPUT_READN 				2
#define INPUT_FOPEN 				3
#define INPUT_FCLOSE 				4
#define INPUT_FSEEK 				5
#define INPUT_FTELL 				6
#define INPUT_FEOF 					7
#define INPUT_UNGETC 				8
#define INPUT_REWIND 				9
#define INPUT_POPEN 				10
#define INPUT_PCLOSE 				11
#define INPUT_OPEN 					12
#define INPUT_CLOSE 				13
#define INPUT_LSEEK 				14
#define INPUT_READ 					15
#define INPUT_FILEINFO 			16
#define INPUT_FDINFO 				17
#define INPUT_CLEARERR 			18

#define INSRC_STDIN 				1
#define INSRC_FILEIN 				2

/* - LibC operation type */
#define LIBC_REMOVE 				1
#define LIBC_UNLINK 				2
#define LIBC_GETCWD 				3
#define LIBC_SYSTEM 				4
#define LIBC_CHDIR 					5
#define LIBC_GETENV 				6
#define LIBC_RENAME 				7


/* Client Packet type */
#define CLIENT_RETURN 			0
#define CLIENT_REQUEST 			1

#endif
