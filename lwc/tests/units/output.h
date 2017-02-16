#ifndef __OUTPUT_H__
#define __OUTPUT_H__

#define KRED  	"\x1B[31m"
#define KGRN  	"\x1B[32m"
#define KRESET 	"\x1b[0m"	

#define TSUCCESS(s) printf(KGRN s KRESET)

#define TFAILURE(s) printf(KRED s KRESET) 

#endif