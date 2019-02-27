#ifndef header
#define header

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOGIN "login"
#define LOGOUT "logout"
#define SOLD "listsold"
#define TRANSF "transfer"
#define UNLOCK "unlock"
#define QUIT "quit"

#define NOLOGIN -1
#define SESSIONACTIVE -2
#define WRONGPIN -3
#define WRONGCARD -4
#define CARDLOCKED -5
#define FAIL -6
#define UNLOCKFAIL -7
#define INSUFFICIENTFUNDS -8
#define CANCELED -9
#define ERROR -10
#define PASSWORDNEEDED -11
#define OK 10

#define NOLOGIN_MSG "Clientul nu este autentificat"
#define SESSIONACTIVE_MSG "Sesiune deja deschisa"
#define WRONGPIN_MSG "Pin gresit"
#define WRONGCARD_MSG "Numar card inexistent"
#define CARDLOCKED_MSG "Card blocat"
#define FAIL_MSG "Operatie esuata"
#define UNLOCKFAIL_MSG "Deblocare esuata"
#define INSUFFICIENTFUNDS_MSG "Fonduri insuficiente"
#define CANCELED_MSG "Operatie anulata"
#define ERROR_MSG "Eroare"
#define PASSWORDNEEDED_MSG "Trimite parola secreta"

#define YESCHR 'y'

void error(char *msg)
{
    perror(msg);
    exit(1);
}

char** strsplit( const char* s, const char* del ) {
	void* data;
	char* _s = ( char* )s;
	const char** ptrs;
	unsigned int
		ptrsSize,
		sLen = strlen( s ),
		delLen = strlen( del ),
		nbWords = 1;

	while ( ( _s = strstr( _s, del ) ) ) {
		_s += delLen;
		++nbWords;
	}
	ptrsSize = ( nbWords + 1 ) * sizeof( char* );
	ptrs = data = malloc( ptrsSize + sLen + 1 );
	if ( data ) {
		*ptrs = _s = strcpy( ( ( char* )data ) + ptrsSize, s );
		if ( nbWords > 1 ) {
			while ( ( _s = strstr( _s, del ) ) ) {
				*_s = '\0';
				_s += delLen;
				*++ptrs = _s;
			}
		}
		*++ptrs = NULL;
	}
	return data;
}

#endif
