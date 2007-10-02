/***************************************************************************
 *   Copyright (C) 2007 by Bernd Kosmahl   *
 *   beko@duke.famkos.net   *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/


#include "SDL_rwops.h"
#include "log.h"

/*
mode kann folgende Werte annehmen:
r  Datei zum Lesen �ffnen, die Datei muss existieren.
w  Datei zum Schreiben �ffnen, die Datei muss existieren.
a  Hinzuf�gen zu der Datei, Daten werden am Ende der Datei hinzugef�gt.
r+  Datei zum Lesen und Schreiben �ffnen, die Datei muss existieren.
w+  �ffnet eine leere Datei zum Lesen und Schreiben. Wenn eine Datei mit dem Namen existiert, wird sie �berschrieben.
a+  �ffnet eine Datei zum Lesen und Hinzuf�gen. Alle Schreiboperationen geschehen am Ende der Datei.

Zus�tzlich kann noch einer der folgenden Buchstaben zu mode hinzugef�gt werden.
t  Textmodus: Das Ende der Datei ist das STRG+Z-Zeichen.
b  Bin�re Modus: Das Ende der Datei ist am letzen Byte der Datei erreicht.*/

const char *ee="(EE): "; //errors
const char *ww="(WW): "; //warnings
const char *ii="(II): "; //informations
const char *dd="(DD): "; //debug

static SDL_RWops *logfile;

cLog::cLog()
{}

int cLog::init ( )
{
	logfile = SDL_RWFromFile ( "max.log","w+t" );
	if ( logfile==NULL )
	{
		fprintf ( stderr,"Couldn't open max.log\n" );
		return ( 1 );
	}
	return 0;
}

//Send str to logfile and add error/warning tag
//TYPEs:
// 1 		== warning 	(WW):
// 2 		== error	(EE):
// 3		== debug	(DD):
// else		== information	(II):
int cLog::write ( char *str, int TYPE )
{
	switch(TYPE)
	{
		case 1 : SDL_RWwrite ( logfile,ww,1,6 ); break;
		case 2 : SDL_RWwrite ( logfile,ee,1,6 ); break;
		case 3 : SDL_RWwrite ( logfile,dd,1,6 ); break;
		default : SDL_RWwrite ( logfile,ii,1,6 );
	
	}
	return writeMessage( str );
}

// noTYPE	== information 	(II):
int cLog::write ( char *str )
{
	SDL_RWwrite ( logfile,ii,1,6 );
	return writeMessage( str );
}

int cLog::writeMessage( char *str )
{
	int wrote;
	wrote=SDL_RWwrite ( logfile,str,1,strlen ( str ) );

	if ( wrote<0 )
	{
		fprintf ( stderr,"Couldn't write to max.log\n" );
		return ( 2 );
	}

	//fprintf ( stderr,"Wrote %d 1-byte blocks\n",wrote );
	return ( 0 );
}

//Am Programmende oder bei Programmabbruch ausf�hren
int cLog::close()
{
	//Die Funktion SDL_RWclose liefert aktuell immer 0 zur�ck, auf Fehler wird intern noch nicht gepr�ft (SDL 1.2.9).
	SDL_RWclose ( logfile );

}
