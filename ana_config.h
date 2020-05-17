//Some defines

#define PTRBUFF 32
#define BUFFERSIZE 32000
#define NUMATTRS 50
#define NUMSTYLES 100
#define MAXSEARCHRESULTS 2000
#define ANA_HNDL_BLOCK 1024
#define n_levels 32
#define NUMBOOKS 100
#define MAXMSS 64
#define MoreNom 0
#define OnlyNom 1
#define LessNom 2
#define NotNom 3
#define NotAny 4
//#define ANA_MYSQL

#if !defined(WIN32)
	#define MAC
#endif

//#define ANASTANDALONE 1
//#define _ANA_CDROM_ 1
// Apache's compatibility warnings are of no concern to us...this one is only not available on sunos4 which will never bother us
#undef strtoul
